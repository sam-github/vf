//
// vf_tarfile.h
//
// Copyright (C) 1999 Sam Roberts
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
// Copyright (c) 1999, Sam Roberts
// 
// $Log$
// Revision 1.4  1999/08/09 15:17:56  sam
// Ported framework modifications down.
//
// Revision 1.3  1999/04/28 03:27:28  sam
// Stamped sources with the GPL.
//
// Revision 1.2  1999/04/24 04:41:46  sam
// added support for symbolic links, and assorted bugfixes
//
// Revision 1.1  1999/04/11 06:45:36  sam
// Initial revision
//

#ifndef VF_TARFILE_H
#define VF_TARFILE_H

#include <vf_file.h>

#include "tar_arch.h"

class VFTarFileEntity : public VFFileEntity
{
public:
	VFTarFileEntity(const TarArchive::iterator& file);
	~VFTarFileEntity();

	int Write(pid_t pid, size_t nbytes, off_t* offset, const void* data, int len);
	int Read(pid_t pid, size_t nbytes, off_t* offset);

private:
	TarArchive::iterator file_;

	static char buffer_[BUFSIZ];
};

#endif

