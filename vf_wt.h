//
// vf_wt.h
//
// Copyright (C) 1999 Sam Roberts
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 1, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  I can be contacted as sroberts@uniserve.com, or sam@cogent.ca.
//
// $Log$
// Revision 1.2  1999/09/23 01:39:22  sam
// first cut at templatization running ok
//
// Revision 1.1  1999/09/19 22:24:47  sam
// Initial revision
//

#ifndef VF_WT_H
#define VF_WT_H

#include <errno.h>
#include <sys/types.h>
#include <iostream.h>

#include <wcvector.h>

#include "vf_fifo.h"
#include "vf_log.h"
#include "vf_os.h"
#include "vf_ptr.h"

// inline helpers for binary i/o to iostreams

inline int ioread(istream& is, void* buf, int nbytes)
{
	char*	next = (char*) buf;
	while(nbytes-- > 0) {
		char	ch = is.get();
		if(!is.good()) {
			break;
		}
		*next++ = ch;
	}

	if(is.bad()) { errno = EIO; return -1; }
	if(is.fail()) { errno = EINVAL; return -1; }

	return next - (char*)buf;
}

// Message types and definitions

#define	VF_WORKTEAM_MSG		0xf101	// 0x0000 -> 0x0f00 are QSSL reserved

#define WTMSG_REQUEST	0x0001
#define WTMSG_DATA		0x0002
#define WTMSG_COMPLETE	0x0003

template <class Request>
struct wtmsg_request {
	void Fill(int rid_, const Request& request_) {
		type	= VF_WORKTEAM_MSG;
		subtype	= WTMSG_REQUEST;
		rid		= rid_;
		request	= request_;
	}

	msg_t	type;
	msg_t	subtype;

	int		rid;

	Request	request;
};

struct wtmsg_data {
	void Fill(int rid_, int length_) {
		type	= VF_WORKTEAM_MSG;
		subtype	= WTMSG_DATA;
		rid		= rid_;
		length	= length_;
	}

	msg_t	type;
	msg_t	subtype;

	int		rid;

	int		length;
	int		data[1];
};

struct wtmsg_complete {
	void Fill(int rid_, int cause_) {
		type	= VF_WORKTEAM_MSG;
		subtype	= WTMSG_COMPLETE;
		rid		= rid_;
		cause	= cause_;
	}

	msg_t	type;
	msg_t	subtype;

	int		rid;

	int cause; // >= 0, exited; < 0, signaled
};

template <class Request, class Info, class Task, class Complete>
class VFWorkTeam
{
public:
	static VFWorkTeam* Create(Complete* mgr, int maxthreads);

	int Push(const Request& request, const Info& info, iostream* data);

	int Service(pid_t pid);

private:
	VFWorkTeam(Complete* mgr, int maxthreads);

	VFWorkTeam*	Start();

	void	DoPending();

	struct WorkRequest : public VFFifo<WorkRequest>::Link {
		WorkRequest(const Request& r, int rid, const Info& info, iostream* data) :
			request(r), rid(rid), info(info), data(data) {}
		Request		request;
		int			rid;
		iostream*	data;
		Info		info;
	};

	class RequestQueue
	{
	public:
		RequestQueue() : requested_(0) {}
		int	Pending() const { return fifo_.Size(); }
		int	Active() const { return requested_ - Pending(); }

		int Push(const Request& request, const Info& info, iostream* data) {
			// find first unused request id
			WorkRequest* wr = 0;
			int rid = -1;

			for(int i = 0; rid == -1; i++)
			{
				if(requests_[i] == 0) { rid = i; break; } // done

				// when a vector resize fails due to lack of memory, the last
				// item in the vector is returned, detect this
				if(wr == requests_[i]) { break; }

				wr = requests_[i];
			}

			if(rid == -1) {
				VFLog(1, "RequestQueue::Push() vector resize failed!");
				return ENOMEM;
			}

			wr = new WorkRequest(request, rid, info, data);

			if(!wr) { return errno; }

			requests_[rid] = wr;

			assert(requests_[rid] == wr);

			fifo_.Push(wr); // can't fail, even from no memory

			requested_++;

			return EOK;
		}
		WorkRequest* Next() {
			WorkRequest* wr = fifo_.Pop();
			return wr;
		}
		WorkRequest* Peek(int rid) {
			assert(rid >= 0 || rid < requests_.length());
			return requests_[rid];
		}
		WorkRequest* Pop(int rid) {
			WorkRequest* wr = Peek(rid);
			if(wr) {
				requested_--;
				requests_[rid] = 0;
			}
			return wr;
		}

	private:
		int	requested_;;

		VFFifo<WorkRequest>	fifo_;
		WCValVector< VFPointer<WorkRequest> > requests_; // index is request id
	};

	RequestQueue	rq_;

	Complete* complete_;
	pid_t	leader_;
	int		maxthreads_;
};

//
//
//

class VFTaskDataHandle // Used by the client's Task
{
public:
	virtual int Data(const void* data, size_t length) = 0;
};

//
// VFTeamLead: manages the work team, running as the lead thread.
//

template <class Request, class Task>
class VFTeamLead
{
public:
	static int Start();

private:
	VFTeamLead();

	int		Run();
	void	HandleDeath();
	int		HandleRequest(pid_t pid, const wtmsg_request<Request>& req);

	class Active
	{
	public:
		Active(pid_t client) : client_(client), count_(0) {}

		int Count() { return count_; }
		int	Start(int rid, const Request& request);
		int Complete(const VFExitStatus& status);

	private:
		// template <class Task>
		class Worker : public VFTaskDataHandle
		{
		public:
			Worker(int rid, pid_t client) :
				rid_(rid), pid_(-1), client_(client) {}

			// used by Active
			int		Start(const Request& request);
			pid_t	Pid() const { return pid_; }
			pid_t	Rid() const { return rid_; }
			void	Complete(const VFExitStatus& status);

			// used by Task
			int	Data(const void* data, size_t length);

		private:
			void SendComplete(int cause);

			int		rid_;
			pid_t	pid_;
			pid_t	client_;

			Task	task_;
		};

		pid_t	client_;
		int		count_;
		WCValVector< VFPointer<Worker> >	workers_;
	};

	Active	active_;

	pid_t	client_;

	static pid_t	sigproxy_;
	static void		SigHandler(int signo);

	friend class Active;
};

#endif

