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

VFWorkTeam* VFWorkTeam::Create(VFPop* pop, int maxthreads)
{
	VFWorkTeam* wt = new VFWorkTeam(pop, maxthreads);

	if(!wt) { return 0; }

	return wt->Start();
}

// int Push(Request r, Info i, iostream* data = 0);
int VFWorkTeam::Push(int msgid, VFPopFile* file, iostream* data)
{
	if(!file || !data) {
		return EINVAL;
	}

	int e = rq_.Push(msgid, file, data);

	DoPending();

	return e;
}

void VFWorkTeam::DoPending()
{
	VFLog(3, "VFTeamLead::DoPending() active %d max %d pending %d",
		rq_.Active(), maxthreads_, rq_.Pending());

	if(rq_.Active() >= maxthreads_) { return; }

	WorkRequest* wr = rq_.Next();

	if(!wr) { return; }

	wtmsg_request msg; msg.Fill(wr->rid, wr->msgid);
	msg_t status;

	if(Send(leader_, &msg, &status, sizeof(msg), sizeof(status)) == -1) {
		status = errno;
	}
	if(status != EOK) {
		assert(rq_.Peek(wr->rid) == wr);
		rq_.Pop(wr->rid);
		pop_->Complete(status, wr->file, wr->data);
		delete wr;
	}
}

int VFWorkTeam::Service(pid_t pid)
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

		pop_->Complete(msg.complete.cause, wr->file, wr->data);

		delete wr;

		DoPending();

		return EOK;
		}

	default:
		return ENOSYS;
	}
}

VFWorkTeam::VFWorkTeam(VFPop* pop, int maxthreads) :
	pop_		(pop),
	leader_		(-1),
	maxthreads_	(maxthreads)
{
}

VFWorkTeam* VFWorkTeam::Start()
{
	switch((leader_ = fork()))
	{
	case -1:
		delete this;
		return 0;

	case 0:
		exit(VFTeamLead::Start());
		break;

	default:
		break;
	}
	return this;
}

//
// VFTeamLead
//

pid_t VFTeamLead::sigproxy_ = -1;

int VFTeamLead::Start()
{
	static VFTeamLead tl;

	VFLog(3, "VFTeamLead::Start() leader is pid %d\n", getpid());

	sigproxy_ = qnx_proxy_attach(0, 0, 0, -1);

	if(sigproxy_ == -1) {
		VFLog(1, "VFTeamLead::Start() proxy attach failed: [%d] %s", VFERR(errno));
		return errno;
	}

	signal(SIGCHLD, &SigHandler);

	return tl.Run();
}

VFTeamLead::VFTeamLead() :
	active_		(getppid()),
	client_		(getppid())
{
}

int VFTeamLead::Run()
{
	wtmsg_request	msg;
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

void VFTeamLead::HandleDeath()
{
	VFExitStatus status;

	if(status.Wait() == -1) {
		VFLog(1, "VFTeamLead::HandleDeath() wait failed: [%d] %s", VFERR(errno));
		return;
	}

	active_.Complete(status);
}

int VFTeamLead::HandleRequest(pid_t pid, const wtmsg_request& req)
{
	VFLog(3, "VFTeamLead::HandleRequest() pid %d msgid %d rid %d",
		pid, req.msg, req.rid);

	if(pid != client_) {
		VFLog(1, "VFTeamLead::HandleRequest() request from %d not allowed", pid);
		return EACCES;	// only our client can make requests!
	}
	return active_.Start(req.rid, req.msg);
}

void VFTeamLead::SigHandler(int signo)
{
	VFLog(3, "VFTeamLead::SigHandler() signo %d", signo);

	if(signo == SIGCHLD) {
		Trigger(sigproxy_);
	}
}

//
// VFTeamLead::Active
//

int VFTeamLead::Active::Start(int rid, int msgid)
{
    assert(workers_[rid] == 0);

    Worker* w = new Worker(rid, client_);

    if(!w) { return ENOMEM; }

    int e = w->Start(msgid);

    if(e != EOK) { delete w; return e; }

    count_++;

    workers_[rid] = w;

    assert(workers_[w->Rid()] == w);

    return EOK;
}

int VFTeamLead::Active::Complete(const VFExitStatus& status)
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

int VFTeamLead::Active::Worker::Start(int msgid)
{
	pid_ = fork();
	switch(pid_)
	{
	case -1:
		return errno;
	case 0: {
		VFLog(3, "VFTeamLead::Active::Worker::Start() starting task");

		int e = DoTask(this, msgid);

		VFLog(3, "VFTeamLead::Active::Worker::DoTask() returned [%d] %s", VFERR(e));

		SendComplete(e);
		}
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

void VFTeamLead::Active::Worker::Complete(const VFExitStatus& status)
{
	if(!status.Ok()) {
		// complete transaction for our errant child: we can't send to client,
		// that would cause deadlock, instead fork() and send

		VFLog(1, "VFTeamLead::Active::Worker::Complete() pid %d rid %d failed: %s with [%d] %s",
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

int VFTeamLead::Active::Worker::Data(const void* data, int length)
{
	msg_t		status;
	wtmsg_data	msg; msg.Fill(rid_, length);
	struct _mxfer_entry smx[2];
	struct _mxfer_entry rmx[2];

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

int VFTeamLead::Active::Worker::DoTask(Worker* dh, int msgid)
{
	pop3	pop;
	char*	host_ = "localhost";
	char*	user_ = "guest";
	char*	pass_ = "guest";

	// if(msgid == 2) raise(SIGUSR1); // simulate failure!

	VFLog(3, "VFTeamLead::Active::Worker::Start() connecting...");

    int fail = pop->connect(host_);

    if(fail) {
        VFLog(0, "connect to %s failed: [%d] %s\n",
            (const char*)host_, fail, strerror(fail));
        return ECONNREFUSED;
    }

     if(!pop->checkconnect()) {
        VFLog(0, "connect to %s failed: %s",
            (const char*)host_, pop->response() + strlen("-ERR "));
        return ECONNREFUSED;
    }

    if(!pop->user(user_)) {
        VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
        return EACCES;
    }

    if(!pop->pass(pass_)) {
        VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
        return EACCES;
    }

	VFLog(3, "VFTeamLead::Active::Worker::Start() retrieving...");

    istream& is = pop->retr(msgid);

    if(!is.good()) { return EIO; }

    static char buf[16 * 1024];

	while(!is.eof())
	{
		int n = ioread(is, buf, sizeof(buf));

		if(n == -1) {
			return errno;
		}

		int e = dh->Data(buf, n);

		if(e != EOK) {
			return e;
		}
	}

    pop->quit();

    return EOK;
}

void VFTeamLead::Active::Worker::SendComplete(int cause)
{
	wtmsg_complete msg; msg.Fill(Rid(), cause);

	if(Send(client_, &msg, 0, sizeof(msg), 0) == -1) {
		VFLog(0, "VFTeamLead::Active::Worker::SendComplete() failed: [%d] %s",
			errno, strerror(errno));
		exit(errno);
	}

	exit(EOK);
}

