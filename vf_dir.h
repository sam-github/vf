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
// Revision 1.13  1999/12/05 01:50:24  sam
// replaced String with a custom Path class
//
// Revision 1.12  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.11  1999/07/02 15:29:25  sam
// added public accessors, information is useful to vf_tar
//
// Revision 1.10  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
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
	virtual VFEntity* AutoDir()
	{
		return ENotSup();
	}
	virtual VFEntity* MkSpecial(pid_t pid, mode_t mode, const char* linkto)
	{
		switch(S_IFMT & mode)
		{
		case S_IFIFO:	return Fifo	(pid, mode);
		case S_IFDIR:	return Dir	(pid, mode);
		case S_IFLNK:	return Link	(pid, mode, linkto);
		case S_IFREG:	return File	(pid, mode);
			// This last isn't used by the std library, it uses Open(..O_CREAT..),
			// but I support it as an extension.
		default:
			errno = EINVAL;
			return 0;
		}
	}
	virtual VFEntity* Fifo(pid_t pid, mode_t mode)
	{
		pid = pid, mode = mode;

		return ENotSup();
	}
	virtual VFEntity* Link(pid_t pid, mode_t mode, const char* linkto)
	{
		pid = pid, mode = mode, linkto = linkto;

		return ENotSup();
	}
	virtual VFEntity* Dir(pid_t pid, mode_t mode)
	{
		pid = pid, mode = mode;

		return ENotSup();
	}
	virtual VFEntity* File(pid_t pid, mode_t mode)
	{
		pid = pid, mode = mode;

		return ENotSup();
	}

protected:
	VFEntity* ENotSup()
	{
		errno = ENOTSUP;
		return 0;
	}
};

class VFDirEntity : public VFEntity
{
public:
	VFDirEntity(pid_t pid, mode_t mode, VFEntityFactory* factory = 0);
	VFDirEntity(uid_t uid, gid_t gid, mode_t mode, VFEntityFactory* factory = 0);

	~VFDirEntity();

	int Open	(pid_t pid, const Path& path, int fd, int oflag, mode_t mode);
	int Stat	(pid_t pid, const Path& path, int lstat);
	int ChDir	(pid_t pid, const Path& path);
	int ReadLink(pid_t pid, const Path& path);
	int MkSpecial(pid_t pid, const Path& path, mode_t mode, const char* linkto);

	// fd-based services supported by directories
	int		ReadDir	(int index, struct dirent* de);

	// Other
	int Insert(const Path& path, VFEntity* entity);

	// Public Accessors (unused by vf framework)
	int Entries () const
	{
		return map_.entries();
	}

	struct EntityNamePair;

	EntityNamePair* EnPair(int index)
	{
		if(index >= map_.entries()) { return 0; }

		return index_[index];
	}

    // position -> entity/name map
	struct EntityNamePair
	{
		// default ctor required by Watcom vector
		EntityNamePair() : entity(0) { }
		EntityNamePair(VFEntity* entity_, const Path& name_) :
			entity(entity_), name(name_)
		{
		}

		VFEntity* entity;
		Path name;
	};

private:
	friend class VFDirOcb;

	// Define our wrapper to avoid op[] adding null entries to the map,
	// and so that we have a find that behaves the way I want.
	class EntityMap : public WCValHashDict<Path, VFEntity*>
	{
		public:
			EntityMap(unsigned (*hashfn)(const Path&)) :
				WCValHashDict<Path,VFEntity*>(hashfn)
			{}
			VFEntity* find(const Path& name) const
			{
				VFEntity* entity = 0;
				WCValHashDict<Path,VFEntity*>::find(name, entity);
				return entity;
			}
	};
	typedef WCPtrVector<EntityNamePair> EntityIndex;

	EntityMap   map_;
	EntityIndex index_;

	VFEntityFactory* factory_;

	void SplitPath(const Path& path, Path& lead, Path& tail);
	void InitInfo(mode_t mode, uid_t uid = -1, gid_t gid = -1);

	// used by the WC hash dictionary
	static unsigned Hash(const Path& key);
};

class VFDirOcb : public VFOcb
{
public:
	VFDirOcb(VFDirEntity* dir);
	~VFDirOcb();

	int Write	(pid_t pid, int nbytes, const void* data, int len);
	int Read	(pid_t pid, int nbytes);
	int Seek	(pid_t pid, int whence, off_t offset);
	int Fstat	(pid_t pid);
	int Chmod	(pid_t pid, mode_t mode);
	int Chown	(pid_t pid, uid_t uid, gid_t gid);

	int ReadDir(pid_t pid, int ndirs);
	int RewindDir(pid_t pid);

private:
	VFDirEntity* dir_;

	int index_;
};

#endif

