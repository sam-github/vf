//
// vf_wt.cc
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/kernel.h>
#include <sys/proxy.h>

#include "vf_wt.h"
#include "vf_log.h"

#include "vf_pop.h"

template <class Request, class Info, class Task>
VFWorkTeam<Request,Info,Task>* VFWorkTeam<Request,Info,Task>::
Create(VFCompleteIfx<Info>& pop, Task& task, int maxthreads)
{
	VFWorkTeam* wt = new VFWorkTeam(pop, maxthreads);

	if(!wt) { return 0; }

	return wt->Start(task);
}

template <class Request, class Info, class Task>
int VFWorkTeam<Request,Info,Task>::
Push(const Request& request, const Info& info, iostream* data)
{
	if(!data) {
		return EINVAL;
	}

	int e = rq_.Push(request, info, data);

	DoPending();

	return e;
}

template <class Request, class Info, class Task>
int VFWorkTeam<Request,Info,Task>::
Service(pid_t pid)
{
	union {
		struct {
			msg_t	type;
			msg_t	subtype;
		};

		wtmsg_data		data;
		wtmsg_complete	complete;
	} msg;

	if(Readmsg(pid, 0, &msg, sizeof(msg)) == -1) {
		return errno;
	}

	VFLog(4, "VFWorkTeam::Service() pid %d type %#x sub %#x",
		pid, msg.type, msg.subtype);

	assert(msg.type == VF_WORKTEAM_MSG);

	switch(msg.subtype)
	{
	case WTMSG_DATA: {
		WorkRequest* wr = rq_.Peek(msg.data.rid);
		if(!wr) { return EINVAL; }

		char data[4096];
		int offset = sizeof(msg.data) - sizeof(msg.data.data);
		int	n = 0;
		while(n < msg.data.length)
		{
			int s = sizeof(data);
			int e = Readmsg(pid, offset + n, data, sizeof(data));
			if(e == -1) { return errno; }

			if(e == 0) { // msg.data.length must be wrong?
				VFLog(1, "VFWorkTeam::Service() Readmsg failed: read 0 bytes");
				 return EINVAL;
			}

			if(!wr->data->write(data, e)) {
				VFLog(1, "VFWorkTeam::Service() write failed: ios %#x",
					wr->data->rdstate());
				return EIO;
			}

			n += e;
		}

		return EOK;
		}

	case WTMSG_COMPLETE: {
		WorkRequest* wr = rq_.Pop(msg.complete.rid);
		if(!wr) { return EINVAL; }

		complete_.Complete(msg.complete.cause, wr->info, wr->data);

		delete wr;

		DoPending();

		return EOK;
		}

	default:
		return ENOSYS;
	}
}

template <class Request, class Info, class Task>
VFWorkTeam<Request,Info,Task>::
VFWorkTeam(VFCompleteIfx<Info>& complete, int maxthreads) :
	complete_	(complete),
	leader_		(-1),
	maxthreads_	(maxthreads)
{
}

template <class Request, class Info, class Task>
VFWorkTeam<Request,Info,Task>* VFWorkTeam<Request,Info,Task>::
Start(Task& task)
{
	switch((leader_ = fork()))
	{
	case -1:
		delete this;
		return 0;

	case 0:
		exit( VFTeamLead<Request,Task>::Start(task) );
		break;

	default:
		break;
	}
	return this;
}

template <class Request, class Info, class Task>
void VFWorkTeam<Request,Info,Task>::
DoPending()
{
	VFLog(3, "VFTeamLead::DoPending() active %d max %d pending %d",
		rq_.Active(), maxthreads_, rq_.Pending());

	if(rq_.Active() >= maxthreads_) { return; }

	WorkRequest* wr = rq_.Next();

	if(!wr) { return; }

	wtmsg_request<Request> msg; msg.Fill(wr->rid, wr->request);
	msg_t status;

	if(Send(leader_, &msg, &status, sizeof(msg), sizeof(status)) == -1) {
		status = errno;
	}
	if(status != EOK) {
		assert(rq_.Peek(wr->rid) == wr);
		rq_.Pop(wr->rid);
		complete_.Complete(status, wr->info, wr->data);
		delete wr;
	}
}

