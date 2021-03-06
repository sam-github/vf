//
// vf_pop.cc
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
// Revision 1.15  1999/11/25 04:20:19  sam
// can now be called as mount_pop by the mount utility
//
// Revision 1.14  1999/10/04 03:34:21  sam
// forking is now done by the manager
//
// Revision 1.13  1999/09/27 00:17:41  sam
// using new list() command to read the whole message list, rather than
// piece by piece.
//
// Revision 1.12  1999/09/26 23:40:21  sam
// moved the popfile into it's own module
//
// Revision 1.11  1999/09/26 22:50:27  sam
// reorganized the templatization, checking in prior to last cleanup
//
// Revision 1.10  1999/09/23 01:39:22  sam
// first cut at templatization running ok
//
// Revision 1.9  1999/09/19 22:24:47  sam
// implemented threading with a work team
//
// Revision 1.8  1999/08/09 15:19:38  sam
// Multi-threaded downloads! Pop servers usually lock the maildrop, so concurrent
// downloads are not possible, but stats and reads of already downloaded mail
// do not block on an ongoing download!
//
// Revision 1.7  1999/08/09 09:27:43  sam
// started to multi-thread, but fw didn't allow blocking, checking in to fix fw
//
// Revision 1.6  1999/07/21 16:00:17  sam
// the symlink extension wasn't working, commented it out
//
// Revision 1.5  1999/07/19 15:23:20  sam
// can make aribtrary info show up in the link target, now just need to get
// that info...
//
// Revision 1.4  1999/06/21 12:48:02  sam
// caches mail on disk instead of file (by default), and reports a sys version
//
// Revision 1.3  1999/06/20 17:21:53  sam
// now conduct transactions with server to prevent timeouts
//
// Revision 1.2  1999/06/18 14:56:03  sam
// now download file and set stat correctly
//
// Revision 1.1  1999/05/17 04:37:40  sam
// Initial revision
//

#include <unistd.h>
#include <unix.h>
#include <stdlib.h>
#include <stdio.h>
#include <strstream.h>
#include <fstream.h>

#include <sys/kernel.h>
#include <sys/proxy.h>
#include <sys/wait.h>

#include <vf_dir.h>
#include <vf_file.h>
#include <vf_log.h>
#include <vf_os.h>

#include "vf_pop.h"
#include "vf_popfile.h"

#include "url.h"

// include definitions of the workteam functions
#include <vf_wt.cc>

const char* ItoA(int i)
{
	static char buffer[8 * sizeof(int) + 1];
	return itoa(i, buffer, 10);
}

void PopFail(const char* cmd, pop3& pop) 
{
	VFLog(0, "%s failed: %s", cmd, pop->response() + strlen("-ERR "));
	exit(1);
}

//
// PopTask
//

int PopTask::operator () (const PopRequest& request, VFDataIfx& dh)
{
	VFLog(3, "VFPopTask::op() connecting...");

	pop3	pop;

    int fail = pop->connect(host);

    if(fail) {
        VFLog(0, "connect to %s failed: [%d] %s\n",
            (const char*)host, fail, strerror(fail));
        return ECONNREFUSED;
    }

     if(!pop->checkconnect()) {
        VFLog(0, "connect to %s failed: %s",
            (const char*)host, pop->response() + strlen("-ERR "));
        return ECONNREFUSED;
    }

    if(!pop->user(user)) {
        VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
        return EACCES;
    }

    if(!pop->pass(pass)) {
        VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
        return EACCES;
    }

	VFLog(3, "PopTask::op() retrieving...");

    istream& is = pop->retr(request.msgid);

    if(!is.good()) { return EIO; }

    static char buf[16 * 1024];

	while(!is.eof())
	{
		int n = ioread(is, buf, sizeof(buf));

		if(n == -1) {
			return errno;
		}

		int e = dh.Data(buf, n);

		if(e != EOK) {
			return e;
		}
	}

    pop->quit();

    return EOK;
}

//
// VFPop
//

VFPop::VFPop(const char* host, const char* user, const char* pass, int inmem) :
	VFManager	(VFVersion("vf_pop", 1.2)),
	root_	(getuid(), getgid(), 0500),
	host_	(host),
	user_	(user),
	pass_	(pass),
	inmem_	(inmem)
{
	pop3 pop;

	if(!Connect(pop)) { exit(1); }

	istream& list = pop->list();
	if(!list) { PopFail("list", pop); }

	int msg;
	int size;
	while(list >> msg >> size)
	{
		int e = root_.Insert(ItoA(msg), new VFPopFile(msg, *this, size));
		if(e != EOK)
		{
			VFLog(0, "insert msg %d failed: [%d] %s", msg, e, strerror(e));
			Disconnect(pop); // unnecessary, but polite
			exit(1);
		}
	}

	Disconnect(pop);
}

