//
// vf_syml.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.1  1999/04/24 04:37:06  sam
// Initial revision
//

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/sendmx.h>
#include <unistd.h>
#include <time.h>

#include "vf_syml.h"
#include "vf_log.h"

//
// VFSymLinkEntity
//

VFSymLinkEntity::VFSymLinkEntity(const char* linkto, uid_t uid, gid_t gid)
{
	VFLog(3, "VFSymLinkEntity::VFSymLinkEntity(%s uid %d gid %d)",
		linkto, uid, gid);

	linkto_[0] = '\0';

	memset(&stat_, 0, sizeof(stat_));

	if(!linkto || strlen(linkto) > PATH_MAX) {
		errno = EINVAL;
		return;
	}
	stat_.st_size = strlen(linkto_);
	stat_.st_ouid = uid;
	stat_.st_ogid = gid;
	stat_.st_mode = S_IFLNK | 0777;

	strcpy(linkto_, linkto);
}

VFOcb* VFSymLinkEntity::Open(
	const String& path,
	_io_open* req,
	_io_open_reply* reply)
{
	VFLog(2, "VFSymLinkEntity::Open() path \"%s\"", (const char *) path);

	RewriteOpenPath(path, req->path, &reply->status);

	return 0;
}

int VFSymLinkEntity::Stat(
	const String& path,
	_io_open* req,
	_io_fstat_reply* reply
	)
{
	VFLog(2, "VFSymLinkEntity::Stat(\"%s\") mode %#x eflag %#x",
		(const char *) path, req->mode, req->eflag);

	req = req; reply = reply;

	// If we're just a component in the path, rewrite it.
	if(path != "")
	{
		return RewriteOpenPath(path, req->path, &reply->status);
	}

	// if mode is set it's an lstat()
	if(req->mode == S_IFLNK && !req->eflag) {
		reply->status = EOK;
		reply->zero = 0;
		reply->stat   = stat_;
		return sizeof(*reply);
	}

	// otherwise it's a normal stat(), so rewrite the path
	return RewriteOpenPath(path, req->path, &reply->status);
}

int VFSymLinkEntity::ChDir(const String& path, _io_open* req, _io_open_reply* reply)
{
	VFLog(2, "VFSymLinkEntity::ChDir(\"%s\")", (const char*) path);

	return RewriteOpenPath(path, req->path, &reply->status);
}

int VFSymLinkEntity::Unlink()
{
	VFLog(2, "VFSymLinkEntity::Unlink()");

	return 0;
}

int VFSymLinkEntity::MkSpecial(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply)
{
	VFLog(2, "VFSymLinkEntity::MkSpecial(\"%s\")", (const char*) path);

	req = req; reply = reply;

	return RewriteOpenPath(path, req->path, &reply->status);
}

int VFSymLinkEntity::ReadLink(const String& path, _fsys_readlink* req, _fsys_readlink_reply* reply)
{
	VFLog(2, "VFSymLinkEntity::ReadLink(\"%s\")", (const char*) path);

	if(path != "") {
		return RewriteOpenPath(path, req->path, &reply->status);
	}

	req = req; reply = reply;

	reply->status = EOK;
	strcpy(reply->path, linkto_);

	return sizeof(*reply) + strlen(linkto_);
}

bool VFSymLinkEntity::Insert(const String& path, VFEntity* entity)
{
	VFLog(2, "VFSymLinkEntity::Insert(\"%s\")", (const char*) path);

	entity = entity;

	errno = ENOTDIR;

	return false;
}

struct stat* VFSymLinkEntity::Stat()
{
	return &stat_;
}

int VFSymLinkEntity::RewriteOpenPath(const String& path, char* ret, msg_t* status)
{
	VFLog(2, "VFSymLinkEntity::RewriteOpenPath() rewrite \"%s\" as \"%s\"",
		(const char*) path, linkto_);

	*status = EMORE;
	strcpy(ret, linkto_);

	if(path != "")
	{
		// append the sub-path they're really interested in
		strcat(ret, "/");
		strcat(ret, path);
	}

	return sizeof(_io_open) + strlen(ret);
}