template <class Request, class Info, class Task>
int VFWorkTeam<Request,Info,Task>::RequestQueue::
Push(const Request& request, const Info& info, iostream* data)
{
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

//
// VFTeamLead
//

template <class Request, class Task>
pid_t VFTeamLead<Request,Task>::sigproxy_ = -1;

template <class Request, class Task>
int VFTeamLead<Request,Task>::
Start(Task& task)
{
	static VFTeamLead tl(task);

	VFLog(3, "VFTeamLead::Start() leader is pid %d\n", getpid());

	sigproxy_ = qnx_proxy_attach(0, 0, 0, -1);

	if(sigproxy_ == -1) {
		VFLog(1, "VFTeamLead::Start() proxy attach failed: [%d] %s", VFERR(errno));
		return errno;
	}

	signal(SIGCHLD, &SigHandler);

	return tl.Run();
}

template <class Request, class Task>
VFTeamLead<Request,Task>::
VFTeamLead(Task& task) :
	active_		(getppid(), task),
	client_		(getppid())
{
}

template <class Request, class Task>
int VFTeamLead<Request,Task>::
Run()
{
	wtmsg_request<Request>	msg;
	pid_t	pid;

	while(1)
	{
		pid = Receive(0, &msg, sizeof(msg));

		if(pid == -1) {
			if(errno != EINTR) {
				int e = errno; // in case VFLog affects errno
				VFLog(0, "VFTeamLead::Run() Receive() failed: [%d] %s", VFERR(e));
				exit(e);
			}
			continue;
		}

		if(pid == sigproxy_) {
			HandleDeath();
			continue;
		}

		msg_t status = ENOSYS;

		switch(msg.type)
		{
		case VF_WORKTEAM_MSG:
			switch(msg.subtype)
			{
			case WTMSG_REQUEST:
				status = HandleRequest(pid, msg);
				break;
			}
			break;
		}

		if(status != -1) {
			Reply(pid, &status, sizeof(status));
		}
	}
}

template <class Request, class Task>
void VFTeamLead<Request,Task>::
HandleDeath()
{
	VFExitStatus status;

	if(status.Wait() == -1) {
		VFLog(1, "VFTeamLead::HandleDeath() wait failed: [%d] %s", VFERR(errno));
		return;
	}

	active_.Complete(status);
}

template <class Request, class Task>
int VFTeamLead<Request,Task>::
HandleRequest(pid_t pid, const wtmsg_request<Request>& req)
{
	VFLog(3, "VFTeamLead::HandleRequest() pid %d rid %d", pid, req.rid);

	if(pid != client_) {
		VFLog(1, "VFTeamLead::HandleRequest() request from %d not allowed", pid);
		return EACCES;	// only our client can make requests!
	}
	return active_.Start(req.rid, req.request);
}

template <class Request, class Task>
void VFTeamLead<Request,Task>::SigHandler(int signo)
{
	VFLog(3, "VFTeamLead::SigHandler() signo %d", signo);

	if(signo == SIGCHLD) {
		Trigger(sigproxy_);
	}
}

//
// VFTeamLead::Active
//

template <class Request, class Task>
int VFTeamLead<Request,Task>::Active::
Start(int rid, const Request& request)
{
	assert(workers_[rid] == 0);

	Worker* w = new Worker(rid, client_);

	if(!w) { return ENOMEM; }

	int e = w->Start(request, task_);

	if(e != EOK) { delete w; return e; }

	count_++;

	workers_[rid] = w;

	assert(workers_[w->Rid()] == w);

	return EOK;
}

template <class Request, class Task>
int VFTeamLead<Request,Task>::Active::
Complete(const VFExitStatus& status)
{
	VFLog(3, "VFTeamLead::Active::Complete() pid %d %s with [%d] %s",
		status.Pid(), status.Reason(), status.Number(), status.Name());

	Worker* w = 0;
	for(int i = 0; i < workers_.length(); i++) {
		if(workers_[i] && workers_[i]->Pid() == status.Pid()) {
			w = workers_[i];
			workers_[i] = 0;
			break;
		}
	}
	if(!w) {
		VFLog(3, "VFTeamLead::Active::Complete() pid %d was a completer",
			status.Pid());

		if(!status.Ok()) {
			// failed to complete a request, have to exit
			exit(status.Exited() ? status.Number() : EINTR);
		}
		return EOK;
	}

	assert(w && w->Pid() == status.Pid());

	count_--;

	w->Complete(status);

	delete w;

	return EOK;
}

//
// VFTeamLead::Worker
//

template <class Request, class Task>
int VFTeamLead<Request,Task>::Active::Worker::
Start(const Request& request, Task& task)
{
	pid_ = fork();
	switch(pid_)
	{
	case -1:
		return errno;

	case 0: {
		VFLog(3, "VFTeamLead::Active::Worker::Start() starting task");

		int e = task(request, *this);

		VFLog(3, "VFTeamLead::Active::Worker::DoTask() return [%d] %s", VFERR(e));

		SendComplete(e);
	  }	break;

	default:
		break;
	}
	return EOK;
}

// When we do the completion for our child, we either succeed or exit with
// a failure code. I'd like to handle this more gracefully, but we can't report
// an error, because this is a failure during an attempt to report an error.
// Not reporting a completion to the client causes the read request on the
// I/O manager to block forever, better to fail the whole team than to have
// that occur.

template <class Request, class Task>
void VFTeamLead<Request,Task>::Active::Worker::
Complete(const VFExitStatus& status)
{
	if(!status.Ok()) {
		// complete transaction for our errant child: we can't send to client,
		// that would cause deadlock, instead fork() and send

		VFLog(1, "VFTeamLead::Active::Worker::Complete() "
			"pid %d rid %d failed: %s with [%d] %s",
			Pid(), Rid(), status.Reason(), status.Number(), status.Name());

		pid_t child = fork();

		switch(child)
		{
		case -1:
			VFLog(3, "VFTeamLead::Active::Worker()::Complete() fork failed: "
				"[%d] %s", VFERR(errno));

			exit(errno); // if we can't fork, we can't be a team leader!
			break;

		case 0: 		// notify client
			VFLog(3, "VFTeamLead::Active::Worker()::Complete() complete thread",
				getpid());
			SendComplete(status.Exited() ? status.Number() : -status.Number());
			break;

		default: {
			// Remember, waiting for our child is deadlock, the same as if we
			// sent ourselves. The wait will occur when the child exits,
			// and we discover that its not a worker because our search for
			// its pid fails.

			} break;
		}
	}

	return;
}

template <class Request, class Task>
int VFTeamLead<Request,Task>::Active::Worker::
Data(const void* data, size_t length)
{
// This should be a while loop, in case length is greater than can be
// sent atomically.
	msg_t		status;
	wtmsg_data	msg; msg.Fill(rid_, length);
	struct _mxfer_entry smx[2];
	struct _mxfer_entry rmx[1];

	_setmx(smx + 0, &msg,	sizeof(msg) - sizeof(msg.data));
	_setmx(smx + 1, data,	length);
	_setmx(rmx + 0, &status, sizeof(status));

	VFLog(4, "::Data() length %d", length);

	int e = Sendmx(client_, 2, 1, smx, rmx);;
	if(e == -1) {
		VFLog(1, "VFTeamLead::Active::Worker::Data() Send failed: [%d] %s",
			errno, strerror(errno));
		return errno;
	}
	if(status != EOK) {
		VFLog(1, "VFTeamLead::Active::Worker::Data() data msg failed: [%d] %s",
			status, strerror(status));
		return status;
	}
	return EOK;
}

template <class Request, class Task>
void VFTeamLead<Request,Task>::Active::Worker::
SendComplete(int cause)
{
	wtmsg_complete msg; msg.Fill(Rid(), cause);

	if(Send(client_, &msg, 0, sizeof(msg), 0) == -1) {
		VFLog(0, "VFTeamLead::Active::Worker::SendComplete() failed: [%d] %s",
			errno, strerror(errno));
		exit(errno);
	}

	exit(EOK);
}

