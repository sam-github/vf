//
// vf_file.h
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
// Revision 1.11  1999/12/05 01:50:24  sam
// replaced String with a custom Path class
//
// Revision 1.10  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
// Revision 1.9  1999/08/03 06:13:38  sam
// moved ram filesystem into its own subdirectory
//
// Revision 1.8  1999/07/11 11:26:46  sam
// Added arg to init stat to a particular type, defaults to reg file.
//
// Revision 1.7  1999/06/20 10:09:06  sam
// Made InitStat a protected member of the class that had the stat_ data.
//
// Revision 1.6  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.5  1999/04/24 04:38:39  sam
//  added support for symbolic links
//
// Revision 1.4  1999/04/11 06:41:37  sam
// split FileEntity into a RamFileEntity and a base FileEntity that is used by
// the FileOcb, turns out most of the FileEntity is reuseable, just the actual
// read/write functions were specific to the RAM filesystem
//
// Revision 1.3  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.2  1998/04/06 06:50:19  sroberts
// implemented creat(), and write()
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_FILE_H
#define VF_FILE_H

#include "vf.h"

class VFFileEntity : public VFEntity
{
public:
	VFFileEntity(pid_t pid, mode_t perm) :
		VFEntity(pid, S_IFREG | (perm & 0777)) {}

	VFFileEntity(uid_t uid, gid_t gid, mode_t perm) :
		VFEntity(uid, gid, S_IFREG | (perm & 0777)) {}

	int Open(pid_t pid, const Path& path, int fd, int oflag, mode_t mode);
	int Stat    (pid_t pid, const Path& path, int lstat);
	int ChDir   (pid_t pid, const Path& path);
	int ReadLink(pid_t pid, const Path& path);
	int MkSpecial(pid_t pid, const Path& path, mode_t mode, const char* linkto);

	int Stat	(struct stat* s) { return VFEntity::Stat(s); }
	int	Insert(const Path& path, VFEntity* entity);

	// API extensions for use by VFFileOcb
	virtual int Write(pid_t pid, size_t nbytes, off_t* offset,
		const void* data, int len);
	virtual int Read(pid_t pid, size_t nbytes, off_t* offset);
};

class VFFileOcb : public VFOcb
{
public:
	VFFileOcb(VFFileEntity* file);
	~VFFileOcb();

	int Write(pid_t pid, int nbytes, const void* data, int len);
	int Read(pid_t pid, int nbytes);
	int Seek(pid_t pid, int whence, off_t offset);
	int Fstat(pid_t pid);
	int Chmod(pid_t pid, mode_t mode);
	int Chown(pid_t pid, uid_t uid, gid_t gid);

	int ReadDir(pid_t pid, int ndirs);
	int RewindDir(pid_t pid);

private:
	VFFileEntity* file_;

	off_t offset_;
};

#endif

