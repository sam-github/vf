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

#include "vf_tgzfile.h"

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

	VFEntity* CreateFile(const TarUntar& untar, off_t size)
	{
		VFEntity* entity = new VFTgzFileEntity(untar, size);
		if(!entity) {
			VFLog(0, "creating file entity  failed: [%d] %s", VFERR(errno));
			exit(0);
		}
		return entity;
	}
	VFEntity* CreateLink(const String& linkto)
	{
		VFEntity* entity =
			new VFSymLinkEntity(uid_, gid_, mask_, linkto);
		if(!entity) {
			VFLog(0, "creating link entity to '%s' failed: [%d] %s",
				(const char*)linkto, errno, strerror(errno));
			exit(0);
		}
		return entity;
	}

	VFEntity* AutoDir()
	{
		return new VFDirEntity(uid_, gid_, 0777 & ~mask_, this);
	}

private:
	mode_t	mask_;
	uid_t	uid_;
	gid_t	gid_;
};

//
// Usage and Options
//

#ifdef __USAGE
%C - mounts a compressed tar archive virtual filesystem

usage: vf_tgz [-hvde] file [vf]
    -h   Print this helpful message.
    -v   Increase the verbosity level.
    -d   Don't become a daemon, default is to fork into the background after
         reading the tar file.
    -e   Elide top-level directory if there is only one and use that name as
         the path 'vf' if the -p option was not specified.

Mounts 'file' as a virtual (read-only) filesystem at 'vf', which defaults
to the base name of the file, without any extensions. The file system can
be unmounted by doing a umount or rmdir on the vfsys path (causing vf_tgz
to exit).

It isn't clear what the correct way of dealing with absolute paths is. The
current behaviour is:

  - for archive members the leading slash, if present, is stripped, before
    placing the member in the virtual filesystem,

  - for archived symbolic links to absolute paths, the target path of the
    symbolic link is left as it is.

In the second case, the archive could be searched for a member matching the
target path, and if present the symbolic link could be adjusted to refer to
that member.

The -e option is an attempt at dealing with the common case of archived
sub-directories, such as "yoyo-widgets-1.3.4/..." by stripping the single
leading path element out, and naming the virtual filesystem path after
that leading element. It may become the default because I use it almost
all the time.
#endif


//
// vf_tgz: globals
//

int		vOpt	= 0;
char*	pathOpt	= 0;
int		eOpt	= 0;
int		dOpt	= 0;
char*	tarOpt	= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvdet:")) != -1; ) {
		switch(c) {
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

		case 'e':
			eOpt = 1;
			break;

		case 't':	// ignore the -t option passed by 'mount'
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind++])) {
		fprintf(stderr, "no tarfile specified!\n");
		exit(1);
	}

	pathOpt = argv[optind] ? argv[optind] : 0;

	return 0;
}

static int ParseUntarOutput(ipipestream& ip, char& type, String& file)
{
	strstream ss;

	ip.get(*ss.rdbuf(), '\n');
	ip.get();

	ss << '\0';

	if(!ip.good()) { return 0; }

	// Untar format is
	//  'x: <filename>' with x one of 'f', 'd', 'l', and
	//  '-> <filename>' follows a 'l' line to give the link target,
	// so do this:

	char* str = ss.str();

	if(strlen(str) < 4) {
		VFLog(0, "untar returned a short line");
		exit(1);
	}
	type = str[0];
	file = str+3;

	delete[] str;

	return 1;
}

void main(int argc, char* argv[])
{
	VFVersion	vfver("vf_tgz", 0.1);
	VFManager	vfmgr(vfver);

	VFLevel("vf_tgz", vOpt);

	GetOpts(argc, argv);

	mode_t mask = umask(0); umask(mask);

	VFTgzEntityFactory* factory = new VFTgzEntityFactory;
	VFDirEntity* root =
		new VFDirEntity(getuid(), getgid(), 0555 & ~mask, factory);

	VFEntity* top = 0;

	strstream cmd;
	cmd << "untar -z -l " << tarOpt << '\0';
	ipipestream ip(cmd.str());

	char	type;
	String	file;
	while(ParseUntarOutput(ip, type, file))
	{
		VFLog(2, "untar -> type %c '%s'", type, (const char*)file);

		switch(type)
		{
		case 'd': // don't bother with these for now
			break;

		case 'f': {
			String to(tarOpt);

			TarUntar untar(to, file, TarUntar::GUESS);
			//TarUntar untar(tarOpt, file, TarUntar::GUESS);

			int e = root->Insert(file, factory->CreateFile(untar, 0));
		  }	break;

		case 'l': {
			char	minus;
			String	linkto;
			if(!ParseUntarOutput(ip, minus, linkto)) {
				VFLog(0, "link '%s' has no target", (const char*)file);
				exit(1);
			}
			int e = root->Insert(file, factory->CreateLink(linkto));
		  }	break;

		default:
			VFLog(0, "failed to parse untar output: invalid type '%c'",
				type);
			exit(1);
		}
		file = "";
	}

	// check if there is only one top-level directory, and elide it if so
	if(eOpt && root->Entries() == 1)
	{
		VFDirEntity::EntityNamePair* pair = root->EnPair(0);
		
		if(S_ISDIR(pair->entity->Info()->mode))
		{
			delete root;
			top = pair->entity;
		}
		if(!pathOpt)
		{
			ostrstream s;
			s << pair->name << '\0';
			pathOpt = s.str();
		}
	}

	if(!top)
	{
		top = root;
	}

	if(!pathOpt) {
		char path[_MAX_PATH];
		char ext[_MAX_EXT];
		_splitpath(tarOpt, 0, 0, path, ext);

		pathOpt = new char[strlen(path) + 1];
		strcpy(pathOpt, path);
	}

	if(!dOpt)
	{
		// go into background

		switch(fork())
		{
		case 0:
			break;
		case -1:
			VFLog(0, "fork failed: [%d] %s", errno, strerror(errno));
			exit(1);
		default:
			exit(0);
		}
	}

	if(!vfmgr.Init(top, pathOpt, vOpt)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	vfmgr.Run();
}

