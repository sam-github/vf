//
// vf_dir.h
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
// Revision 1.9  1999/06/20 10:04:16  sam
// dir entity's factory now abstract and more modular
//
// Revision 1.8  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.7  1999/04/24 04:38:39  sam
//  added support for symbolic links
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

#ifndef VF_DIR_H
#define VF_DIR_H

#include <sys/stat.h>

#include <wchash.h>
#include <wcvector.h>

#include "vf.h"

class VFEntityFactory
{
public:
	virtual VFEntity* NewDir()
	{
		return ENotSup();
	}
	virtual VFEntity* NewSpecial(_fsys_mkspecial* req)
	{
		return ENotSup(req);
	}
	virtual VFEntity* NewFile(_io_open* req = 0)
	{
		return ENotSup(req);
	}

protected:
	VFEntity* ENotSup(void* v = 0)
	{
		v = v;

		errno = ENOTSUP;
		return 0;
	}
};

class VFDirEntity : public VFEntity
{
public:
	VFDirEntity(mode_t mode, uid_t uid = -1, gid_t gid = -1,
		VFEntityFactory* factory = 0);
	~VFDirEntity();

	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	int Stat(const String& path, _io_open* req, _io_fstat_reply* reply);
	int ChDir(const String& path, _io_open* req, _io_open_reply* reply);
	int Unlink();
	int MkSpecial(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply);
	int ReadLink(const String& path, _fsys_readlink* req, _fsys_readlink_reply* reply);

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

	VFEntityFactory* factory_;

	void SplitPath(const String& path, String& lead, String& tail);
	void InitStat(mode_t mode, uid_t uid = -1, gid_t gid = -1);

	// used by the WC hash dictionary
	static unsigned Hash(const String& key);
};

class VFDirOcb : public VFOcb
{
public:
	VFDirOcb(VFDirEntity* dir);
	~VFDirOcb();

	int Write(pid_t pid, _io_write* req, _io_write_reply* reply);
	int Read(pid_t pid, _io_read* req, _io_read_reply* reply);
	int Seek(pid_t pid, _io_lseek* req, _io_lseek_reply* reply);
	int Stat(pid_t pid, _io_fstat* req, _io_fstat_reply* reply);
	int Chmod();
	int Chown();

	int ReadDir(pid_t pid, _io_readdir* req, _io_readdir_reply* reply);
	int RewindDir(pid_t pid, _io_rewinddir* req, _io_rewinddir_reply* reply);

private:

	VFDirEntity* dir_;

	int readIndex_;
};

#endif

