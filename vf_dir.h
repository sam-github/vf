//
// vf_dir.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.2  1998/03/15 22:07:49  sroberts
// implemented insertion
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_DIR_H
#define VF_DIR_H

#include <wchash.h>

#include "vf.h"

class VFDirEntity : public VFEntity
{
public:
	VFDirEntity();

	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	struct stat* Stat(const String& path, _io_open* req, _io_open_reply* reply);
	int Chdir();
	int Unlink();

	bool Insert(const String& path, VFEntity* entity);


private:
	typedef WCValHashDict<String, VFEntity*> EntityMap;

	EntityMap entityMap_;

	void SplitPath(const String& path, String& lead, String& tail);

	static unsigned Hash(const String& key);
};

class VFDirOcb
{
public:
	int Close();
	int Stat();
	int Read();
	int Write();
	int Seek();
	int Chmod();
	int Chown();

	int DirRead();
	int DirRewind();
};

#endif

