//
// vf.cc
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
// Revision 1.4  1999/08/09 15:29:29  sam
// distinct inode numbers implemented, now find works
//
// Revision 1.3  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.2  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.1  1998/03/19 07:43:30  sroberts
// Initial revision
//

#include "sys/kernel.h"
#include "sys/sendmx.h"

#include "vf.h"
#include "vf_log.h"
#include "vf_fdmap.h"

//
// VFInfo
//

static ino_t VFInfo::INo()
{
	static ino_t ino = 1;

	return ++ino;
}


static dev_t VFInfo::DevNo()
{
	static dev_t dev = qnx_device_attach();

	assert(dev != -1);

	return dev;
}

//
// VFEntity
//

VFEntity::~VFEntity()
{
	VFLog(3, "VFEntity::~VFEntity()");
}

int VFEntity::Unlink(pid_t pid, const String& path)
{
    VFLog(2, "VFDirEntity::Unlink() pid %d path \"%s\"",
        pid, (const char *) path);

    return ENOSYS;
}

int VFEntity::Chmod(pid_t pid, mode_t mode)
{
	VFLog(2, "VFEntity::Chmod() pid %d mode %#x\n", pid, mode);

	// permissions check...

	info_.mode = (info_.mode & ~07777) | (mode & 07777);

	return EOK;
}

int VFEntity::Chown(pid_t pid, uid_t uid, gid_t gid)
{
	VFLog(2, "VFEntity::Chown() pid %d uid %d gid %d\n", pid, uid, gid);

	// permissions check...

	info_.uid = uid;
	info_.gid = gid;

	return EOK;
}

int VFEntity::Fstat(pid_t pid)
{
    VFLog(2, "VFEntity::Fstat() pid %d", pid);

    ReplyInfo(pid);

    return -1;
}

int VFEntity::Stat(struct stat* s)
{
    info_.Stat(s);

    return EOK;
}

int VFEntity::ReplyStatus(pid_t pid, int status)
{
	msg_t msg = status;

	return ReplyMsg(pid, &msg, sizeof msg);
}

int VFEntity::ReplyInfo(pid_t pid, const struct VFInfo* info)
{
	if(!info) { info = &info_; }

	struct _io_fstat_reply reply;

	reply.status = EOK;
	reply.zero = 0;
	info->Stat(&reply.stat);

	return ReplyMsg(pid, &reply, sizeof reply);
}

int VFEntity::ReplyMsg(pid_t pid, const void* msg, size_t size)
{
	if(Reply(pid, msg, size) == -1) {
		VFLog(1, "VFEntity::ReplyMsg() Reply(%d) failed: [%d] %s",
			pid, errno, strerror(errno));
	}

	return -1;
}

int VFEntity::ReplyMx(pid_t pid, unsigned len, struct _mxfer_entry* mx)
{
	// cast away the const because the pragma doesn't declare its args as
	// const, though the function does
	if(Replymx(pid, len, mx) == -1) {
		VFLog(1, "VFEntity::ReplyMsg() Replymx(%d) failed: [%d] %s",
			pid, errno, strerror(errno));
	}

	return -1;
}

int VFEntity::FdAttach(pid_t pid, int fd, VFOcb* ocb)
{
	if(!ocb) { return ENOMEM; }

	if(!VFFdMap::Fd().Map(pid, fd, ocb)) {
		delete ocb;
		return errno;
	}

	return EOK;
}


//
// VFOcb
//

VFOcb::VFOcb() :
	refCount_(0)
{
	VFLog(3, "VFOcb::VFOcb()");
}

VFOcb::~VFOcb()
{
	VFLog(3, "VFOcb::~VFOcb()");
}

