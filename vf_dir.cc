//
// vf_dir.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/fsys.h>
#include <time.h>

#include "vf_log.h"
#include "vf_dir.h"

//
// VFDirOcb
//

VFDirEntity::VFDirEntity() :
	map_(&Hash)
{
	memset(&stat_, 0, sizeof stat_);

	stat_.st_mode = 0555 | S_IFDIR; // r-xr-xr-x and is a directory
	stat_.st_nlink = 1;

	stat_.st_ouid = getuid();
	stat_.st_ogid = getgid();

	stat_.st_ftime =
	  stat_.st_mtime =
	    stat_.st_atime =
	      stat_.st_ctime = time(0);

}

// need another ctor that takes a stat as an arg

VFDirEntity::~VFDirEntity()
{
	VFLog(3, "VFDirEntity::~VFDirEntity()");
}

VFOcb* VFDirEntity::Open(
	const String& path,
	_io_open* open,
	_io_open_reply* reply)
{
	VFLog(2, "VFDirEntity::Open(\"%s\")", (const char *) path);

	// check permissions

	if(path == "") // open the directory
	{
		reply->status = EOK;
		return new VFDirOcb(this);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_[lead];

	if(!entity)
	{
		VFLog(2, "VFDirEntity::Open() failed: no entity");
		// could create if O_CREAT was spec'd in open flags

		reply->status = ENOENT;
		return 0;
	}

	return entity->Open(tail, open, reply);
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


	if(!map_.contains(lead))
	{
		VFLog(2, "VFDirEntity::Stat() failed: no entity");
		reply->status = ENOENT;
		return sizeof(reply->status);
	}

	VFEntity* entity = map_[lead];
	return entity->Stat(tail, req, reply);
}

int VFDirEntity::Chdir(
	const String& path,
	_io_open* open,
	_io_open_reply* reply)
{
	VFLog(2, "VFDirEntity::Chdir(\"%s\")", (const char *) path);

	if(path == "")
	{
		// check permissions...

		reply->status = EOK;

		return sizeof(reply->status);
	}

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	VFEntity* entity = map_[lead];

	if(!entity)
	{
		VFLog(2, "VFDirEntity::Chdir() failed: no entity");

		reply->status = ENOENT;
		return 0;
	}

	return entity->Chdir(tail, open, reply);
}


int VFDirEntity::Unlink()
{
	return 0;
}

bool VFDirEntity::Insert(const String& path, VFEntity* entity)
{
	VFLog(2, "VFDirEntity::Insert(\"%s\")", (const char *) path);

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

	// create the subdirectory if we need to
	if(map_.contains(lead))
	{
		sub = map_[lead];
		assert(sub);
	}
	else
	{
		sub = new VFDirEntity;
		if(!sub)
		{
			errno = ENOMEM;
			return false;
		}
		int i = map_.entries();

		map_[lead] = sub;
		index_[i]  = new EntityNamePair(entity, lead);

		stat_.st_size = map_.entries();

		assert(map_[lead] == sub);
		assert(index_[i]->entity == entity);
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

int VFDirOcb::Close()
{
	return 0;
}

int VFDirOcb::Stat()
{
	return 0;
}

int VFDirOcb::Read()
{
	return 0;
}

int VFDirOcb::Write()
{
	return 0;
}

int VFDirOcb::Seek()
{
	return 0;
}

int VFDirOcb::Chmod()
{
	return 0;
}

int VFDirOcb::Chown()
{
	return 0;
}

int VFDirOcb::ReadDir(_io_readdir* req, _io_readdir_reply* reply)
{
	int entries = dir_->map_.entries();

	VFLog(2, "VFDirOcb::ReadDir() index %d entries %d", readIndex_, entries);

	reply->status = EOK;

	if(readIndex_ == entries)
	{
		reply->ndirs  = 0;
		return sizeof(*reply);
	}
	
	VFDirEntity::EntityNamePair* pair = dir_->index_[readIndex_++];
	assert(pair);

	reply->ndirs  = 1;
	memcpy(&reply->data[0], pair->entity->Stat(), sizeof(struct stat));
	strcpy(&reply->data[sizeof(struct stat)], pair->name);

	VFLog(2, "VFDirOcb::ReadDir() name %s", &reply->data[sizeof(struct stat)]);

	return sizeof(*reply) + sizeof(struct dirent);
}

int VFDirOcb::RewindDir(_io_rewinddir* req, _io_rewinddir_reply* reply)
{
	VFLog(2, "VFDirOcb::RewindDir()");

	readIndex_ = 0;

	reply->status = EOK;

	return sizeof(*reply);
}

