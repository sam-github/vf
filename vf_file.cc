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

VFFileEntity::VFFileEntity()
{
	memset(&stat_, 0, sizeof(stat_));
}

VFOcb* VFFileEntity::Open(
	const String& path,
	_io_open* req,
	_io_open_reply* reply)
{
	VFLog(2, "VFFileEntity::Open() fd %d path \"%s\"",
		req->fd, (const char *) path);

	if(path != "")
	{
		reply->status = ENOTDIR;
		return 0;
	}

	reply->status = EOK;
	return new VFFileOcb(this);
}

int VFFileEntity::Stat(
    const String& path,
	_io_open* req,
	_io_fstat_reply* reply
	)
{
	VFLog(2, "VFFileEntity::Stat(\"%s\")", (const char *) path);

	req = req; reply = reply;

	if(path != "")
	{
		reply->status = ENOTDIR;
		return 0;
	}
	else
	{
		reply->status = EOK;
		reply->zero = 0;
		reply->stat   = stat_;
	}

	return sizeof(*reply);
}

int VFFileEntity::ChDir(
	const String& path,
	_io_open* open,
	_io_open_reply* reply
	)
{
	VFLog(2, "VFFileEntity::ChDir(\"%s\")", (const char *) path);

	open = open; reply = reply;

	reply->status = ENOTDIR;
	return sizeof(reply->status);
}

int VFFileEntity::Unlink()
{
	return 0;
}

int VFFileEntity::MkSpecial(
	const String& path,
	_fsys_mkspecial* req,
	_fsys_mkspecial_reply* reply
	)
{
	VFLog(2, "VFFileEntity::MkSpecial(\"%s\")", (const char *) path);

	req = req;

	reply->status = ENOTDIR;

	return sizeof(reply->status);
}

int VFFileEntity::ReadLink(
	const String& path,
	_fsys_readlink* req,
	_fsys_readlink_reply* reply
	)
{
	VFLog(2, "VFFileEntity::ReadLink(\"%s\")", (const char *) path);

	req = req;

	if(path == "") {
		reply->status = EINVAL;
	} else { 
		reply->status = ENOTDIR;
	}

	return sizeof(reply->status);
}

bool VFFileEntity::Insert(const String& path, VFEntity* entity)
{
	(const char*) path; entity = entity;

	errno = ENOTDIR;
	return false;
}

struct stat* VFFileEntity::Stat()
{
	return &stat_;
}

int VFFileEntity::Write(pid_t pid, size_t nbytes, off_t offset)
{
	pid = pid; nbytes = nbytes; offset = offset;

	errno = ENOTSUP;
	return -1;
}

int VFFileEntity::Read(pid_t pid, size_t nbytes, off_t offset)
{
	pid = pid; nbytes = nbytes; offset = offset;

	errno = ENOTSUP;
	return -1;
}

