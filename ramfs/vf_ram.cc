//
// vf_ram.cc
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
// Revision 1.9  1999/06/21 12:37:27  sam
// implemented sysmsg... version
//
// Revision 1.8  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.7  1999/06/20 10:04:16  sam
// dir entity's factory now abstract and more modular
//
// Revision 1.6  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.5  1999/04/24 04:37:06  sam
// added support for symbolic links
//
// Revision 1.4  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.3  1998/04/05 23:53:13  sroberts
// added mkdir() support, so don't create my own directories any more
//
// Revision 1.2  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "vf_mgr.h"
#include "vf_dir.h"
#include "vf_file.h"
#include "vf_syml.h"

#ifdef __USAGE
%C - an in-memory virtual filesystem

usage: vf_ram [-hv] [-p vf]
    -h   print help message
    -v   increse the verbosity level
    -p   path of virtual file system to create

Mounts a RAM filesystem at 'vf'. It is intended to be fully functional, but
is primarily a test of the virtual filesystem framework.
#endif

//
// VFRamEntityFactory
//

class VFRamEntityFactory : public VFEntityFactory
{
public:
	VFEntity* NewSpecial(_fsys_mkspecial* req);
	VFEntity* NewFile(_io_open* req);
};

VFEntity* VFRamEntityFactory::NewSpecial(_fsys_mkspecial* req)
{
	VFEntity* entity = 0;

	assert(req);

	if(S_ISDIR(req->mode)) {
		entity = new VFDirEntity(req->mode, -1, -1, this);
	} else if(S_ISLNK(req->mode)) {
		const char* pname = &req->path[0] + PATH_MAX + 1;
		entity = new VFSymLinkEntity(pname);
	} else {
		errno = ENOTSUP;
		return 0;
	}

	if(!entity) { errno = ENOMEM; }

	return entity;
}

VFEntity* VFRamEntityFactory::NewFile(_io_open* req)
{
	VFEntity* entity = new VFRamFileEntity(req->mode);
	if(!entity) { errno = ENOMEM; }
	return entity;
}

//
// vf_ram: globals
//

int		vOpt = 0;
char*	pathOpt = "ram";


int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:")) != -1; )
	{
		switch(c)
		{
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			break;

		case 'p':
			pathOpt = optarg;
			break;

		default:
			exit(1);
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	GetOpts(argc, argv);

	VFEntity* root = new VFDirEntity(0555, -1, -1, new VFRamEntityFactory);

	VFVersion vfver("vf_ram", 0.02);

	VFManager vfmgr(vfver);

	if(!vfmgr.Init(root, pathOpt, vOpt))
	{
		printf("VFManager init failed: [%d] %s\n",
			errno, strerror(errno)
			);
		exit(1);
	}

	vfmgr.Run();

	assert(0);

	return 0;
}

