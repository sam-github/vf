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
// Revision 1.15  1999/12/05 01:59:00  sam
// now using the rewritten <tar/tararch.h> classes
//
// Revision 1.14  1999/10/04 03:33:20  sam
// forking is now done by the manager
//
// Revision 1.13  1999/08/09 15:17:56  sam
// Ported framework modifications down.
//
// Revision 1.12  1999/07/19 15:14:02  sam
// small changes and bugfixes
//
// Revision 1.11  1999/07/02 15:26:28  sam
// simple bug causing segvs for tar members with absolute ("/...") paths fixed
//
// Revision 1.10  1999/06/21 13:49:03  sam
// now becomes a daemon, and can elide a leading directory path
//
// Revision 1.9  1999/06/21 12:41:08  sam
// implemented sysmsg... version
//
// Revision 1.8  1999/06/20 13:37:13  sam
// factory interface changed
//
// Revision 1.7  1999/06/18 17:03:36  sam
// implemented new entity factory for tar
//
// Revision 1.6  1999/04/30 02:39:38  sam
// more info in usage message
//
// Revision 1.5  1999/04/30 01:54:26  sam
// Changes to deal with archives where members are not in order of deepest
// last, and where intermediate directories are not in the archive.
//
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

#include <strstream.h>

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_syml.h>
#include <vf_log.h>

#include "vf_tarfile.h"
#include <tar/tararch.h>

#ifdef __USAGE
%C - a tar archive virtual filesystem

usage: vf_tar [-hvd] [-e] file [vf]
    -h   Print this helpful message.
    -v   Increase the verbosity level.
    -d   Don't become a daemon, default is to fork into the background after
         reading the tar file.
    -e   Elide top-level directory if there is only one and use that name as
         the mount path if 'vf' was not explicitly specified.

Mounts 'tarfile' as a virtual (read-only) filesystem at 'vf'. It can be
unmounted in the standard way.

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
// VFTarEntityFactory
//

class VFTarEntityFactory : public VFEntityFactory
{
public:
	VFTarEntityFactory()
	{
		mask_ = ~umask(0);
		umask(mask_);
	}

	VFEntity* AutoDir()
	{
		return new VFDirEntity(getpid(), 0555 & mask_, this);
	}

	VFEntity* Create(Tar::Archive& tar)
	{
		VFEntity* entity = 0;

		struct stat stat;

		tar.Record()->Stat(&stat);

		if(S_ISREG(stat.st_mode)) {
			Tar::Member* m = tar.Member();

			entity = new VFTarFileEntity(m, stat.st_mode & mask_);
		}
		else if(S_ISDIR(stat.st_mode)) {
			entity = new VFDirEntity(getuid(), getgid(),
				stat.st_mode & mask_, this);
		}
		else if(S_ISLNK(stat.st_mode)) {
			entity = new VFSymLinkEntity(getuid(), getgid(),
							stat.st_mode & mask_, tar.LinkTo());
		}
		else {
			VFLog(1, "unsupported tar file type, mode %#x", stat.st_mode);

			// XXX try taring up a FIFO and seeing what this does
			return 0;
		}

		if(!entity) {
			VFLog(0, "creating entity failed: [%d] %s", VFERR(errno));
			exit(1);
		}
		return entity;
	}

private:
	mode_t	mask_;
};

//
// vf_tar: globals
//

int		vOpt	= 0;
int		dOpt	= 0;
int		eOpt	= 0;
char*	tarOpt	= 0;
char*	pathOpt	= 0;

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvdet:")) != -1; ) {
		switch(c) {
		case 'h':
			print_usage(argv);
			exit(0);

		case 'v':
			vOpt++;
			VFLevel("vf_tar", vOpt);
			break;

		case 'd':
			dOpt = 1;
			break;

		case 'e':
			eOpt = 1;
			break;

		case 't': // ignore the -t option passed by mount
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind++])) {
		fprintf(stderr, "no tarfile specified!\n");
		exit(1);
	}

	pathOpt = argv[optind];

	return 0;
}

void main(int argc, char* argv[])
{
	VFVersion	vfver("vf_tar", 1.1);
	VFManager	vfmgr(vfver);

	VFLevel("vf_tar", vOpt);

	GetOpts(argc, argv);

	Tar::Reader tar(tarOpt, Tar::Reader::TAR);

	if(!tar.Open()) {
		VFLog(0, "%s failed: [%d] %s",
			tar.ErrorInfo(), tar.ErrorNo(), tar.ErrorStr());
		exit(1);
	}

	mode_t mask = umask(0); umask(mask);

	VFTarEntityFactory* factory = new VFTarEntityFactory;
	VFDirEntity* root = new VFDirEntity(getuid(), getgid(), 0555 & ~mask, factory);

	do {
		VFLog(3, "member %s", tar.Path());

		VFEntity* entity = factory->Create(tar);

		int e = root->Insert(tar.Path(), entity);
		if(e != EOK) {
			VFLog(0, "inserting entity %s failed: [%d] %s",
				tar.Path(), e, strerror(e));
			exit(1);
		}
	} while(tar.Next());

	VFEntity* top = 0;

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

	if(!vfmgr.Init(top, pathOpt, vOpt)) {
		VFLog(0, "init failed: [%d] %s\n", errno, strerror(errno));
		exit(1);
	}

	vfmgr.Start(dOpt);

	vfmgr.Run();
}

