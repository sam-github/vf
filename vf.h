//
// vf.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.2  1998/03/15 22:07:03  sroberts
// added definitions for VFOcb and VFEntity
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_H
#define VF_H

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/io_msg.h>
#include <string.hpp>

#if __WATCOMC__ < 1100
	typedef int bool;
	const bool true  = 1;
	const bool false = 0;
#endif

// forward references
class VFEntity;
class VFOcb;

class VFEntity
{
public:
	virtual VFOcb* Open(const String& path, struct _io_open* req, struct _io_open_reply* reply) = 0;
	virtual struct stat* Stat(const String& path, struct _io_open* req, struct _io_open_reply* reply) = 0;
	virtual int Chdir() = 0;
	virtual int Unlink() = 0;

	virtual bool Insert(const String& path, VFEntity* entity) = 0;
};

class VFOcb
{
public:
	virtual int Close();
	virtual int Stat();
	virtual int Read();
	virtual int Write();
	virtual int Seek();
	virtual int Chmod();
	virtual int Chown();

	virtual int DirRead();
	virtual int DirRewind();
};

#endif

