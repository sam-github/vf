//
// vf_popfile.cc
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
// Revision 1.1  1999/09/26 23:40:21  sam
// Initial revision
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

#include <vf_log.h>

#include "vf_pop.h"
#include "vf_popfile.h"

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

