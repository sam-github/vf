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

#include "url.h"

// include definitions of the workteam functions
#include "vf_wt.cc"

const char* ItoA(int i)
{
	static char buffer[8 * sizeof(int) + 1];
	return itoa(i, buffer, 10);
}

PopFail(const char* cmd, pop3& pop) 
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
// VFPopFile
//

VFPopFile::VFPopFile(int msg, VFPop& pop, int size) :
	VFFileEntity(getuid(), getgid(), 0400),
	msg_(msg), pop_(pop), size_(size), data_(0), description_(0)
{
	info_.size = size_;

	ostrstream os;
	os << "TBD" << '\0';
	description_ = os.str();
}

VFPopFile::~VFPopFile()
{
	delete data_;
	delete description_;
}

int VFPopFile::Stat(pid_t pid, const String* path, int lstat)
{
    VFLog(2, "VFPopFile::Stat() pid %d path \"%s\" lstat %d",
		pid, (const char*) path, lstat);

	return VFEntity::ReplyInfo(pid);

#if 0
// not working yet!

	reply->status	= EOK;
	reply->zero		= 0;
	reply->stat		= stat_;

	/* if it's an lstat(), claim we're a link */
	if(req->mode == S_IFLNK) {
		reply->stat.st_mode = (reply->stat.st_mode & ~S_IFMT) | S_IFREG;
	}

	return -1;
#endif
}

int VFPopFile::ReadLink(pid_t pid, const String& path)
{
	VFLog(2, "VFSymLinkEntity::ReadLink() pid %d path '%s' description '%s'",
		pid, (const char*) path, description_);

	return EINVAL;

#if 0
	// not working yet!

	reply->status = EOK;
	strcpy(reply->path, description_);

	return sizeof(*reply) + strlen(description_);
#endif
}

int VFPopFile::Read(pid_t pid, size_t nbytes, off_t* offset)
{
	VFLog(2, "VFPopFile::Read() pid %d nbytes %ld offset %ld size %ld",
		pid, nbytes, *offset, size_);

	if(data_)			// data retrieved, reply immediately
	{
		return ReplyRead(pid, nbytes, offset);
	}
	if(readq_.Size())	// data has been requested, block
	{
		return QueueRead(pid, nbytes, offset);
	}

	int e = pop_.Retr(msg_, this);

	switch(e)
	{
	case EOK:	// retr completed immediately
		return ReplyRead(pid, nbytes, offset);

	case -1:	// retr queued, block
		return QueueRead(pid, nbytes, offset);

	default:	// error occurred
		return e;
	}
}

void VFPopFile::Return(int status, istream* data)
{
	VFLog(2, "VFFile::Return() status %d readq size %d", status, readq_.Size());

	// worked and there is data, or failed and there is no data!
	assert((status == EOK && data) || (status != EOK && !data));

	assert(data_ == 0); // no current data!

	data_ = data;

	while(readq_.Size())
	{
		ReadRequest* rr = readq_.Pop();
		assert(rr);

		if(status != EOK)
		{
			ReplyStatus(rr->pid, status);
		}
		else
		{
			int e = ReplyRead(rr->pid, rr->nbytes, rr->offset);

			if(e != -1) {
				ReplyStatus(rr->pid, e);
			}
		}

		delete rr;
	}
}

int VFPopFile::QueueRead(pid_t pid, int nbytes, off_t* offset)
{
	ReadRequest* rr = new ReadRequest(pid, nbytes, offset);

	if(!rr) { return ENOMEM; }

	readq_.Push(rr);

	return -1;
}

int VFPopFile::ReplyRead(pid_t pid, int nbytes, off_t* offset)
{
	VFLog(3, "VFPopFile::ReplyRead() pid %d nbytes %d offset %d",
		pid, nbytes, *offset);

	assert(data_);

	// adjust nbytes if the read would be past the end of file
	if(*offset > size_) {
			nbytes = 0;
	} else if(*offset + nbytes > size_) {
		nbytes = size_ - *offset;
	}

	// XXX Max msg len is 65500U, we need to be able to deal with this
	char* buffer = (char*) alloca(nbytes);

	if(!buffer)
	{
		return ENOMEM;
	}

	// these will set errno if we're using a file, if its a strstream...?
	if(!data_->seekg(*offset)) {
		VFLog(1, "ReplyRead() seekg() iostate %#x", data_->rdstate());
		return EIO;
	}
	if(!data_->read(buffer, nbytes)) {
		VFLog(1, "ReplyRead() read() iostate %#x gcount %d",
			data_->rdstate(), data_->gcount());
		return EIO;
	}

	nbytes = data_->gcount(); // might not be nbytes...

	struct _io_read_reply reply;
	struct _mxfer_entry mx[2];

	reply.status = EOK;
	reply.nbytes = nbytes;
	reply.zero = 0;

	_setmx(mx + 0, &reply, sizeof(reply) - sizeof(reply.data));
	_setmx(mx + 1, buffer, nbytes);

	if(Replymx(pid, 2, mx) != -1)
	{
		*offset += nbytes;
	}

	return -1;
}

