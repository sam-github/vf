//
// vf_tgzfile.cc
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
// Revision 1.1  1999/11/24 03:54:01  sam
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

#include "vf_tgz.h"
#include "vf_tgzfile.h"

//
// VFTgzFile
//

char VFTgzFileEntity::buffer_[BUFSIZ];

VFTgzFileEntity::VFTgzFileEntity(const TarUntar& untar, off_t size) :
	VFFileEntity(getuid(), getgid(), 0440),
	untar_	(untar),
	cache_	(-1)
{
	size = size;
}

VFTgzFileEntity::~VFTgzFileEntity()
{
	if(cache_ != -1) {	
		close(cache_);
	}
}

int VFTgzFileEntity::Read(pid_t pid, size_t nbytes, off_t* offset)
{
	VFLog(2, "VFTgzFileEntity::Read() pid %d nbytes %ld offset %ld",
		pid, nbytes, *offset);

	if(cache_ == -1) {
		// open tmp file
		const char* n = tmpnam(0);
		cache_ = open(n, O_CREAT|O_EXCL|O_RDWR, 0600);
		if(cache_ == -1) {
			int e = errno;
			VFLog(0, "open %s failed: " VFERRF, VFERR(e));
			return e;
		}
		unlink(n);

		untar_.Open();
		int b = 0;
		while((b = untar_.Read(buffer_, sizeof(buffer_))) > 0) {
			if(write(cache_, buffer_, b) == -1) {
				int e = errno;
				VFLog(0, "write cache failed: " VFERRF, VFERR(e));
				return e;
			}
		}
	}

	assert(cache_ != -1);

	// adjust nbytes if the read would be past the end of file
	if(*offset > info_.size) {
			nbytes = 0;
	} else if(*offset + nbytes > info_.size) {
		nbytes = info_.size - *offset;
	}

	// XXX Max msg len is 65500U, we need to be able to deal with this
	char* buffer = (char*) alloca(nbytes);

	if(!buffer)
	{
		return ENOMEM;
	}

	// these will set errno if we're using a file, if its a strstream...?
	if(lseek(cache_, *offset, SEEK_SET) == -1) {
		int e = errno;
		VFLog(0, "lseek %u cache failed: " VFERRF, *offset, VFERR(e));
		return e;
	}
	if((nbytes = read(cache_, buffer, nbytes)) == -1) {
		int e;
		VFLog(0, "read cache failed: " VFERRF, VFERR(e));
		return e;
	}

	struct _io_read_reply reply;
	struct _mxfer_entry mx[2];

	reply.status = EOK;
	reply.nbytes = nbytes;
	reply.zero = 0;

	_setmx(mx + 0, &reply, sizeof(reply) - sizeof(reply.data));
	_setmx(mx + 1, buffer, nbytes);

	if(Replymx(pid, 2, mx) == -1)
	{
		int e;
		VFLog(0, "Replymx %d failed: " VFERRF, pid, VFERR(e));
		return e;
	}

	*offset += nbytes;

	return -1;
}

