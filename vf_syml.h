//
// vf_syml.h
//
// Copyright (c) 1999, Sam Roberts
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
// Revision 1.1  1999/04/24 04:38:39  sam
// Initial revision
//

#ifndef VF_SYML_H
#define VF_SYML_H

#include <sys/stat.h>

#include "vf.h"

class VFSymLinkEntity : public VFEntity
{
public:
	VFSymLinkEntity(pid_t pid, mode_t perm, const char* linkto);
	VFSymLinkEntity(uid_t uid, gid_t gid, mode_t perm, const char* linkto);

	int Open	(pid_t pid, const Path& path, int fd, int oflag, mode_t mode);
	int Stat	(pid_t pid, const Path& path, int lstat);
	int ChDir	(pid_t pid, const Path& path);
	int Unlink	(pid_t pid, const Path& path);
	int ReadLink(pid_t pid, const Path& path);
	int MkSpecial(pid_t pid, const Path& path, mode_t mode, const char* linkto);

	int Stat	(struct stat* s) { return VFEntity::Stat(s); }

	int Insert(const Path& path, VFEntity* entity);

private:
	char	linkto_[PATH_MAX + 1];

	int RewriteOpenPath(pid_t pid, const Path& path);
};

#endif

