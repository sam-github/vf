//
// vf_file.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.2  1998/04/06 06:50:19  sroberts
// implemented creat(), and write()
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#ifndef VF_FILE_H
#define VF_FILE_H

#include "vf.h"

class VFFileEntity : public VFEntity
{
public:
	VFFileEntity(mode_t mode);
	~VFFileEntity();

	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	int Stat(const String& path, _io_open* req, _io_fstat_reply* reply);
	int ChDir(const String& path, _io_open* req, _io_open_reply* reply);
	int Unlink();
	int MkDir(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply);

	bool Insert(const String& path, VFEntity* entity);
	struct stat* Stat();

	// used by the Ocb
	int Write(pid_t pid, size_t nbytes, off_t offset);

private:
	char* data_; // pointer to buffer
	off_t len_;  // length of data buffer
	off_t size_; // size of data (may be less than that of the buffer)

	struct stat stat_;

	void InitStat(mode_t mode);
};

class VFFileOcb : public VFOcb
{
public:
	VFFileOcb(VFFileEntity* file);
	~VFFileOcb();

	int Stat();
	int Read();
	int Write(pid_t pid, _io_write* req, _io_write_reply* reply);
	int Seek();
	int Chmod();
	int Chown();

	int ReadDir(_io_readdir* req, _io_readdir_reply* reply);
	int RewindDir(_io_rewinddir* req, _io_rewinddir_reply* reply);

private:
	VFFileEntity* file_;

	off_t offset_;
};

#endif

