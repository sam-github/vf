//
// vf.h
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
// Revision 1.12  1999/10/17 16:22:41  sam
// Reinstalled a clean Watcom10.6, and string.hpp is String.h now.
//
// Revision 1.11  1999/08/09 15:29:29  sam
// distinct inode numbers implemented, now find works
//
// Revision 1.10  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.9  1999/06/21 12:36:22  sam
// implemented sysmsg... version
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
// Revision 1.5  1998/04/06 06:48:14  sroberts
// implemented write()
//
// Revision 1.4  1998/04/05 23:54:00  sroberts
// added mkdir() support
//
// Revision 1.3  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.2  1998/03/15 22:07:03  sroberts
// added definitions for VFOcb and VFEntity
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_H
#define VF_H

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fsys_msg.h>
#include <sys/io_msg.h>
#include <sys/sys_msg.h>

#include "vf_path.h"

// forward references
class VFEntity;
class VFOcb;
class VFManager;
class VFFdMap;

struct VFInfo
{
	VFInfo(pid_t pid, mode_t mode) :
		ino(INo()),
		dev(DevNo()),
		mode(mode),
		size(0),
		rdev(0),
		uid(getuid()),
		gid(getgid()),
		mtime(time(0)),
		atime(time(0)),
		ctime(time(0)),
		nlink(0)
		{ pid = pid; /* should get uid,gid from this pid */ }

	VFInfo(uid_t uid, gid_t gid, mode_t mode) :
		ino(INo()),
		dev(DevNo()),
		mode(mode),
		size(0),
		rdev(0),
		uid(uid),
		gid(gid),
		mtime(time(0)),
		atime(time(0)),
		ctime(time(0)),
		nlink(0)
		{}


	void Stat(struct stat* s) const
	{
		memset(s, 0, sizeof(*s));
		s->st_ino	= ino;
		s->st_dev	= dev;
		s->st_mode	= mode;
		s->st_size	= size;
		s->st_rdev	= rdev;
		s->st_uid	= s->st_ouid = uid;
		s->st_gid	= s->st_ogid = gid;
		s->st_mtime	= mtime;
		s->st_atime	= atime;
		s->st_ctime	= ctime;
		s->st_ftime	= ctime;
		s->st_nlink	= nlink;
		s->st_status = _FILE_USED; // used by readdir()
	}

	ino_t	ino;
	dev_t	dev;
	mode_t	mode;
	off_t	size;
	dev_t	rdev;
	uid_t	uid;
	gid_t	gid;
	time_t	mtime,
			atime,
			ctime;
	nlink_t	nlink;

	static ino_t INo();
	static dev_t DevNo();
};

class VFEntity
{
public:
	VFEntity	(pid_t pid, mode_t mode) :
					info_(pid, mode) {}
	VFEntity	(uid_t uid, gid_t gid, mode_t mode) :
					info_(uid, gid, mode) {}

	virtual ~VFEntity() = 0;

	// path-based calls
	virtual int	Open(pid_t pid, const Path& path, int fd, int oflag, mode_t mode) = 0;
//	virtual int Handle(pid_t pid, const Path& path, int oflag, mode_t mode, int eflag) = 0;
	virtual int	Stat	(pid_t pid, const Path& path, int lstat) = 0;
	virtual int	ChDir	(pid_t pid, const Path& path) = 0;
	virtual int	Unlink	(pid_t pid, const Path& path) /* XXX = 0*/;
	virtual int	ReadLink(pid_t pid, const Path& path) = 0;
	virtual int	MkSpecial(pid_t pid, const Path& path, mode_t mode, const char* linkto) = 0;

	virtual int	Chmod   (pid_t pid, mode_t mode);
	virtual int	Chown   (pid_t pid, uid_t uid, gid_t gid);
	virtual int	Fstat   (pid_t pid);
	virtual int	Stat    (struct stat* s);

	virtual int	Insert(const Path& path, VFEntity* entity) = 0;

	// Helper methods
	int ReplyStatus	(pid_t pid, int status);
	int ReplyInfo	(pid_t pid, const struct VFInfo* s = 0);
	int	ReplyMsg	(pid_t pid, const void* msg, size_t size);
	int	ReplyMx		(pid_t pid, unsigned len, struct _mxfer_entry* mx);
	int FdAttach	(pid_t pid, int fd, VFOcb* ocb);

	VFInfo*			Info() { return &info_; }

protected:

	// stat summary
	VFInfo info_;
};

class VFOcb
{
public:
	VFOcb();
	virtual ~VFOcb() = 0;

	virtual int Write(pid_t pid, int nbytes, const void* data, int len) = 0;
	virtual int Read(pid_t pid, int nbytes) = 0;
	virtual int Seek(pid_t pid, int whence, off_t offset) = 0;
	virtual int Fstat(pid_t pid) = 0;
	virtual int Chmod(pid_t pid, mode_t mode) = 0;
	virtual int Chown(pid_t pid, uid_t uid, gid_t gid) = 0;

	virtual int ReadDir(pid_t pid, int ndirs) = 0;
	virtual int RewindDir(pid_t pid) = 0;

private:
	friend class VFFdMap;

	void Refer() { refCount_++; }
	void Unfer() { refCount_--; if(refCount_ <= 0) delete this; }
		// should we call Close()? or is that the dtor's job

	int refCount_;
};

#endif

