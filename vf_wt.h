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
// Revision 1.3  1999/09/26 22:50:27  sam
// reorganized the templatization, checking in prior to last cleanup
//
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

//
// ioread(): an inline helper for binary i/o from an iostream
//
// There's oddities and ambiguities in the definition of binary i/o
// methods for iostreams, involving what happens when a request for
// a chunk can only be partially satisfied. The stream considers this
// a failure of sorts. This function maps all this to return types
// that are similar to read(3). It should be no more or less efficient
// than istream::read(void*,size_t). Reporting of some errors is put
// off until the next time the stream is read.
//
// arguments:
//		is		- a valid istream
//		data	-
// returns:
//		the number of bytes read, or -1 and sets errno.
//
// errors:
//		EIO		- the istream was bad
//		EFAULT	- nbytes is > 0, but buf is NULL
//		EINVAL	- the istream was failed

inline int ioread(istream& is, void* data, int nbytes)
{
	if(nbytes && !data) { errno = EFAULT; return -1; }

	if(is.bad()) { errno = EIO; return -1; }

	// If the is in the fail state, any attempts to get() from the
	// stream will fail until the fail state is reset, so don't bother.
	if(is.fail()) { errno = EINVAL; return -1; }

	char*	next = (char*) data;
	while(nbytes-- > 0) {
		char ch = is.get();
		if(!is.good()) {
			break;
		}
		*next++ = ch;
	}

	nbytes = next - (char*) data;

	// If no bytes were read, return an error if one occurred, otherwise
	// we'll see the bad state on the next call.
	if(nbytes == 0 && is.bad()) { errno = EBADF; return -1; }

	return nbytes; // If nbytes is 0, that's the traditional EOF notification.
}

//
// VFCompleteIfx:
//	Called back by VFWorkTeam to indicate completetion of a request.
//

template <class Info>
class VFCompleteIfx
{
public:
	virtual void Complete(int status, const Info& info, iostream* data) = 0;
};

//
// VFDataIfx:
//	Passed to Task, to be used to push file data to the main task.
//

class VFDataIfx
{
public:
	virtual int Data(const void* data, size_t size) = 0;
};

//
// VFWorkTeam
//

template <class Request, class Info, class Task>
class VFWorkTeam
{
public:
	static VFWorkTeam* Create(VFCompleteIfx<Info>& complete,
				Task& task, int maxthreads);

	int Push	(const Request& request, const Info& info, iostream* data);
	int Service	(pid_t pid);

private:
	VFWorkTeam(VFCompleteIfx<Info>& complete, int maxthreads);

	VFWorkTeam* Start(Task& task);

	void DoPending();

	struct WorkRequest : public VFFifo<WorkRequest>::Link
	{
		WorkRequest(const Request& r, int rid,
			const Info& info, iostream* data) :
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

		int Push(const Request& request, const Info& info, iostream* data);
		WorkRequest* Next() { return fifo_.Pop(); }
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

	VFCompleteIfx<Info>& complete_;
	pid_t	leader_;
	int		maxthreads_;
};

//
// VFTeamLead: manages the work team, running as the lead thread.
//

template <class Request, class Task>
class VFTeamLead
{
public:
	static int Start(Task& task);

private:
	VFTeamLead(Task& task);

	int		Run();
	void	HandleDeath();
	int		HandleRequest(pid_t pid, const wtmsg_request<Request>& req);

	class Active
	{
	public:
		Active(pid_t client, Task& task) :
			client_(client), count_(0), task_(task) {}

		int Count() { return count_; }
		int	Start(int rid, const Request& request);
		int Complete(const VFExitStatus& status);

	private:
		class Worker : public VFDataIfx
		{
		public:
			Worker(int rid, pid_t client) :
				rid_(rid), pid_(-1), client_(client) {}

			// used by Active
			int		Start(const Request& request, Task& task);
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
		};

		pid_t	client_;
		int		count_;
		Task&	task_;
		WCValVector< VFPointer<Worker> >	workers_;
	};

	Active	active_;

	pid_t	client_;

	static pid_t	sigproxy_;
	static void		SigHandler(int signo);

	friend class Active;
};

//
// Message types and definitions
//

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


#endif

