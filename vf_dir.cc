//
// vf_dir.cc
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
// Revision 1.16  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.15  1999/07/02 15:29:06  sam
// assert more things that must be true
//
// Revision 1.14  1999/06/21 10:28:20  sam
// fixed bug causing random return status of unsupported functions
//
// Revision 1.13  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.12  1999/06/20 10:04:16  sam
// dir entity's factory now abstract and more modular
//
// Revision 1.11  1999/04/30 01:50:57  sam
// Insert() will now create subdirectories if the directory has a factory.
//
// Revision 1.10  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.9  1999/04/24 04:37:06  sam
// added support for symbolic links
//
// Revision 1.8  1999/04/11 06:40:55  sam
// cleaned up code to stop unused arg warnings
//
// Revision 1.7  1998/04/28 07:25:22  sroberts
// added fd number to diagnostic output
//
// Revision 1.6  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.5  1998/04/06 06:49:05  sroberts
// implemented write(), implemented FileEntity factory, and removed unused
// close() memthod
//
// Revision 1.4  1998/04/05 23:54:41  sroberts
// added support for mkdir(), and a factory class for dir and file entities
//
// Revision 1.3  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.2  1998/03/15 22:07:49  sroberts
// implemented insertion
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/fsys.h>
#include <sys/kernel.h>
#include <sys/psinfo.h>
#include <sys/sendmx.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "vf_log.h"
#include "vf_dir.h"

//
// VFDirEntity
//

VFDirEntity::VFDirEntity(pid_t pid, mode_t perm, VFEntityFactory* factory) :
	VFEntity(pid, S_IFDIR | (perm & 0777)),
	map_(&Hash),
	factory_(factory)
{
}

VFDirEntity::VFDirEntity(uid_t uid, gid_t gid, mode_t perm, VFEntityFactory* factory) :
	VFEntity(uid, gid, S_IFDIR | (perm & 0777)),
	map_(&Hash),
	factory_(factory)
{
}

// need another ctor that takes a stat as an arg

VFDirEntity::~VFDirEntity()
{
	VFLog(3, "VFDirEntity::~VFDirEntity()");
}