void VFPop::Run(const char* mount, int verbosity, int nodaemon)
{
	if(!Init(&root_, mount, verbosity)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	Start(nodaemon);

	signal(SIGCHLD, SigHandler);

	PopTask task(host_, user_, pass_);
	team_ = VFWorkTeam<PopRequest, PopInfo, PopTask>::Create(*this, task, 1);

	if(!team_) {
		VFLog(0, "work team create failed: [%d] %s\n", VFERR(errno));
		exit(1);
	}

	VFManager::Run();
}

int VFPop::Service(pid_t pid, VFMsg* msg)
{
	VFLog(4, "VFPop::Service() pid %d type %#x", pid, msg->type);

	switch(msg->type)
	{
	case VF_WORKTEAM_MSG:
		return team_->Service(pid);

	default:
		return VFManager::Service(pid, msg);
	}
}

int VFPop::Retr(int msg, VFPopFile* file)
{
	iostream* mail = Stream();

	if(!mail) { return errno; }

	PopRequest request; request.msgid = msg;

	PopInfo	info; info.file = file;

	int e = team_->Push(request, info, mail);

	if(e == EOK) {
		return -1; // request blocked, no status yet
	}

	return e;	// else an error number
}

void VFPop::Complete(int status, const PopInfo& info, iostream* data)
{
	VFLog(4, "VFPop::Complete() status %d", status);

	assert(info.file);
	assert(data);

	if(status != EOK) { delete data; data = 0; }

	if(status < 0) { status = EINTR; }

	info.file->Return(status, data);
}

iostream* VFPop::Stream() const
{
	iostream* s = 0;

	if(inmem_)
	{
		s = new strstream;
	}
	else
	{
		const char* n = tmpnam(0);
		s = new fstream(n, ios::in|ios::out, 0600);
		unlink(n);
	}

	if(!s)
	{
		VFLog(0, "out of memory");
		errno = ENOMEM;
	}

	if(!s->good())
	{
		delete s;
		s = 0;
	}

	return s;
}

int VFPop::Connect(pop3& pop)
{
	int fail = pop->connect(host_);

	if(fail) {
		VFLog(0, "connect to %s failed: [%d] %s\n",
			(const char*)host_, fail, strerror(fail));
		return 0;
	}

	 if(!pop->checkconnect()) {
		VFLog(0, "connect to %s failed: %s",
			(const char*)host_, pop->response() + strlen("-ERR "));
		return 0;
	}

	if(!pop->user(user_)) {
		VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
		return 0;
	}

	if(!pop->pass(pass_)) {
		VFLog(0, "login failed: %s", pop->response() + strlen("-ERR "));
		return 0;
	}

	return 1;
}

int VFPop::Disconnect(pop3& pop)
{
	if(!pop->quit()) {
		VFLog(0, "quit failed: %s", pop->response() + strlen("-ERR "));
		return 0;
	}
	return 1;
}

static void VFPop::SigHandler(int signo)
{
	VFLog(2, "VFPop::SigHandler() got signo %d", signo);

	switch(signo)
	{
	case SIGCHLD: {
		int stat;
		pid_t child = wait(&stat);
		if(child == -1) {
			VFLog(0, "wait failed: [%d] %s", errno, strerror(errno));
			exit(1);
		}

		VFExitStatus estat = stat;

		VFLog(0, "VFPop::SigHandler() child %d failed: %s with [%d] %s",
			child, estat.Reason(), estat.Number(), estat.Name());

		exit(1);
	} break;

	default:
		break;
	}
}

//
// server
//

#ifdef __USAGE
%C - mounts a pop3 virtual filesystem

usage: vf_pop [-hvd] user[:passwd]@hostname [vf]
    -h   Print this message and exit.
    -v   Increase the verbosity level, default is 0.
    -d   Don't become a daemon, default is to fork into the background after
         prompting for a password (if necessary).
    -m   Buffer read mail messages in memory, default is in a temp file
         opened in /tmp with perms 0600 and immediately unlinked after
         being opened.

Mounts the specified pop3 account as a virtual filesystem at 'vf', the
default is "./user@hostname".

The 'passwd' parameter is optional, probably shouldn't be specified on
the command line (for security reasons), and will be prompted for if not
supplied.

Doing a 'rmdir' or 'umount' on the path 'vf' will cause vf_pop to exit.
#endif

int		vOpt		= 0;
char*	pathOpt		= 0;
char*	accountOpt	= 0;
char*	userOpt		= 0;
char*	passOpt		= 0;
char*	hostOpt		= 0;
int		inmemOpt	= 0;
int		dOpt		= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvdmt:")) != -1; ) {
		switch(c) {
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			VFLevel("vf_pop3", vOpt);
			break;

		case 'd':
			dOpt = 1;
			break;

		case 'm':
			inmemOpt = 1;
			break;

		case 't':	// ignore this option, its passed by 'mount'
			break;

		default:
			exit(1);
		}
	}

	if(!(accountOpt = argv[optind++])) {
		fprintf(stderr, "no account specified (try -h for help)!\n");
		exit(1);
	}

	if(!UrlParseAccount(accountOpt, &userOpt, &passOpt, &hostOpt)) {
		fprintf(stderr, "account specification malformed!\n");
		exit(1);
	}

	if(!passOpt) {
		passOpt = getpass("password: ");
	}

	assert(passOpt);

	if(argv[optind]) {
		pathOpt = argv[optind];
	} else {
		ostrstream os;

		os << userOpt << "@" << hostOpt << '\0';

		pathOpt = os.str();
	}

	return 0;
}

void main(int argc, char* argv[])
{
	GetOpts(argc, argv);

	VFLevel("vf_pop", vOpt);

	VFPop vfpop(hostOpt, userOpt, passOpt, inmemOpt);

	vfpop.Run(pathOpt, vOpt, dOpt);
}

