//
// vf_file.cc
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
// Revision 1.14  1999/12/05 01:50:24  sam
// replaced String with a custom Path class
//
// Revision 1.13  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.12  1999/08/03 06:13:38  sam
// moved ram filesystem into its own subdirectory
//
// Revision 1.11  1999/07/11 11:26:46  sam
// Added arg to init stat to a particular type, defaults to reg file.
//
// Revision 1.10  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.9  1999/06/20 10:05:19  sam
// fixed buffer overflow problems with large files and verbose debug messages
//
// Revision 1.8  1999/06/18 16:48:06  sam
// Made InitStat a protected member of the class that had the stat_ data.
//
// Revision 1.7  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.6  1999/04/24 04:37:06  sam
// added support for symbolic links
//
// Revision 1.5  1999/04/11 06:41:37  sam
// split FileEntity into a RamFileEntity and a base FileEntity that is used by
// the FileOcb, turns out most of the FileEntity is reuseable, just the actual
// read/write functions were specific to the RAM filesystem
//
// Revision 1.4  1998/04/28 07:22:53  sroberts
// the sprintf of an entire file in the Write() log message was segving
//
// Revision 1.3  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.2  1998/04/06 06:50:19  sroberts
// implemented creat(), and write()
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/sendmx.h>
#include <unistd.h>
#include <time.h>

#include "vf_file.h"
#include "vf_log.h"

//
// VFFileEntity
//

int VFFileEntity::Open(pid_t pid, const Path& path, int fd, int oflag, mode_t mode)
{
	VFLog(2, "VFFileEntity::Open() pid %d path \"%s\" fd %d oflag %#x mode %#x",
		pid, path.c_str(), fd, oflag, mode);

	if(path != "")
	{
		return ENOTDIR;
	}

	return FdAttach(pid, fd, new VFFileOcb(this));
}

int VFFileEntity::Stat(pid_t pid, const Path& path, int lstat)
{
	VFLog(2, "VFFileEntity::Stat() pid %d path \"%s\" lstat %d",
		pid, path.c_str(), lstat);

	if(path != "")
	{
		return ENOTDIR;
	}

	return ReplyInfo(pid);
}

int VFFileEntity::ChDir(pid_t pid, const Path& path)
{
	VFLog(2, "VFFileEntity::ChDir() pid %d \"%s\"",
		pid, path.c_str());

	return ENOTDIR;
}

int VFFileEntity::MkSpecial(pid_t pid, const Path& path, mode_t mode,
		const char* linkto)
{
	VFLog(2, "VFFileEntity::MkSpecial() pid %d path \"%s\" mode %#x",
		pid, path.c_str(), mode);

	linkto = linkto;

	if(path == "") { return EEXIST; }

	return ENOTDIR;
}

int VFFileEntity::ReadLink(pid_t pid, const Path& path)
{
	VFLog(2, "VFFileEntity::ReadLink() pid %d path \"%s\"",
		pid, path.c_str());

	if(path != "") {
		return ENOTDIR;
	}
	return EINVAL;
}

int VFFileEntity::Insert(const Path& path, VFEntity* entity)
{
	path.c_str(), entity = entity;

	return ENOTDIR;
}

int VFFileEntity::Write(pid_t pid, size_t nbytes, off_t* offset,
		const void* data, int len)
{
	pid = pid, nbytes = nbytes, offset = offset, data = data, len = len;

	return ENOTSUP;
}

int VFFileEntity::Read(pid_t pid, size_t nbytes, off_t* offset)
{
	pid = pid, nbytes = nbytes, offset = offset;

	return ENOTSUP;
}

//
// VFFileOcb
//

VFFileOcb::VFFileOcb(VFFileEntity* file) :
	file_	(file),
	offset_	(0)
{
}

VFFileOcb::~VFFileOcb()
{
}

int VFFileOcb::Write(pid_t pid, int nbytes, const void* data, int len)
{
	// permissions checks...

	return file_->Write(pid, nbytes, &offset_, data, len);
}

int VFFileOcb::Read(pid_t pid, int nbytes)
{
	// permissions checks...

	return file_->Read(pid, nbytes, &offset_);
}

int VFFileOcb::Seek(pid_t pid, int whence, off_t offset)
{
	pid = pid;


	switch(whence)
	{
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += offset_;
		break;
	case SEEK_END: {
		struct stat s;
		file_->Stat(&s);
		offset = s.st_size - offset;
		break;
		}
	default:
		return EINVAL;
	}

	if(offset < 0)
	{
		return EINVAL;
	}

	struct _io_lseek_reply reply;

	reply.status = EOK;
	reply.zero = 0;
	reply.offset = offset_ = offset;

	return file_->ReplyMsg(pid, &reply, sizeof(reply));
}

int VFFileOcb::Fstat(pid_t pid)
{
	VFLog(2, "VFFileEntity::Fstat() pid %d", pid);

	return file_->Fstat(pid);
}

int VFFileOcb::Chmod(pid_t pid, mode_t mode)
{
	return file_->Chmod(pid, mode);
}

int VFFileOcb::Chown(pid_t pid, uid_t uid, gid_t gid)
{
	return file_->Chown(pid, uid, gid);
}

int VFFileOcb::ReadDir(pid_t pid, int ndirs)
{
	pid = pid, ndirs = ndirs;

	return EBADF;
}

int VFFileOcb::RewindDir(pid_t pid)
{
	pid = pid;

	return EBADF;
}

