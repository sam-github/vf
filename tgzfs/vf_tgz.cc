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
// Revision 1.4  2000/01/13 02:31:29  sam
// updated usage, and fixed bug with absolute tar paths
//
// Revision 1.3  1999/12/05 06:11:45  sam
// converted to use Path instead of String
//
// Revision 1.2  1999/11/25 03:56:16  sam
// prelimnary testing ok, but looks like a problem with String temps...
//
// Revision 1.1  1999/11/24 03:54:01  sam
// Initial revision
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <strstream.h>

#include <pipestream.h>

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_file.h>
#include <vf_syml.h>
#include <vf_log.h>

#include <tar/tararch.h>

#include "vf_tgzfile.h"

#ifdef __USAGE
%C - mounts a compressed tar archive as a virtual filesystem

usage: vf_tgz [-hvd] [-F filter] file [vf]
    -h   Print this helpful message.
    -v   Increase the verbosity level.
    -d   Don't become a daemon, default is to fork into the background after
         reading the tar file.
    -F   Filter used to decompress the file (default is to guess using
         the same techniques as the 'file' utility, searching for magic
         bytes - so the name of the file is irrelevant). It is called
         as 'filter <file> | ...'.

Mounts 'file' as a virtual (read-only) filesystem at 'vf'. It can be
unmounted in the standard way, causing the virtual filesystem manager
to exit.

If there is only a single top-level directory in the tar archive, and
'vf' was not explicitly specified, then that directory is elided, and
it's name is used as the value of 'vf'. Otherwise, the base name of
the archive, minus an extension, is used as 'vf'.

This heuristic is an attempt at dealing with the common case of archived
sub-directories, such as "yoyo-widgets-1.3.4/..." by stripping the single
leading path element out, and naming the virtual filesystem path after
that leading element.

It isn't clear what the correct way of dealing with absolute paths is. The
current behaviour is:

  - for archive members the leading slash, if present, is stripped, before
    placing the member in the virtual filesystem,

  - for archived symbolic links to absolute paths, the target path of the
    symbolic link is left as it is.

In the second case, the archive could be searched for a member matching the
target path, and if present the symbolic link could be adjusted to refer to
that member.
#endif

//
// VFTgzEntityFactory
//

class VFTgzEntityFactory : public VFEntityFactory
{
public:
	VFTgzEntityFactory()
	{
		mask_	= umask(0);
		umask(mask_);
		uid_	= getuid();
		gid_	= getgid();
	}

	VFEntity* CreateFile(Tar::Reader& tar)
	{
		VFEntity* entity = new VFTgzFileEntity(tar);
		if(!entity) {
			VFLog(0, "creating file entity  failed: [%d] %s", VFERR(errno));
			exit(0);
		}
		return entity;
	}
	VFEntity* CreateLink(const Path& linkto)
	{
		VFEntity* entity =
			new VFSymLinkEntity(uid_, gid_, mask_, linkto.c_str());

		if(!entity) {
			VFLog(0, "creating link entity to '%s' failed: [%d] %s",
				linkto.c_str(), errno, strerror(errno));
			exit(0);
		}
		return entity;
	}

	VFDirEntity* AutoDir()
	{
		return new VFDirEntity(uid_, gid_, 0555 & ~mask_, this);
	}

private:
	mode_t	mask_;
	uid_t	uid_;
	gid_t	gid_;
};


//
// vf_tgz: globals
//

int		vOpt	= 0;
char*	pathOpt	= 0;
int		dOpt	= 0;
char*	FOpt	= 0;
char*	tarOpt	= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvdF:t:")) != -1; )
	{
		switch(c)
		{
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			VFLevel("vf_tgz", vOpt);
			break;

		case 'd':
			dOpt = 1;
			break;

		case 'F':
			FOpt = optarg;
			break;

		case 't':	// ignore the -t option passed by 'mount'
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind++]))
	{
		fprintf(stderr, "no tarfile specified!\n");
		exit(1);
	}

	pathOpt = argv[optind] ? argv[optind] : 0;

	return 0;
}

void main(int argc, char* argv[])
{
	VFVersion	vfver("vf_tgz", 0.1);
	VFManager	vfmgr(vfver);

	VFLevel("vf_tgz", vOpt);

	GetOpts(argc, argv);

	VFTgzEntityFactory* factory = new VFTgzEntityFactory;
	VFDirEntity* root = factory->AutoDir();

	Tar::Reader tar(
			tarOpt,
			FOpt ? Tar::Reader::FILTER : Tar::Reader::GUESS,
			FOpt);
	if(!tar.Open())
	{
		if(tar.ErrorNo() == ENOENT) {
			VFLog(0, "No standard tar headers found, is this a tar archive?");
		} else {
			VFLog(0, "Reader::Open() said %s failed: [%d] %s",
				tar.ErrorInfo(), tar.ErrorNo(), tar.ErrorStr());
		}
		exit(1);
	}

	do {
		VFLog(2, "next '%s'", tar.Path());

		struct stat stat;
		tar.Record()->Stat(&stat);
		int type = S_IFMT & stat.st_mode;

		const char* path = tar.Path();
		if(*path == '/')
			path++;

		switch(type)
		{
		case S_IFDIR: // don't bother with these for now
			break;

		case S_IFREG: {
			Path to(tarOpt);

			int e = root->Insert(path, factory->CreateFile(tar));
		  }	break;

		case S_IFLNK: {
			int e = root->Insert(path, factory->CreateLink(tar.LinkTo()));
		  }	break;

		default:
			VFLog(1, "unknown type: %#x, member '%s'",
				type, tar.Path());
			exit(1);
		}
	}
	while(tar.Next());

	tar.Close();

	VFEntity* top = root;

	// check if there is only one top-level directory, and elide it if so
	if(!pathOpt && root->Entries() == 1)
	{
		VFDirEntity::EntityNamePair* pair = root->EnPair(0);
		
		if(S_ISDIR(pair->entity->Info()->mode))
		{
			delete top;
			top = pair->entity;

			ostrstream s;
			s << pair->name << '\0';
			pathOpt = s.str();
		}
	}

	if(!pathOpt)
	{
		char path[_MAX_PATH];
		char ext[_MAX_EXT];
		_splitpath(tarOpt, 0, 0, path, ext);

		pathOpt = new char[strlen(path) + 1];
		strcpy(pathOpt, path);
	}

	if(!vfmgr.Init(top, pathOpt, vOpt)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	vfmgr.Start(dOpt);

	vfmgr.Run();
}

