//
// vf_file.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
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

VFFileEntity::VFFileEntity(mode_t mode) :
	data_	(0),
	len_	(0),
	size_	(0)
{
	InitStat(mode);
}

VFFileEntity::~VFFileEntity()
{
	VFLog(3, "VFDirEntity::~VFFileEntity()");
}

VFOcb* VFFileEntity::Open(
	const String& path,
	_io_open* req,
	_io_open_reply* reply)
{
	VFLog(2, "VFFileEntity::Open(\"%s\")", (const char *) path);

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

int VFFileEntity::MkDir(
	const String& path,
	_fsys_mkspecial* req,
	_fsys_mkspecial_reply* reply
	)
{
	VFLog(2, "VFFileEntity::MkDir(\"%s\")", (const char *) path);

	req = req;

	reply->status = ENOTDIR;
	return sizeof(reply->status);
}

bool VFFileEntity::Insert(const String& path, VFEntity* entity)
{
	errno = ENOTDIR;
	return false;
}

struct stat* VFFileEntity::Stat()
{
	return &stat_;
}

int VFFileEntity::Write(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "VFFileEntity::Write() size %ld, offset %ld len %ld size %ld",
		nbytes, offset, len_, size_);

	// grow the buffer if possible/necessary
	if(offset + nbytes > len_)
	{
		char* b = (char*) realloc(data_, offset + nbytes);
		if(b) { data_ = b; len_ = offset + nbytes; }
	}
	// see if we can read any data into available space
	if(offset >= len_)
	{
		errno = ENOSPC;
		return -1;
	}
	if(offset >= size_)
	{
		// offset past end of file, so zero data up to write point
		memset(&data_[size_], 0, offset - size_);
	}
	if(len_ - offset < nbytes)
	{
		// we can only partially fulfill the write request
		nbytes = len_ - offset;
	}

	// ready to read nbytes into the data buffer
	_mxfer_entry mx[1];
	_setmx(&mx[0], &data_[offset], nbytes);

	size_t dataOffset = offsetof(struct _io_write, data);
	unsigned ret = Readmsgmx(pid, dataOffset, 1, mx);

	if(ret != -1)
	{
		VFLog(2, "VFFileEntity::Write() wrote \"%.*s\"", ret, &data_[offset]);

		// update sizes if end of write is past current size
		off_t end = offset + ret;
		if(end > size_) { size_ = stat_.st_size = end; }
	}

	return ret;
}

void VFFileEntity::InitStat(mode_t mode)
{
	memset(&stat_, 0, sizeof stat_);

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
	file_	(file)
{
}

VFFileOcb::~VFFileOcb()
{
}

int VFFileOcb::Stat()
{
	return 0;
}

int VFFileOcb::Read()
{
	return 0;
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

	reply->zero = 0;
	return sizeof(*reply);
}

int VFFileOcb::Seek()
{
	return 0;
}

int VFFileOcb::Chmod()
{
	return 0;
}

int VFFileOcb::Chown()
{
	return 0;
}

int VFFileOcb::ReadDir(_io_readdir* req, _io_readdir_reply* reply)
{
	return 0;
}

int VFFileOcb::RewindDir(_io_rewinddir* req, _io_rewinddir_reply* reply)
{
	return 0;
}

