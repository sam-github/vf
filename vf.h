//
// vf.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.5  1998/04/06 06:48:14  sroberts
// implemented write()
//
// Revision 1.4  1998/04/05 23:54:00  sroberts
// added mkdir() support
//
// Revision 1.3  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
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
#include <sys/fsys_msg.h>
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
class VFManager;
class VFOcbMap;

class VFEntity
{
public:
	virtual ~VFEntity() = 0;

	virtual VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply) = 0;
	virtual int Stat(const String& path, _io_open* req, _io_fstat_reply* reply) = 0;
	virtual int ChDir(const String& path, _io_open* req, _io_open_reply* reply) = 0;
	virtual int Unlink() = 0;
	virtual int MkDir(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply) = 0;

	virtual bool Insert(const String& path, VFEntity* entity) = 0;
	virtual struct stat* Stat() = 0;
};

class VFOcb
{
public:
	VFOcb();
	virtual ~VFOcb() = 0;

	virtual int Stat() = 0;
	virtual int Read() = 0;
	virtual int Write(pid_t pid, _io_write* req, _io_write_reply* reply) = 0;
	virtual int Seek() = 0;
	virtual int Chmod() = 0;
	virtual int Chown() = 0;

	virtual int ReadDir(_io_readdir* req, _io_readdir_reply* reply) = 0;
	virtual int RewindDir(_io_rewinddir* req, _io_rewinddir_reply* reply) = 0;

private:
	friend class VFOcbMap;

	void Refer() { refCount_++; }
	void Unfer() { refCount_--; if(refCount_ <= 0) delete this; }
	int refCount_;
};

#endif

