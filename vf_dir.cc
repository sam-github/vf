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
#include <sys/psinfo.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "vf_log.h"
#include "vf_dir.h"

//
// VFDirEntity
//

VFDirEntity::VFDirEntity(mode_t mode, uid_t uid, gid_t gid, VFEntityFactory* factory) :
	map_(&Hash),
	factory_(factory)
{
	InitStat(mode, uid, gid);
}

// need another ctor that takes a stat as an arg

VFDirEntity::~VFDirEntity()
{
	VFLog(3, "VFDirEntity::~VFDirEntity()");
}

VFOcb* VFDirEntity::Open(
	const String& path,
	_io_open* req,
	_io_open_reply* reply)
{
	VFLog(2, "VFDirEntity::Open() fd %d path \"%s\"",
		req->fd, (const char *) path);

	// check permissions...

	if(path == "") // open the directory
	{
		reply->status = EOK;
		return new VFDirOcb(this);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	// if there is not an entity, perhaps create one
	if(!entity && tail == "" && factory_ && (req->oflag & O_CREAT))
	{
		entity = factory_->NewFile(req);
		if(!entity || !Insert(lead, entity))
		{
			VFLog(2, "VFDirEntity::Open() create failed: %s", strerror(errno));
			reply->status = errno;
			delete entity;
			return 0;
		}
	}

	if(entity)
	{
		return entity->Open(tail, req, reply);
	}

	VFLog(2, "VFDirEntity::Open() failed: no entity");

	reply->status = ENOENT;
	return 0;
}

int VFDirEntity::Stat(
	const String& path,
	_io_open* req,
	_io_fstat_reply* reply)
{
	VFLog(2, "VFDirEntity::Stat(\"%s\")", (const char *) path);

	req = req; reply = reply;

	if(path == "")
	{
		reply->status = EOK;
		reply->zero = 0;
		reply->stat   = stat_;
		return sizeof(*reply);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);


	VFEntity* entity = map_.find(lead);
	if(!entity)
	{
		VFLog(2, "VFDirEntity::Stat() failed: no entity");
		reply->status = ENOENT;
		return sizeof(reply->status);
	}

	return entity->Stat(tail, req, reply);
}

int VFDirEntity::ChDir(
	const String& path,
	_io_open* open,
	_io_open_reply* reply)
{
	VFLog(2, "VFDirEntity::ChDir(\"%s\")", (const char *) path);

	if(path == "")
	{
		// check permissions...

		reply->status = EOK;

		return sizeof(reply->status);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	if(!entity)
	{
		VFLog(2, "VFDirEntity::ChDir() failed: no entity");

		reply->status = ENOENT;
		return 0;
	}

	return entity->ChDir(tail, open, reply);
}

int VFDirEntity::Unlink()
{
	return 0;
}

int VFDirEntity::MkSpecial(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply)
{
	VFLog(2, "VFDirEntity::MkSpecial(\"%s\")", (const char *) path);

	if(path == "")
	{
		reply->status = ENOENT;	// so it is written in creat(3)
		return 0;
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
			reply->status = ENOENT;
			return 0;
		}
		return entity->MkSpecial(tail, req, reply);
	}

	// this is where special is to be made
	if(!factory_)
	{
		reply->status = ENOTSUP;
		return 0;
	}
	
	VFEntity* entity = factory_->NewSpecial(req);

	if(!entity)
	{
		reply->status = errno;
	}
	else if(!Insert(lead, entity))
	{
		reply->status = errno;
	}
	else
	{
		reply->status = EOK;
	}

	if(reply->status != EOK)
	{
		VFLog(2, "VFDirEntity::MkSpecial() failed: [%d] %s",
			reply->status, strerror(reply->status));

		delete entity;
	}

	return sizeof(reply->status);
}

int VFDirEntity::ReadLink(
	const String& path,
	_fsys_readlink* req,
	_fsys_readlink_reply* reply)
{
	VFLog(2, "VFDirEntity::ReadLink(\"%s\")", (const char *) path);

	if(path == "")
	{
		reply->status = EINVAL;

		return sizeof(reply->status);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_.find(lead);

	if(!entity)
	{
		VFLog(2, "VFDirEntity::ReadLink() failed: no entity");

		reply->status = ENOENT;
		return 0;
	}

	return entity->ReadLink(tail, req, reply);
}

bool VFDirEntity::Insert(const String& path, VFEntity* entity)
{
	VFLog(2, "VFDirEntity::Insert(\"%s\")", (const char *) path);

	assert(path != "");
	assert(entity);

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	// no tail, so lead is the name of entity
	if(tail == "")
	{
		if(map_.contains(lead))
		{
			errno = EEXIST;
			return false;
		}

		int i = map_.entries();

		map_[lead] = entity;
		index_[i]  = new EntityNamePair(entity, lead);

		stat_.st_size = map_.entries();

		assert(map_[lead] == entity);
		assert(index_[i]->entity == entity);

		return true;
	}

	// there is a tail, so we need to insert into a subdirectory

	VFEntity* sub = 0;

	// auto-create the subdirectory to insert into, if necessary and possible
	if(!map_.contains(lead) && factory_)
	{
		sub = factory_->AutoCreateDirectory();

		if(sub && !Insert(lead, sub)) {
			delete sub;
			return false;
		}
	}

	// find the subdirectory to insert into
	sub = map_.find(lead);
	if(!sub) {
		errno = ENOENT;
		return false;
	}

	return sub->Insert(tail, entity);
}

struct stat* VFDirEntity::Stat()
{
	return &stat_;
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

void VFDirEntity::InitStat(mode_t mode, uid_t uid, gid_t gid)
{
	memset(&stat_, 0, sizeof stat_);

	// stip type info from mode
	mode &= 0777;

	stat_.st_mode = mode | S_IFDIR;
	stat_.st_nlink = 1;

	stat_.st_ouid = uid != -1 ? uid :getuid();
	stat_.st_ogid = gid != -1 ? gid :getgid();

	stat_.st_ftime =
	  stat_.st_mtime =
	    stat_.st_atime =
	      stat_.st_ctime = time(0);

}

unsigned VFDirEntity::Hash(const String& key)
{
	const char* s = key;

	return EntityMap::bitHash(s, strlen(s));
}

//
// VFDirOcb
//

VFDirOcb::VFDirOcb(VFDirEntity* dir) :
	dir_       (dir),
	readIndex_ (0)
{
	VFLog(2, "VFDirOcb::VFDirOcb()");
}

VFDirOcb::~VFDirOcb()
{
	VFLog(3, "VFDirOcb::~VFDirOcb()");
}

int VFDirOcb::Write(pid_t pid, _io_write* req, _io_write_reply* reply)
{
	pid = pid, req = req;
	reply->status = ENOSYS;
	return sizeof(reply->status);
}

int VFDirOcb::Read(pid_t pid, _io_read* req, _io_read_reply* reply)
{
	pid = pid, req = req, reply = reply;

	return 0;
}

int VFDirOcb::Seek(pid_t pid, _io_lseek* req, _io_lseek_reply* reply)
{
	pid = pid, req = req, reply = reply;

	return 0;
}

int VFDirOcb::Stat(pid_t pid, _io_fstat* req, _io_fstat_reply* reply)
{
	VFLog(2, "VFDirOcb::Stat() pid %d fd %d", pid, req->fd);

	struct stat* stat = dir_->Stat();
	assert(stat);

	// yeah, I know I should do the Replymx thing to avoid memcpys...
	reply->status = EOK;
	reply->zero = 0;
	reply->stat = *stat;

	return sizeof(*reply);
}

int VFDirOcb::Chmod()
{
	return 0;
}

int VFDirOcb::Chown()
{
	return 0;
}

int VFDirOcb::ReadDir(pid_t pid, _io_readdir* req, _io_readdir_reply* reply)
{
	pid = pid, req = req;

	int entries = dir_->map_.entries();

	VFLog(2, "VFDirOcb::ReadDir() index %d entries %d", readIndex_, entries);

	reply->status = EOK;

	reply->zero[0] = 0; // should use a memset
	reply->zero[1] = 0;
	reply->zero[2] = 0;
	reply->zero[3] = 0;
	reply->zero[4] = 0;

	if(readIndex_ == entries)
	{
		reply->ndirs  = 0;
		return sizeof(*reply);
	}
	
	VFDirEntity::EntityNamePair* pair = dir_->index_[readIndex_++];
	assert(pair);

	reply->ndirs  = 1;
	struct dirent* dirent = (struct dirent*) &reply->data[0];

	strcpy(dirent->d_name, pair->name); // should probably use strncpy()
	dirent->d_stat = *pair->entity->Stat();
	dirent->d_stat.st_status |= _FILE_USED;

	VFLog(2, "VFDirOcb::ReadDir() name %s", &reply->data[sizeof(struct stat)]);

	return sizeof(*reply) - sizeof(reply->data) + sizeof(struct dirent);
}

int VFDirOcb::RewindDir(pid_t pid, _io_rewinddir* req, _io_rewinddir_reply* reply)
{
	pid = pid, req = req, reply = reply;

	VFLog(2, "VFDirOcb::RewindDir()");

	readIndex_ = 0;

	reply->status = EOK;

	return sizeof(*reply);
}

