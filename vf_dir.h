//
// vf_dir.h
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

#ifndef VF_DIR_H
#define VF_DIR_H

#include <sys/stat.h>

#include <wchash.h>
#include <wcvector.h>

#include "vf.h"

class VFDirEntity : public VFEntity
{
public:
	VFDirEntity();
	~VFDirEntity();

	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	int Stat(const String& path, _io_open* req, _io_fstat_reply* reply);
	int Chdir(const String& path, _io_open* req, _io_open_reply* reply);
	int Unlink();

	bool Insert(const String& path, VFEntity* entity);
	struct stat* Stat();

private:
	friend class VFDirOcb;

    // position -> entity/name map
	struct EntityNamePair
	{
		// default ctor required by Watcom vector
		EntityNamePair() : entity(0) { }
		EntityNamePair(VFEntity* entity_, const String& name_) :
			entity(entity_), name(name_)
		{
		}

		VFEntity* entity;
		String name;
	};

	typedef WCValHashDict<String, VFEntity*> EntityMap;
	typedef WCPtrVector<EntityNamePair> EntityIndex;

	EntityMap   map_;
	EntityIndex index_;

	struct stat stat_;

	void SplitPath(const String& path, String& lead, String& tail);

	// used by the WC hash dictionary
	static unsigned Hash(const String& key);
};

class VFDirOcb : public VFOcb
{
public:
	VFDirOcb(VFDirEntity* dir);
	~VFDirOcb();

	int Close();
	int Stat();
	int Read();
	int Write();
	int Seek();
	int Chmod();
	int Chown();

	int ReadDir(_io_readdir* req, _io_readdir_reply* reply);
	int RewindDir(_io_rewinddir* req, _io_rewinddir_reply* reply);

private:

	VFDirEntity* dir_;

	int readIndex_;
};

#endif

