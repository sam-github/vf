//
// vf_ram.cc
//
// Copyright (c) 1998, Sam Roberts
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
// Revision 1.14  1999/12/05 06:19:42  sam
// for some reason changing from String to Path requires that I also
// change from the offsetof macro to the __offsetof builtin
//
// Revision 1.13  1999/11/25 04:27:18  sam
// now integrates with mount command, when called mount_ram
//
// Revision 1.12  1999/10/04 03:33:52  sam
// forking is now done by the manager
//
// Revision 1.11  1999/08/09 15:16:23  sam
// Ported framework modifications down.
//
// Revision 1.10  1999/08/03 06:14:55  sam
// moved ram filesystem into its own subdirectory
//
// Revision 1.9  1999/06/21 12:37:27  sam
// implemented sysmsg... version
//
// Revision 1.8  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.7  1999/06/20 10:04:16  sam
// dir entity's factory now abstract and more modular
//
// Revision 1.6  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.5  1999/04/24 04:37:06  sam
// added support for symbolic links
//
// Revision 1.4  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.3  1998/04/05 23:53:13  sroberts
// added mkdir() support, so don't create my own directories any more
//
// Revision 1.2  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/kernel.h>

#include "vf_mgr.h"
#include "vf_file.h"
#include "vf_dir.h"
#include "vf_syml.h"
#include "vf_log.h"

#ifdef __USAGE
%C - mounts an in-memory virtual filesystem

usage: vf_ram [-hvd] [vf]
    -h   print help message
    -v   increase the verbosity level
    -d   debug, don't fork into the background

Mounts a RAM filesystem at 'vf' (the default is "ram").

This is primarily a test of the virtual filesystem framework. Currently
some basic filesystem operations such as execute, rename, and unlink,
as well as any permissions checks are missing. These operations are
being implemented, and as the framework supports them, so will the RAM
filesystem.
#endif

//
// VFRamFileEntity
//

class VFRamFileEntity : public VFFileEntity
{
public:
	VFRamFileEntity(pid_t pid, mode_t perm);
	~VFRamFileEntity();

	int Write(pid_t pid, size_t nbytes, off_t* offset,
			const void* data, int len);
	int Read(pid_t pid, size_t nbytes, off_t* offset);

private:
	char* data_; // pointer to buffer
	off_t dataLen_;  // length of data buffer
	off_t fileSize_; // size of file data (may be less than dataLen_)
};

VFRamFileEntity::VFRamFileEntity(pid_t pid, mode_t perm) :
	VFFileEntity(pid, perm),
	data_		(0),
	dataLen_	(0),
	fileSize_	(0)
{
}

VFRamFileEntity::~VFRamFileEntity()
{
	VFLog(3, "VFDirEntity::~VFRamFileEntity()");

	free(data_);
}

int VFRamFileEntity::Write(pid_t pid, size_t nbytes, off_t* offset,
		const void* data, int len)
{
	VFLog(2, "VFRamFileEntity::Write() pid %d size %ld, offset %ld len %ld",
		pid, nbytes, *offset, len);

	data = data, len = len;

	// grow the buffer if possible/necessary
	if(*offset + nbytes > dataLen_)
	{
		// for fast writes I should always allocate 2*dataLen_, and
		// unallocate when fileSize_ is < dataLen_/4, for now I use the more
		// memory conservative approach
		char* b = (char*) realloc(data_, *offset + nbytes);
		if(b) { data_ = b; dataLen_ = *offset + nbytes; }
	}
	// see if we can read any data into available space
	if(*offset >= dataLen_)
	{
		return ENOSPC;
	}
	if(*offset > fileSize_)
	{
		// offset past end of file, so zero data up to write point
		memset(&data_[fileSize_], 0, *offset - fileSize_);
	}
	if(dataLen_ - *offset < nbytes)
	{
		// we can only partially fulfill the write request
		nbytes = dataLen_ - *offset;
	}

	// ready to read nbytes into the data buffer from the offset of the
	// "data" part of the write message
	size_t dataOffset = __offsetof(struct _io_write, data);
	unsigned incr = Readmsg(pid, dataOffset, &data_[*offset], nbytes);

	// XXX use the optimization of data/len

	if(incr != -1)
	{
		VFLog(5, "VFRamFileEntity::Write() wrote \"%.*s\"",
			__max(incr, 20),
			&data_[*offset]);

		// update sizes if end of write is past current size
		off_t end = *offset + incr;
		if(end > fileSize_) { fileSize_ = info_.size = end; }

		// update current ocb offset
		*offset += incr;
	}

	struct _io_write_reply reply;

	reply.status = EOK;
	reply.nbytes = incr;
	reply.zero = 0;

	return ReplyMsg(pid, &reply, sizeof reply);
}

int VFRamFileEntity::Read(pid_t pid, size_t nbytes, off_t* offset)
{
	VFLog(2, "VFRamFileEntity::Read() size %ld, offset %ld len %ld size %ld",
		nbytes, *offset, dataLen_, fileSize_);

	// adjust nbytes if the read would be past the end of file
	if(*offset > fileSize_) {
			nbytes = 0;
	} else if(*offset + nbytes > fileSize_) {
		nbytes = fileSize_ - *offset;
	}

	struct _io_read_reply reply;
	struct _mxfer_entry mx[2];

	reply.status = EOK;
	reply.nbytes = nbytes;
	reply.zero = 0;

	_setmx(mx + 0, &reply, sizeof(reply) - sizeof(reply.data));
	_setmx(mx + 1, &data_[*offset], nbytes);

	if(Replymx(pid, 2, mx) != -1)
	{
		*offset += nbytes;
	}

	return -1;
}

//
// VFRamEntityFactory
//

class VFRamEntityFactory : public VFEntityFactory
{
public:
	VFEntity* Link(pid_t pid, mode_t mode, const char* linkto)
	{
		return new VFSymLinkEntity(pid, mode, linkto);
	}

	VFEntity* Dir(pid_t pid, mode_t mode)
	{
		return new VFDirEntity(pid, mode, this);
	}

	VFEntity* File(pid_t pid, mode_t mode)
	{
		VFEntity* entity = new VFRamFileEntity(pid, mode);
		if(!entity) { errno = ENOMEM; }
		return entity;
	}
};

//
// vf_ram: globals
//

int		vOpt = 0;
char*	pathOpt = "ram";
int		debugOpt = 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvdt:")) != -1; )
	{
		switch(c)
		{
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			break;

		case 'd':
			debugOpt = 1;
			break;

		case 't': // ignore this, it's passed by 'mount'
			break;

		default:
			exit(1);
		}
	}

	pathOpt = argv[optind] ? argv[optind] : pathOpt;

	return 0;
}

void main(int argc, char* argv[])
{
	GetOpts(argc, argv);

	mode_t mask = umask(0); umask(mask);

	VFEntity* root = new VFDirEntity(getpid(), 0555 & ~mask, new VFRamEntityFactory);

	VFVersion vfver("vf_ram", 0.02);

	VFManager vfmgr(vfver);

	if(!vfmgr.Init(root, pathOpt, vOpt))
	{
		printf("VFManager init failed: [%d] %s\n",
			errno, strerror(errno)
			);
		exit(1);
	}

	vfmgr.Start(debugOpt);
	vfmgr.Run();
}

