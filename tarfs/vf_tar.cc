//
// vf_tar.cc
//
// Copyright (c) 1999, Sam Roberts
// 
// $Log$
// Revision 1.1  1999/04/11 06:45:36  sam
// Initial revision
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_log.h>

#include "tar_arch.h"
#include "vf_tarfile.h"

#ifdef __USAGE
%C - virtual filesystem for tar files

usage: vf_tar [-hv] [-p vf] tarfile
    -h   print help message
    -v   increse the verbosity level
    -p   path of virtual file system to create
#endif

int		vOpt	= 0;
char*	pathOpt	= 0;
char*	tarOpt	= 0;

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
			VFLevel("vf_tar: ", vOpt);
			break;

		case 'p':
			pathOpt = optarg;
			break;

		default:
			exit(1);
		}
	}

	if(!(tarOpt = argv[optind])) {
		fprintf(stderr, "no tarfile specified\n");
		exit(1);
	}

	if(!pathOpt) {
		pathOpt = new char[strlen(tarOpt) + 1];
		strcpy(pathOpt, tarOpt);
	}

	return 0;
}

void main(int argc, char* argv[])
{
	VFManager* vfmgr = new VFManager;

	VFLevel("vf_tar: ", vOpt);

	GetOpts(argc, argv);

	TarArchive tar;

	if(!tar.Open(tarOpt)) {
		VFLog(0, "open tarfile %s failed: [%d] %s",
			tarOpt, tar.ErrorNo(), tar.ErrorString());
		exit(1);
	}

	VFDirEntity* root = new VFDirEntity(0555);

	for(TarArchive::iterator it = tar.begin(); it; ++it) {
		VFLog(3, "file %s\n", it.Path());

		if(vOpt >= 4) { it.DebugFile(stdout); }

		if(!root->Insert(it.Path(), new VFTarFileEntity(it))) {
			VFLog(1, "inserting %s failed: [%d] %s", errno, strerror(errno));
		}
	}

	if(!vfmgr->Init(root, pathOpt, vOpt))
	{
		printf("VFManager init failed: [%d] %s\n",
			errno, strerror(errno)
			);
		exit(1);
	}

	vfmgr->Run();
}