//
// VFPop
//

VFPop::VFPop(const char* host, const char* user, const char* pass, int inmem, int sync) :
	VFManager	(VFVersion("vf_pop", 1.2)),
	root_	(getuid(), getgid(), 0500),
	host_	(host),
	user_	(user),
	pass_	(pass),
	inmem_	(inmem),
	sync_	(sync)
{
	pop3 pop;

	if(!Connect(pop)) { exit(1); }

	int count;
	if(!pop->stat(&count)) { PopFail("stat", pop); }

	for(int msg = 1; msg <= count; ++msg)
	{
		int size;

		if(!pop->list(msg, &size)) { PopFail("list", pop); }

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

void VFPop::Run(const char* mount, int verbosity)
{
	if(!Init(&root_, mount, verbosity)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

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
#if 0
	if(sync_)
	{
		return SyncRetr(msg, str);
	}
#endif

	iostream* mail = Stream();

	if(!mail) { return errno; }

	PopRequest request; request.msgid = msg;

	PopInfo	info; info.file = file;

	int e = team_->Push(request, info, mail);

	if(e == EOK) {
		return -1; // request blocked, no status yet
	}

	return e;	// else an error number

#if 0
	RetrRequest* rr = new RetrRequest(msg, file, mail);

	if(!rr) { return errno; }

	retrq_.Push(rr);

	return AsyncRetr();
#endif
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

#if 0
int VFPop::SyncRetr(int msg, istream** str)
{
	iostream* mail = Stream();

	if(!mail) { return errno; }

	int e = GetMail(msg, mail);

	if(e == EOK)
	{
		*str = mail;
	}
	else
	{
		delete mail;
	}
	return e;
}
#endif

#if 0
int VFPop::AsyncRetr()
{
	VFLog(4, "VFPop::AsyncRetr() retrq size %d team_ %d",
		retrq_.Size(), child_);

	if( != -1)
	{
		return -1; // busy retrieving something
	}

	RetrRequest* rr = retrq_.Peek();

	if(!rr)
	{
		return -1; // nothing to do
	}

	switch((child_ = fork()))
	{
	case -1:
		VFLog(1, "VFPop::AsyncRetr() fork() failed: [%d] %s",
			errno, strerror(errno));
		return errno;

	case 0: {
		int e = GetMail(rr->msg, rr->mail);

		if(e != EOK) {
			VFLog(1, "VFPop::AsyncRetr() pid %d GetMail() failed: [%d] %s",
				getpid(), errno, strerror(errno));
		}
		exit(e);
	}

	default:
		VFLog(2, "VFPop::AsyncRetr() forked child %d", child_);
		return -1;
	}
}
#endif

#if 0
int VFPop::GetMail(int msg, ostream* mail)
{
	pop3 pop;

	if(!Connect(pop))
	{
		return EHOSTDOWN;
	}

	if(!pop->retr(msg, mail))
	{
		VFLog(1, "retr %d failed: %s", msg, pop->response());

		delete mail;

		return ECONNABORTED;
	}

	mail->flush();

	Disconnect(pop);

	return EOK;
}
#endif

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
%C - a POP3 virtual filesystem

usage: vf_pop3 [-hv] [-p vf] user[:passwd]@hostname
    -h   Print this message and exit.
    -v   Increase the verbosity level, default is 0.
    -p   Path to place virtual file system at, default is './user@hostname'.
    -m   Buffer read mail messages in memory, default is a temp file
         opened in /tmp with perms 0600 and immediately unlinked after
         being opened.
    -d   Don't become a daemon, default is to fork into the background after
         prompting for a password (if necessary).

Mounts the specified pop3 account as a virtual filesystem at 'vf'.

The password is optional, probably shouldn't be specified on the command
line for security reasons, and will be prompted for if not supplied.

Doing a rmdir on the path 'vf' will cause vf_pop to exit.
#endif

int		vOpt		= 0;
char*	pathOpt		= 0;
char*	accountOpt	= 0;
char*	userOpt		= 0;
char*	passOpt		= 0;
char*	hostOpt		= 0;
int		inmemOpt	= 0;
int		syncOpt		= 0;
int		dOpt		= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:msd")) != -1; ) {
		switch(c) {
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			VFLevel("vf_pop3", vOpt);
			break;

		case 'p':
			pathOpt = optarg;
			break;

		case 'm':
			inmemOpt = 1;
			break;

		case 's':
			syncOpt = 1;
			break;

		case 'd':
			dOpt = 1;
			break;

		default:
			exit(1);
		}
	}

	if(!(accountOpt = argv[optind])) {
		fprintf(stderr, "no account specified!\n");
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

	if(!pathOpt) {
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

	VFPop vfpop(hostOpt, userOpt, passOpt, inmemOpt, syncOpt);

	if(!dOpt)
	{
		// go into background

		switch(fork())
		{
		case 0:
			break;
		case -1:
			VFLog(0, "fork failed: [%d] %s", errno, strerror(errno));
			exit(1);
		default:
			exit(0);
		}
	}

	vfpop.Run(pathOpt, vOpt);
}

