//
// vf_dir.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.2  1998/03/15 22:07:49  sroberts
// implemented insertion
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <stdio.h>
#include <string.h>

#include "vf_log.h"
#include "vf_dir.h"


VFDirEntity::VFDirEntity() :
	entityMap_(&Hash)
{
}

VFOcb* VFDirEntity::Open(
	const String& path,
	_io_open* open,
	_io_open_reply* open_reply)
{
	VFLog(2, "VFDirEntity::Open(%s)", (const char *) path);

	open = open; open_reply = open_reply;

	return 0;
}

struct stat* VFDirEntity::Stat(
	const String& path,
	_io_open* open,
	_io_open_reply* open_reply)
{
	VFLog(2, "VFDirEntity::Stat(%s)", (const char *) path);

	return 0;
}

int VFDirEntity::Chdir()
{
	return 0;
}

int VFDirEntity::Unlink()
{
	return 0;
}

bool VFDirEntity::Insert(const String& path, VFEntity* entity)
{
	VFLog(2, "VFDirEntity::Insert(%s)", (const char *) path);

	String lead;
	String tail;
	SplitPath(path, lead, tail);

	// no tail, so lead is the name of entity
	if(tail == "")
	{
		if(entityMap_.contains(lead))
		{
			errno = EEXIST;
			return false;
		}

		entityMap_[lead] = entity;

		assert(entityMap_[lead] == entity);

		return true;
	}
	// there is a tail, so we need to insert into a subdirectory

	VFEntity* sub = 0;

	// create the subdirectory if we need to
	if(entityMap_.contains(lead))
	{
		sub = entityMap_[lead];
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
		entityMap_[lead] = sub;
		assert(entityMap_[lead] == sub);
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

unsigned VFDirEntity::Hash(const String& key)
{
	const char* s = key;

	return EntityMap::bitHash(s, strlen(s));
}

