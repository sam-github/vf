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
// PopFile
//

PopFile::PopFile(int msg, PopDir& pop, int size) :
	dir_(pop), msg_(msg), data_(0)
{
	InitStat(S_IRUSR);
	stat_.st_size = size_ = size;
}

PopFile::~PopFile() { delete data_; }

int PopFile::Write(pid_t pid, size_t nbytes, off_t offset)
{
	pid = pid; nbytes = nbytes; offset = offset;

	errno = ENOTSUP;

	return -1;
}

int PopFile::Read(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "PopFile::Read() size %ld, offset %ld size %ld",
		nbytes, offset, size_);

	if(!data_) {
		data_ = dir_.Retr(msg_);
		if(!data_) {
			return -1;
		}
	}

	// adjust nbytes if the read would be past the end of file
	if(offset > size_) {
			nbytes = 0;
	} else if(offset + nbytes > size_) {
		nbytes = size_ - offset;
	}

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
// PopDir
//

PopDir::PopDir(const char* host, const char* user, const char* pass) :
	VFDirEntity(0500),
	host_(host),
	user_(user),
	pass_(pass)
{
	pop3 pop;

	if(!Connect(pop)) { exit(1); }

	int count;
	if(!pop->stat(&count)) { PopFail("stat", pop); }

	for(int msg = 1; msg <= count; ++msg) {
		int size;

		if(!pop->list(msg, &size)) { PopFail("list", pop); }

		if(!Insert(ItoA(msg), new PopFile(msg, *this, size))) {
			VFLog(0, "insert msg failed: [%d] %s\n", errno, strerror(errno));
			Disconnect(pop); // unnecessary, but polite
			exit(1);
		}
	}

	Disconnect(pop);
}

istream* PopDir::Retr(int msg)
{
	pop3 pop;

	if(!Connect(pop))
	{
		errno = EHOSTDOWN;

		return 0;
	}

	strstream* mail = new strstream;

	if(!mail)
	{
		VFLog(0, "out of memory");

		errno = ENOMEM;

		goto disconnect;
	}

	if(!pop->retr(msg, mail))
	{
		VFLog(1, "retr %d failed: %s", msg, pop->response());

		delete mail;

		mail = 0;

		errno = ECONNABORTED;

		goto disconnect;
	}

disconnect:
	Disconnect(pop);

	return mail;
}

int PopDir::Connect(pop3& pop)
{
	int fail = pop->connect(host_);

	if(fail) {
		VFLog(0, "connect to %s failed: [%d] %s\n",
			host_, fail, strerror(fail));
		return 0;
	}

	 if(!pop->checkconnect()) {
		VFLog(0, "connect to %s failed: %s",
			host_, pop->response() + strlen("-ERR "));
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

int PopDir::Disconnect(pop3& pop)
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
    -h   print help message
    -v   increse the verbosity level
    -p   path of virtual file system to create, default is the base
         name of the file, without any extension

Mounts the specified pop3 account as a virtual filesystem at 'vf'. The
password is optional, probably shouldn't be specified on the command
line (for security reasons), and will be prompted for if not supplied.
Messages on the server can be read or deleted. Deleted mail will actually
be deleted on server only on normal exit, caused by doing a rmdir on the
vfsys path (causing vf_pop3 to exit gracefully).
#endif

int		vOpt		= 0;
char*	pathOpt		= 0;
char*	accountOpt	= 0;
char*	userOpt		= 0;
char*	passOpt		= 0;
char*	hostOpt		= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:")) != -1; ) {
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
	VFManager* vfmgr = new VFManager;

	VFLevel("vf_pop", vOpt);

	GetOpts(argc, argv);

	VFEntity* root = new PopDir(hostOpt, userOpt, passOpt);

	if(!vfmgr->Init(root, pathOpt, vOpt)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	vfmgr->Run();
}