void VFFileEntity::InitStat(mode_t mode)
{
	memset(&stat_, 0, sizeof stat_);

	// clear all but permission bits from mode
	mode &= 0777;

	stat_.st_mode = mode | S_IFREG;
	stat_.st_nlink = 1;

	stat_.st_ouid = getuid();
	stat_.st_ogid = getgid();

	stat_.st_ftime =
	  stat_.st_mtime =
	    stat_.st_atime =
	      stat_.st_ctime = time(0);

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

int VFFileOcb::Write(pid_t pid, _io_write* req, _io_write_reply* reply)
{
	// permissions checks...

	reply->nbytes = file_->Write(pid, req->nbytes, offset_);

	if(reply->nbytes == -1)
	{
		reply->status = errno;
		return sizeof(reply->status);
	}
	// fill in successful reply
	reply->status = EOK;
	reply->zero = 0;

	// update this ocb's file position
	offset_ += reply->nbytes;

	return sizeof(*reply);
}

int VFFileOcb::Read(pid_t pid, _io_read* req, _io_read_reply* reply)
{
	// permissions checks...

	reply->nbytes = file_->Read(pid, req->nbytes, offset_);

	if(reply->nbytes == -1)
	{
		reply->status = errno;
		return sizeof(reply->status);
	}
	// fill in successful reply
	reply->status = EOK;
	reply->zero = 0;

	// update this ocb's file position
	offset_ += reply->nbytes;

	// the file_->Read() already did the data part of the reply,
	// so be very careful not to overwrite that data with our reply
	return sizeof(*reply) - sizeof(reply->data);
}

int VFFileOcb::Seek(pid_t pid, _io_lseek* req, _io_lseek_reply* reply)
{
	pid = pid;

	off_t to = offset_;
	if(req->whence == SEEK_SET)
	{
		to = req->offset;
	}
	else if(req->whence == SEEK_CUR)
	{
		to += req->offset;
	}
	else if(req->whence == SEEK_END)
	{
		to = file_->Stat()->st_size - req->offset;
	}
	else
	{
		reply->status = EINVAL;
		return sizeof(reply->status);
	}

	if(to < 0)
	{
		reply->status = EINVAL;
		return sizeof(reply->status);
	}

	reply->offset = offset_ = to;
	reply->zero = 0;
	reply->status = EOK;

	return sizeof(*reply);
}

int VFFileOcb::Stat(pid_t pid, _io_fstat* req, _io_fstat_reply* reply)
{
	VFLog(2, "VFFileEntity::Stat() pid %d fd %d", pid, req->fd);

	struct stat* stat = file_->Stat();
	assert(stat);

	// yeah, I know I should do the Replymx thing to avoid memcpys...
	reply->status = EOK;
	reply->zero = 0;
	reply->stat = *stat;

	return sizeof(*reply);
}

int VFFileOcb::Chmod()
{
	return 0;
}

int VFFileOcb::Chown()
{
	return 0;
}

int VFFileOcb::ReadDir(pid_t pid, _io_readdir* req, _io_readdir_reply* reply)
{
	pid = pid; req = req; reply = reply;

	return 0;
}

int VFFileOcb::RewindDir(pid_t pid, _io_rewinddir* req, _io_rewinddir_reply* reply)
{
	pid = pid; req = req; reply = reply;

	return 0;
}

//
// VFRamFileEntity
//

VFRamFileEntity::VFRamFileEntity(mode_t mode) :
	data_	(0),
	dataLen_	(0),
	fileSize_	(0)
{
	InitStat(mode);
}

VFRamFileEntity::~VFRamFileEntity()
{
	VFLog(3, "VFDirEntity::~VFRamFileEntity()");

	free(data_);
}

int VFRamFileEntity::Write(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "VFRamFileEntity::Write() size %ld, offset %ld len %ld size %ld",
		nbytes, offset, dataLen_, fileSize_);

	// grow the buffer if possible/necessary
	if(offset + nbytes > dataLen_)
	{
		// for fast writes I should always allocate 2*dataLen_, and
		// unallocate when fileSize_ is < dataLen_/4, for now I use the more
		// memory conservative approach
		char* b = (char*) realloc(data_, offset + nbytes);
		if(b) { data_ = b; dataLen_ = offset + nbytes; }
	}
	// see if we can read any data into available space
	if(offset >= dataLen_)
	{
		errno = ENOSPC;
		return -1;
	}
	if(offset > fileSize_)
	{
		// offset past end of file, so zero data up to write point
		memset(&data_[fileSize_], 0, offset - fileSize_);
	}
	if(dataLen_ - offset < nbytes)
	{
		// we can only partially fulfill the write request
		nbytes = dataLen_ - offset;
	}

	// ready to read nbytes into the data buffer from the offset of the
	// "data" part of the write message
	size_t dataOffset = offsetof(struct _io_write, data);
	unsigned ret = Readmsg(pid, dataOffset, &data_[offset], nbytes);

	if(ret != -1)
	{
		VFLog(5, "VFRamFileEntity::Write() wrote \"%.*s\"",
			__max(ret, 20),
			&data_[offset]);

		// update sizes if end of write is past current size
		off_t end = offset + ret;
		if(end > fileSize_) { fileSize_ = stat_.st_size = end; }
	}

	return ret;
}

int VFRamFileEntity::Read(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "VFRamFileEntity::Read() size %ld, offset %ld len %ld size %ld",
		nbytes, offset, dataLen_, fileSize_);

	// adjust nbytes if the read would be past the end of file
	if(offset > fileSize_) {
			nbytes = 0;
	} else if(offset + nbytes > fileSize_) {
		nbytes = fileSize_ - offset;
	}

	// ready to write nbytes from the data buffer to the offset of the
	// "data" part of the read reply message
	size_t dataOffset = offsetof(struct _io_read_reply, data);
	unsigned ret = Writemsg(pid, dataOffset, &data_[offset], nbytes);

	if(ret != -1) {
		VFLog(5, "VFRamFileEntity::Read() read \"%.*s\"",
			__max(ret, 20),
			&data_[offset]);
	}

	return ret;
}

