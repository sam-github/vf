//
// vf_syml.h
//
// Copyright (c) 1999, Sam Roberts
// 
// $Log$
// Revision 1.1  1999/04/24 04:38:39  sam
// Initial revision
//

#ifndef VF_SYML_H
#define VF_SYML_H

#include <sys/stat.h>

#include "vf.h"

class VFSymLinkEntity : public VFEntity
{
public:
	VFSymLinkEntity(const char* linkto, uid_t uid = getuid(), gid_t gid = getgid());

	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	int Stat(const String& path, _io_open* req, _io_fstat_reply* reply);
	int ChDir(const String& path, _io_open* req, _io_open_reply* reply);
	int Unlink();
	int MkSpecial(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply);
	int ReadLink(const String& path, _fsys_readlink* req, _fsys_readlink_reply* reply);

	bool Insert(const  String& path, VFEntity* entity);
	struct stat* Stat();

private:
	char		linkto_[PATH_MAX + 1];
	struct stat stat_;

	int RewriteOpenPath(const String& path, char* ret, msg_t* status);
};

#endif

