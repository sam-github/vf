//
// vf_test.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
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

#ifdef __USAGE
%C - test driver for the virtual filesystem

usage: vf_test [-hv] [-p vf]
    -h   print help message
    -v   increse the verbosity level
    -p   path of virtual file system to create
#endif

// globals

int vOpt = 0;

char* pathOpt = "vf";

VFManager* vfmgr = new VFManager;
VFIoMsg* msg = new VFIoMsg;

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

	VFDirEntity* dir = new VFDirEntity;

	if(!vfmgr->Init(dir, pathOpt, vOpt))
	{
		printf("VFManager init failed: [%d] %s\n",
			errno, strerror(errno)
			);
		exit(1);
	}

	VFEntity* entity;
	int ok;

	entity = new VFDirEntity;
	ok = dir->Insert("sub", entity);
	assert(ok);

	entity = new VFDirEntity;
	ok = dir->Insert("sub/sub1/sub2", entity);
	assert(ok);

	vfmgr->Run();

	assert(0);

	return 0;
}

/*
	pid_t pid = getpid();

	msg->type = _IO_OPEN;
	strcpy(msg->open.path, "");
	vfmgr->Handle(pid, msg);

	msg->type = _IO_OPEN;
	strcpy(msg->open.path, "sub/sub/enoent");
	vfmgr->Handle(pid, msg);

	msg->type = _IO_OPEN;
	strcpy(msg->open.path, "sub/sub");
	vfmgr->Handle(pid, msg);
*/
