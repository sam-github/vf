//
// vf_tarfile.cc
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

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/sendmx.h>
#include <unistd.h>
#include <time.h>

#include <vf_log.h>
#include <vf_file.h>

#include "vf_tarfile.h"

//
// VFTarFileEntity
//

char VFTarFileEntity::buffer_[BUFSIZ];

VFTarFileEntity::VFTarFileEntity(const TarArchive::iterator& file) :
	VFFileEntity(file.Stat()->st_uid, file.Stat()->st_gid, file.Stat()->st_mode),
	file_	(file)
{
	info_.size = file_.Stat()->st_size;
	info_.mtime = file_.Stat()->st_mtime;
	info_.atime = file_.Stat()->st_atime;
	info_.ctime = file_.Stat()->st_ctime;
}

VFTarFileEntity::~VFTarFileEntity()
{
	VFLog(3, "VFTarFileEntity::~VFTarFileEntity()");
}

int VFTarFileEntity::Write(pid_t pid, size_t nbytes, off_t* offset,
		const void* data, int len)
{
	pid = pid; nbytes = nbytes; offset = offset, data = data, len = len;

	VFLog(2, "VFTarFileEntity::Write()");

	return ENOTSUP;
}

int VFTarFileEntity::Read(pid_t pid, size_t nbytes, off_t* offset)
{
	VFLog(2, "VFTarFileEntity::Read() nbytes %ld, offset %ld", nbytes, *offset);

	// XXX need to loop to implement reads larger than BUFSIZ

	if(nbytes > sizeof(buffer_)) { nbytes = sizeof(buffer_); }

	file_.Seek(*offset);

	unsigned incr = file_.Read(buffer_, nbytes);

	if(incr == -1) {
		return file_.ErrorNo();
	}

	struct _io_read_reply reply;
	struct _mxfer_entry mx[2];

	reply.status = EOK;
	reply.nbytes = incr;
	reply.zero = 0;

	_setmx(mx + 0, &reply, sizeof(reply) - sizeof(reply.data));
	_setmx(mx + 1, buffer_, incr);

	if(Replymx(pid, 2, mx) != -1)
	{
		*offset += incr;
	}

	return -1;
}

