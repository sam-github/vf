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
#include "tar_arch.h"

#ifdef __USAGE
%C - a tar archive virtual filesystem

usage: vf_tar [-hv] [-p vf] tarfile
    -h   Print this helpful message.
    -v   Increase the verbosity level.
    -p   Path of virtual file system to create, default is the base
         name of the file, without any extension.
    -u   Attempt to use the user and group of the tar archive member to find
         the local uid and gid.
    -e   Elide top-level directory if there is only one and use that name as
         the path 'vf' if the -p option was not specified.
    -d   Don't become a daemon, default is to fork into the background after
         reading the tar file.

Mounts 'tarfile' as a virtual (read-only) filesystem at 'vf'. It can be
unmounted by doing a rmdir on the vfsys path (causing vf_tar to exit).

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
	VFTarEntityFactory(int uOpt) : uOpt_(uOpt)
	{
		mask_ = umask(0);
		umask(mask_);
	}

	VFEntity* AutoDir()
	{
		return new VFDirEntity(getpid(), 0555 & ~mask_, this);
	}

	VFEntity* Create(TarArchive::iterator& it)
	{
		// attempt to find the local user by name
		if(uOpt_) {
			struct passwd* pw = getpwnam(it.User());
			struct group* gr = getgrnam(it.Group());

			it.ChUidGid(pw ? pw->pw_uid : -1, gr ? gr->gr_gid : -1);
		}

		VFEntity* entity = 0;
		const struct stat* stat = it.Stat();

		if(S_ISREG(stat->st_mode)) {
			entity = new VFTarFileEntity(it);
		}
		else if(S_ISDIR(stat->st_mode)) {
			// XXX this is a hideous security hole!

			entity = new VFDirEntity(stat->st_uid, stat->st_gid,
							stat->st_mode, this);
		}
		else if(S_ISLNK(stat->st_mode)) {
			entity = new VFSymLinkEntity(stat->st_uid, stat->st_gid,
							stat->st_mode, it.Link());
		}
		else {
			VFLog(1, "unsupported tar file type");

			// XXX try taring up a FIFO and seeing what this does
			return 0;
		}

		if(!entity) {
			VFLog(0, "%s", strerror(errno));
			exit(1);
		}
		return entity;
	}

private:
	int		uOpt_;
	mode_t	mask_;
};


//
// vf_tar: globals
//

int		vOpt	= 0;
char*	pathOpt	= 0;
int		uOpt	= 0;
int		eOpt	= 0;
char*	tarOpt	= 0;
int		dOpt	= 0;

int Depth(const char* p)
{
	int depth = 1;

	while((p = strchr(p, '/'))) {
		++p; // because p now points at the '/'
		if(*p) {
			++depth; // don't count a trailing '/' in the depth
		}
	}
	return depth;
}

int GetOpts(int argc, char* argv[])
{
	for(int c; (c = getopt(argc, argv, "hvp:ued")) != -1; ) {
		switch(c) {
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

		case 'e':
			eOpt = 1;
			break;

		case 'd':
			dOpt = 1;
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind])) {
		fprintf(stderr, "no tarfile specified!\n");
		exit(1);
	}

	return 0;
}

void main(int argc, char* argv[])
{
	VFVersion	vfver("vf_tar", 1.1);
	VFManager	vfmgr(vfver);

	VFLevel("vf_tar", vOpt);

	GetOpts(argc, argv);

	TarArchive tar;

	if(!tar.Open(tarOpt)) {
		VFLog(0, "open tarfile %s failed: [%d] %s",
			tarOpt, tar.ErrorNo(), tar.ErrorString());
		exit(1);
	}

	mode_t mask = umask(0); umask(mask);

	VFTarEntityFactory* factory = new VFTarEntityFactory(uOpt);
	VFDirEntity* root = new VFDirEntity(getuid(), getgid(), 0555 & ~mask, factory);

	// This loop is slow but robust. Basically, we'll create intermediary
	// directories to entities with default attributes if necessary, but
	// we'd rather find that directories actual tar record, which will have
	// the correct attributes. Thus loop through the archive, first
	// inserting all top-level entities, then second-level, etc. The first
	// loop through we'll record the maximum depth so we don't go forever.

	int maxDepth = 1;
	for(int d = 1; d <= maxDepth; ++d) {
		for(TarArchive::iterator it = tar.begin(); it; ++it) {
			const char* p = it.Path();

			if(p[0] == '/') { ++p; }

			int depth = Depth(p);

			VFLog(3, "d %d depth %d file %s", d, depth, it.Path());

			if(depth > maxDepth) { maxDepth = depth; }

			if(d == depth) {
				if(vOpt >= 4) { it.DebugFile(stdout); }

				VFEntity* entity = factory->Create(it);

				if(!entity) {
					VFLog(0, "creating entity for %s failed: [%d] %s",
						it.Path(), errno, strerror(errno));
					exit(1);
				}
				int e = root->Insert(p, entity);
				if(e != EOK) {
					VFLog(0, "inserting entity %s failed: [%d] %s",
						it.Path(), e, strerror(e));
					exit(1);
				}
			}
		}
	}

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

