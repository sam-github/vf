//
// vf_tgzfile.h
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
// $Log$
// Revision 1.2  1999/11/25 03:56:16  sam
// prelimnary testing ok, but looks like a problem with String temps...
//
// Revision 1.1  1999/11/24 03:54:01  sam
// Initial revision
//

#ifndef VF_TGZFILE_H
#define VF_TGZFILE_H

#include <sys/types.h>
#include <vf_file.h>
#include <tar/tararch.h>

class VFTgzFileEntity : public VFFileEntity
{
public:
	VFTgzFileEntity(Tar::Reader& tar);
	~VFTgzFileEntity();

	int Read(pid_t pid, size_t nbytes, off_t* offset);

private:
	Tar::Reader& tar_;
	int		cache_;
	char*	path_;

	static char buffer_[];
};

#endif