int VFDirEntity::Open(pid_t pid, const String& path, int fd, int oflag, mode_t mode)
{
	VFLog(2, "VFDirEntity::Open() pid %d path \"%s\" fd %d oflag %#x mode %#x",
		pid, (const char *) path, fd, oflag, mode);

	// check permissions...

	if(path == "") // open this directory
	{
		return FdAttach(pid, fd, new VFDirOcb(this));
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	// XXX no factory and O_CREAT is a ENOTSUP, not ENOENT!
	// XXX what about O_EXCL? Deal with it!

	// if there is not an entity, perhaps create one
	if(!entity && tail == "" && factory_ && (oflag & O_CREAT))
	{
		entity = factory_->File(pid, mode);
		if(!entity)
		{
			VFLog(2, "VFDirEntity::Open() create failed: [%d] %s",
				errno, strerror(errno));
			return errno;
		}

		int e = Insert(lead, entity);

		if(e != EOK)
		{
			VFLog(2, "VFDirEntity::Open() Insert() failed: [%d] %s",
				e, strerror(e));
			delete entity;
			return e;
		}
	}

	if(entity)
	{
		return entity->Open(pid, tail, fd, oflag, mode);
	}

	VFLog(2, "VFDirEntity::Open() failed: no entity");

	return ENOENT;
}

int VFDirEntity::Stat(pid_t pid, const String& path, int lstat)
{
	VFLog(2, "VFDirEntity::Stat() pid %d path \"%s\" lstat %d",
		pid, (const char *) path, lstat);

	if(path == "")
	{
		return ReplyInfo(pid);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);
	if(!entity)
	{
		VFLog(2, "VFDirEntity::Stat() failed: [%d] %s",
			ENOENT, strerror(ENOENT));
		return ENOENT;
	}

	return entity->Stat(pid, tail, lstat);
}

int VFDirEntity::ChDir(pid_t pid, const String& path)
{
	VFLog(2, "VFDirEntity::ChDir() pid %d path \"%s\")",
		pid, (const char *) path);

	if(path == "")
	{
		// check permissions...

		return EOK;
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	if(!entity)
	{
		VFLog(2, "VFDirEntity::ChDir() failed: no entity");

		return ENOENT;
	}

	return entity->ChDir(pid, tail);
}

int VFDirEntity::MkSpecial(pid_t pid, const String& path,
		mode_t mode, const char* linkto)
{
	VFLog(2, "VFDirEntity::MkSpecial() pid %d path \"%s\" mode %#x",
		pid, (const char *) path, mode);

	if(path == "")
	{
		return EEXIST;
	}

	// recurse down to directory where special is to be made
	String lead;
	String tail;
	SplitPath(path, lead, tail);

	if(tail != "")
	{
		VFEntity* entity = map_.find(lead);

		if(!entity)
		{
			return ENOENT;
		}
		return entity->MkSpecial(pid, tail, mode, linkto);
	}

	// this is where special is to be made
	if(!factory_)
	{
		return ENOTSUP;
	}
	
	VFEntity* entity = factory_->MkSpecial(pid, mode, linkto);

	if(!entity)
	{
		VFLog(2, "VFDirEntity::MkSpecial() MkSpecial(%#x) failed: [%d] %s",
			mode, errno, strerror(errno));
		return errno;
	}

	int e = Insert(lead, entity);

	if(e != EOK)
	{
		VFLog(2, "VFDirEntity::MkSpecial() Insert() failed: [%d] %s",
			e, strerror(e));

		delete entity;
	}

	return e;
}

int VFDirEntity::ReadLink(pid_t pid, const String& path)
{
	VFLog(2, "VFDirEntity::ReadLink() pid %d path \"%s\"",
		pid, (const char *) path);

	if(path == "")
	{
		return EINVAL;
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	if(!entity)
	{
		VFLog(2, "VFDirEntity::ReadLink() failed: no entity");

		return ENOENT;
	}

	return entity->ReadLink(pid, tail);
}

int VFDirEntity::ReadDir(int index, dirent* de)
{
	VFLog(2, "VFDirEntity::DirStt() index %d", index);

	if(index >= map_.entries()) { return EINVAL; }

	EntityNamePair* pair = index_[index];
	assert(pair);

	assert(strlen(pair->name) <= NAME_MAX);
	strcpy(de->d_name, pair->name);
	pair->entity->Stat(&de->d_stat);

	return EOK;
}

int VFDirEntity::Insert(const String& path, VFEntity* entity)
{
	VFLog(2, "VFDirEntity::Insert() path \"%s\"", (const char *) path);

	assert(path != "");
	assert(entity);
	assert(path[0] != '/');

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	// no tail, so lead is the name of entity
	if(tail == "")
	{
		if(map_.contains(lead))
		{
			return EEXIST;
		}

		int i = map_.entries();

		map_[lead] = entity;
		index_[i]  = new EntityNamePair(entity, lead);

		info_.size = map_.entries();

		assert(map_[lead] == entity);
		assert(index_[i]->entity == entity);

		return EOK;
	}

	// there is a tail, so we need to insert into a subdirectory

	VFEntity* sub = 0;

	// auto-create the subdirectory to insert into, if necessary and possible
	if(!map_.contains(lead) && factory_)
	{
		sub = factory_->AutoDir();
		if(!sub) { return errno; }

		int e = Insert(lead, sub);

		if(e != EOK) {
			delete sub;
			return e;
		}
	}

	// find the subdirectory to insert into
	sub = map_.find(lead);
	if(!sub) {
		return ENOENT;
	}

	return sub->Insert(tail, entity);
}

void VFDirEntity::SplitPath(const String& path, String& lead, String& tail)
{
	int sep = path.index("/");
	if(sep == -1)
	{
		// chop everything
		lead = path;
		tail = "";
	}
	else
	{
		// chop lead to /
		lead = path(0, sep);
		tail = path(sep + 1, -1);
	}
}

void VFDirEntity::InitInfo(mode_t mode, uid_t uid, gid_t gid)
{
	memset(&info_, 0, sizeof info_);

	// strip type info from mode
	mode &= 0777;

	info_.mode = mode | S_IFDIR;

	if(uid != -1) { info_.uid = uid; }
	if(gid != -1) { info_.gid = gid; }
}

unsigned VFDirEntity::Hash(const String& key)
{
	const char* s = key;

	assert(s);

	return EntityMap::bitHash(s, strlen(s));
}

//
// VFDirOcb
//

VFDirOcb::VFDirOcb(VFDirEntity* dir) :
	dir_	(dir),
	index_	(0)
{
	VFLog(2, "VFDirOcb::VFDirOcb()");
}

VFDirOcb::~VFDirOcb()
{
	VFLog(3, "VFDirOcb::~VFDirOcb()");
}

int VFDirOcb::Write(pid_t pid, int nbytes, const void* data, int len)
{
	pid = pid, nbytes = nbytes, data = data, len = len;

	return ENOSYS;
}

int VFDirOcb::Read(pid_t pid, int nbytes)
{
	pid = pid, nbytes = nbytes;

	return ENOSYS;
}

int VFDirOcb::Seek(pid_t pid, int whence, off_t offset)
{
	pid = pid, whence = whence, offset = offset;

	return ENOSYS;
}

int VFDirOcb::Fstat(pid_t pid)
{
	VFLog(2, "VFDirOcb::Stat() pid %d", pid);

	return dir_->Fstat(pid);
}

int VFDirOcb::Chmod(pid_t pid, mode_t mode)
{
	return dir_->Chmod(pid, mode);
}

int VFDirOcb::Chown(pid_t pid, uid_t uid, gid_t gid)
{
	return dir_->Chown(pid, uid, gid);
}

int VFDirOcb::ReadDir(pid_t pid, int ndirs)
{
	pid = pid, ndirs = ndirs;

	VFLog(2, "VFDirOcb::ReadDir() pid %d ndirs %d index %d",
		pid, ndirs, index_);

	_io_readdir_reply reply;
	dirent	dirents[1]; // We'll do more than one later
	_mxfer_entry mx[1 + sizeof(dirents) / sizeof(dirent)];

	_setmx(&mx[0], &reply, sizeof(reply) - sizeof(reply.data));

	reply.status	= EOK;
	reply.ndirs		= 0;
	reply.zero[0]	= 0;
	reply.zero[1]	= 0;
	reply.zero[2]	= 0;
	reply.zero[3]	= 0;
	reply.zero[4]	= 0;

	int e = dir_->ReadDir(index_++, &dirents[0]);

	if(e == EINVAL)
	{
		Replymx(pid, 1, mx);
		return -1;
	}
	
	if(e != EOK) {
		return e;
	}

	reply.ndirs  = 1;

	_setmx(&mx[1], &dirents, reply.ndirs * sizeof(struct dirent));

	Replymx(pid, 2, mx);

	return -1;
}

int VFDirOcb::RewindDir(pid_t pid)
{
	pid = pid;

	VFLog(2, "VFDirOcb::RewindDir()");

	index_ = 0;

	return EOK;
}

