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
#include <pwd.h>
#include <grp.h>

#include <sys/kernel.h>

#include "vf_pop.h"
#include <vf_dir.h>
#include <vf_file.h>
#include <vf_log.h>

#include "url.h"

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
// VFPopFile
//

VFPopFile::VFPopFile(int msg, VFPop& pop, int size) :
	pop_(pop), msg_(msg), data_(0), description_(0)
{
	InitStat(S_IRUSR, S_IFREG);
	stat_.st_size = size_ = size;

	ostrstream os;
	os << "TBD" << '\0';
	description_ = os.str();
}

VFPopFile::~VFPopFile()
{
	delete data_;
	delete description_;
}

int VFPopFile::Stat(const String* path, _io_open* req, _io_fstat_reply* reply)
{
    VFLog(2, "VFPopFile::Stat(\"%s\") mode %#x", (const char*) path, req->mode);

	reply->status	= EOK;
	reply->zero		= 0;
	reply->stat		= stat_;

	/* if it's an lstat(), claim we're a link */
	/* not working yet
	if(req->mode == S_IFLNK) {
		reply->stat.st_mode = (reply->stat.st_mode & ~S_IFMT) | S_IFREG;
	}
	*/

	return sizeof(*reply);
}

int VFPopFile::ReadLink(const String& path, _fsys_readlink* req, _fsys_readlink_reply* reply)
{
    VFLog(2, "VFSymLinkEntity::ReadLink(\"%s\") description %s",
		(const char*) path, description_);

    req = req; reply = reply;

    reply->status = EOK;
    strcpy(reply->path, description_);

    return sizeof(*reply) + strlen(description_);
}

int VFPopFile::Write(pid_t pid, size_t nbytes, off_t offset)
{
	pid = pid; nbytes = nbytes; offset = offset;

	errno = ENOTSUP;

	return -1;
}

int VFPopFile::Read(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "VFPopFile::Read() size %ld, offset %ld size %ld",
		nbytes, offset, size_);

	if(!data_) {
		data_ = pop_.Retr(msg_, this);
		if(data_ == VFPop::ERROR) {
			data_ = 0;
			return -1;
		}
		if(data_ == VFPop::PAUSE) {
			// queue the read request
			assert(! "implemented yet");
			data_ = 0;
			return 0;
		}
	}

	// adjust nbytes if the read would be past the end of file
	if(offset > size_) {
			nbytes = 0;
	} else if(offset + nbytes > size_) {
		nbytes = size_ - offset;
	}

	// this should be a loop with a 4k stack buffer, none of this one-shot
	// do it fail shit
	char* buffer = new char[nbytes];

	if(!buffer)
	{
		errno = ENOMEM;
	}

	// these will set errno if we're using a file, if its a strstream...?
	if(!data_->seekg(offset)) { return -1; }
	if(!data_->read(buffer, nbytes)) { return -1; }

	nbytes = data_->gcount(); // might not be nbytes...

	// ready to write nbytes from the data buffer to the offset of the
	// "data" part of the read reply message
	size_t dataOffset = offsetof(struct _io_read_reply, data);
	unsigned ret = Writemsg(pid, dataOffset, buffer, nbytes);

	delete[] buffer;

	return ret;
}

//
// VFPop
//

iostream* VFPop::ERROR = (iostream*) -1;
iostream* VFPop::PAUSE = (iostream*) -1;

VFPop::VFPop(const char* host, const char* user, const char* pass, int mbuf) :
	VFManager(VFVersion("vf_pop", 1.2)),
	root_(0500),
	host_(host),
	user_(user),
	pass_(pass),
	mbuf_(mbuf)
{
	pop3 pop;

	if(!Connect(pop)) { exit(1); }

	int count;
	if(!pop->stat(&count)) { PopFail("stat", pop); }

	for(int msg = 1; msg <= count; ++msg) {
		int size;

		if(!pop->list(msg, &size)) { PopFail("list", pop); }

		if(!root_.Insert(ItoA(msg), new VFPopFile(msg, *this, size))) {
			VFLog(0, "insert msg failed: [%d] %s\n", errno, strerror(errno));
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

	VFManager::Run();
}

istream* VFPop::Retr(int msg, VFPopFile* file)
{
	file = file;

	iostream* mail = Stream();

	if(!mail) { return 0; }

	pop3 pop;

	if(!Connect(pop))
	{
		errno = EHOSTDOWN;

		return ERROR;
	}

	if(!pop->retr(msg, mail))
	{
		VFLog(1, "retr %d failed: %s", msg, pop->response());

		delete mail;

		mail = ERROR;

		errno = ECONNABORTED;
	}

	Disconnect(pop);

	return mail;
}

int VFPop::Service(pid_t pid, VFIoMsg* msg)
{
	return VFManager::Service(pid, msg);
}

iostream* VFPop::Stream() const
{
	iostream* s = 0;

	if(mbuf_)
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
int		mbufOpt		= 0;
int		dOpt		= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:md")) != -1; ) {
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
			mbufOpt = 1;
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

	VFPop vfpop(hostOpt, userOpt, passOpt, mbufOpt);

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

