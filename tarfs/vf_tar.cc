//
// vf_tar.cc
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
// Revision 1.4  1999/04/28 03:27:28  sam
// Stamped sources with the GPL.
//
// Revision 1.3  1999/04/24 04:54:43  sam
// default for -p is now the name of the file minus any extension
//
// Revision 1.2  1999/04/24 04:41:46  sam
// added support for symbolic links, and assorted bugfixes
//
// Revision 1.1  1999/04/11 06:45:36  sam
// Initial revision
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_syml.h>
#include <vf_log.h>

#include "vf_tarfile.h"
#include "tar_arch.h"

#ifdef __USAGE
%C - a tar archive virtual filesystem

usage: vf_tar [-hv] [-p vf] tarfile
    -h   print help message
    -v   increse the verbosity level
    -p   path of virtual file system to create, default is the base
         name of the file, without any extension
    -u   attempt to use the user and group of the tar archive member to find
         the local uid and gid

Mounts 'tarfile' as a virtual (read-only) filesystem at 'vf'.
#endif

int		vOpt	= 0;
char*	pathOpt	= 0;
int		uOpt	= 0;
char*	tarOpt	= 0;

VFEntity* VFTarEntityCreate(TarArchive::iterator& it)
{
	// attmempt to find the local user by name
	if(uOpt) {
		struct passwd* pw = getpwnam(it.User());
		struct group* gr = getgrnam(it.Group());

		it.ChUidGid(pw ? pw->pw_uid : -1, gr ? gr->gr_gid : -1);
	}

	VFEntity* entity = 0;
	struct stat* stat = it.Stat();

	if(S_ISREG(stat->st_mode)) {
		entity = new VFTarFileEntity(it);
	} else if(S_ISDIR(stat->st_mode)) {
		entity = new VFDirEntity(stat->st_mode, stat->st_uid, stat->st_gid);
	} else if(S_ISLNK(stat->st_mode)) {
		entity = new VFSymLinkEntity(
			it.Link(), stat->st_uid, stat->st_gid);
	} else {
		VFLog(1, "unsupported tar file type");
		return 0;
	}

	if(!entity) {
		VFLog(0, "%s", strerror(errno));
		exit(1);
	}
	return entity;
}

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:u")) != -1; )
	{
		switch(c)
		{
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			VFLevel("vf_tar", vOpt);
			break;

		case 'p':
			pathOpt = optarg;
			break;

		case 'u':
			uOpt = 1;
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind])) {
		fprintf(stderr, "no tarfile specified!\n");
		exit(1);
	}

	if(!pathOpt) {
		char path[_MAX_PATH];
		char ext[_MAX_EXT];
		_splitpath(tarOpt, 0, 0, path, ext);

		pathOpt = new char[strlen(path) + 1];
		strcpy(pathOpt, path);
	}

	return 0;
}

void main(int argc, char* argv[])
{
	VFManager* vfmgr = new VFManager;

	VFLevel("vf_tar", vOpt);

	GetOpts(argc, argv);

	TarArchive tar;

	if(!tar.Open(tarOpt)) {
		VFLog(0, "open tarfile %s failed: [%d] %s",
			tarOpt, tar.ErrorNo(), tar.ErrorString());
		exit(1);
	}

	VFDirEntity* root = new VFDirEntity(0555);

	for(TarArchive::iterator it = tar.begin(); it; ++it) {
		VFLog(3, "file %s", it.Path());

		if(vOpt >= 4) { it.DebugFile(stdout); }

		VFEntity* entity = VFTarEntityCreate(it);

		if(entity && !root->Insert(it.Path(), entity)) {
			VFLog(0, "inserting %s failed: [%d] %s",
				it.Path(), errno, strerror(errno));
			exit(1);
		}
	}

	if(!vfmgr->Init(root, pathOpt, vOpt))
	{
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	vfmgr->Run();
}

