//
// vf_fsys.h
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
// Revision 1.12  1999/08/03 05:10:54  sam
// *** empty log message ***
//
// Revision 1.11  1999/06/21 12:36:22  sam
// implemented sysmsg... version
//
// Revision 1.10  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.9  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.8  1999/04/24 21:13:28  sam
// implemented remove on top-level causing exit of vfsys
//
// Revision 1.7  1999/04/24 04:38:39  sam
//  added support for symbolic links
//
// Revision 1.6  1998/04/28 07:23:50  sroberts
// changed Handle() to Service() - the name was confusing me - and added
// readable system message names to debug output
//
// Revision 1.5  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.4  1998/04/06 06:47:47  sroberts
// implimented dup() and write()
//
// Revision 1.3  1998/04/05 23:51:21  sroberts
// added mkdir() support
//
// Revision 1.2  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_MGR_H
#define VF_MGR_H

#include "vf.h"

// temporary for OcbMap
#include <unistd.h>
#include <sys/fd.h>

union VFIoMsg
{
	msg_t	type;
	msg_t	status;

	struct _io_open 				open;
	struct _io_open_reply 			open_reply;

	struct _io_close 				close;
	struct _io_close_reply 			close_reply;

	struct _fsys_mkspecial			mkspec;
	struct _fsys_mkspecial_reply	mkspec_reply;

	struct _fsys_readlink			rdlink;
	struct _fsys_readlink_reply		rdlink_reply;

	struct _io_dup 					dup;
	struct _io_dup_reply 			dup_reply;

	struct _io_write 				write;
	struct _io_write_reply 			write_reply;

	struct _io_read 				read;
	struct _io_read_reply 			read_reply;

	struct _io_lseek 				seek;
	struct _io_lseek_reply 			seek_reply;

	struct _io_fstat				fstat;
	struct _io_fstat_reply			fstat_reply;

	struct _io_readdir 				readdir;
	struct _io_readdir_reply	 	readdir_reply;

	struct _io_rewinddir 			rewinddir;
	struct _io_rewinddir_reply 		rewinddir_reply;

	struct _io_open					remove;
	struct _io_open_reply			remove_reply;

	//struct _io_chmod 			
	//struct _io_chmod_reply 			
	//struct _io_chown 			
	//struct _io_chown_reply 			
	//struct _io_utime 			
	//struct _io_utime_reply 			
	//struct _io_lock 			
	//struct _io_config 			
	//struct _io_config_reply 			
	//struct _io_flags 			
	//struct _io_flags_reply 			
	//struct _io_ioctl 			
	//struct _io_ioctl_reply 			
	//struct _io_qioctl 			
	//struct _io_qioctl_reply 			
	//struct _select_set 			
	//struct _io_select 			
	//struct _io_select_reply 			

	// the system message structure is lame, build a better one here
	struct _sysmsg {
		struct _sysmsg_hdr	hdr;
		union {
			struct _sysmsg_signal	signal;
			struct _sysmsg_version	version;
		} body;
	};

	struct _sysmsg_reply {
		struct _sysmsg_hdr_reply	hdr;
		union {
			struct _sysmsg_version_reply	version;
		} body;
	};

	struct _sysmsg					sysmsg;
	struct _sysmsg_reply			sysmsg_reply;

	// reserve space because struct _io_open and such have buffers of
	// unspecified length as members
	char reserve [4096 + 2*sizeof(_io_read)];
};

struct VFVersion : public _sysmsg_version_reply
{
	VFVersion(
		const char* name_,
		float version_ = 1.00, char letter_ = 'A',
		const char* date_ = __DATE__)
	{
		strncpy(name, name_, sizeof(name));
		strncpy(date, date_, sizeof(date));

		version = (short unsigned)(version_*100);

		letter = letter_;

		more = 0;
	}
};

class OcbMap;

class VFManager
{
public:
	VFManager(const VFVersion& version);

	Init(VFEntity *root, const char* mount, int verbosity = 1);

	virtual int Service(pid_t pid, VFIoMsg* msg);

	void Run();

	static const char* MessageName(msg_t type);
	static const char* HandleOflagName(short int oflag);
	static const char* SysmsgSubtypeName(short unsigned subtype);

private:
	VFOcbMap* ocbMap_;

	VFIoMsg msg_;

	VFEntity*   root_;
	const char* mount_;

	const VFVersion&	version_;
};

#endif

