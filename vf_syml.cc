//
// vf_syml.cc
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
// Revision 1.4  1999/12/05 01:50:24  sam
// replaced String with a custom Path class
//
// Revision 1.3  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.2  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.1  1999/04/24 04:37:06  sam
// Initial revision
//

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/sendmx.h>
#include <unistd.h>
#include <time.h>

#include "vf_syml.h"
#include "vf_log.h"

//
// VFSymLinkEntity
//

VFSymLinkEntity::VFSymLinkEntity(pid_t pid, mode_t perm, const char* linkto) :
	VFEntity(pid, S_IFLNK | 0777)
{
	VFLog(3, "VFSymLinkEntity::VFSymLinkEntity() pid %d perm %#x linkto %s",
		pid, perm, linkto);

	assert(linkto);
	assert(strlen(linkto) <= PATH_MAX);

	info_.size = strlen(linkto_);

	strncpy(linkto_, linkto, sizeof(linkto_) - 1);
	linkto_[sizeof(linkto_)] = '\0';
}

VFSymLinkEntity::VFSymLinkEntity(uid_t uid, gid_t gid, mode_t perm, const char* linkto) :
	VFEntity(uid, gid, S_IFLNK | 0777)
{
	VFLog(3, "VFSymLinkEntity::VFSymLinkEntity() uid %d gid %d perm %#x linkto %s",
		uid, gid, perm, linkto);

	// XXX this code should be factored into a method, not cut-n-pasted!

	assert(linkto);
	assert(strlen(linkto) <= PATH_MAX);

	info_.size = strlen(linkto_);

	strncpy(linkto_, linkto, sizeof(linkto_) - 1);
	linkto_[sizeof(linkto_)] = '\0';
}

int VFSymLinkEntity::Open(pid_t pid, const Path& path, int fd, int oflag, mode_t mode)
{
	VFLog(2, "VFSymLinkEntity::Open() pid %d path \"%s\"",
		pid, path.c_str());

	fd = fd, oflag = oflag, mode = mode;

	return RewriteOpenPath(pid, path);
}

int VFSymLinkEntity::Stat(pid_t pid, const Path& path, int lstat)
{
	VFLog(2, "VFSymLinkEntity::Stat() pid %d path \"%s\" lstat %d",
		path.c_str(), lstat);

	// If we're just a component in the path, rewrite it.
	if(path != "")
	{
		return RewriteOpenPath(pid, path);
	}

	// if mode is set it's an lstat()
	if(lstat) {
		ReplyInfo(pid, &info_);
	}

	// otherwise it's a normal stat(), so rewrite the path
	return RewriteOpenPath(pid, path);
}

int VFSymLinkEntity::ChDir(pid_t pid, const Path& path)
{
	VFLog(2, "VFSymLinkEntity::ChDir() pid %d path \"%s\"",
		pid, path.c_str());

	return RewriteOpenPath(pid, path);
}

int VFSymLinkEntity::Unlink(pid_t pid, const Path& path)
{
	VFLog(2, "VFSymLinkEntity::Unlink() pid %d path \"%s\"",
		pid, path.c_str());

	if(path != "")
	{
		return RewriteOpenPath(pid, path);
	}

	return ENOSYS;
}

int VFSymLinkEntity::MkSpecial(pid_t pid, const Path& path, mode_t mode,
		const char* linkto)
{
	VFLog(2, "VFSymLinkEntity::MkSpecial() pid %d path \"%s\" mode %#x",
		pid, path.c_str(), mode);

	linkto = linkto;

	if(path == "") { return EEXIST; }

	return RewriteOpenPath(pid, path);
}

int VFSymLinkEntity::ReadLink(pid_t pid, const Path& path)
{
	VFLog(2, "VFSymLinkEntity::ReadLink() pid %d path \"%s\"",
		pid, path.c_str());

	if(path != "") {
		return RewriteOpenPath(pid, path);
	}

	struct _fsys_readlink_reply reply;
	struct _mxfer_entry mx[2];

	reply.status = EOK;
	reply.zero = 0;

	_setmx(mx + 0, &reply, sizeof(reply) - sizeof(reply.path));
	_setmx(mx + 1, linkto_, strlen(linkto_) + 1);

	return ReplyMx(pid, 2, mx);
}

int VFSymLinkEntity::Insert(const Path& path, VFEntity* entity)
{
	VFLog(2, "VFSymLinkEntity::Insert(\"%s\")", path.c_str());

	entity = entity;

	return ENOTDIR;
}

int VFSymLinkEntity::RewriteOpenPath(pid_t pid, const Path& path)
{
	VFLog(2, "VFSymLinkEntity::RewriteOpenPath() pid rewrite '%s' as '%s'",
		path.c_str(), linkto_);

	if((strlen(linkto_) + 1 + path.size()) > _MAX_PATH) {
		return ENAMETOOLONG;
	}

	msg_t reply = EMORE;

	char p[_MAX_PATH + 1];

	strcpy(p, linkto_); // p is bigger than the linkto buffer

	if(path != "")
	{
		// append the sub-path they're really interested in
		strcat(p, "/");
		strcat(p, path.c_str());
	}

	int o = __offsetof(_io_open, path);
	if(Writemsg(pid, o, p, strlen(p) + 1) == -1)
	{
		VFLog(1, "VFSymLinkEntity Writemsg(pid %d off %d) failed: [%d] %s",
			pid, __offsetof(_io_open, path), errno, strerror(errno));
		return errno;
	}

	return EMORE;
}

