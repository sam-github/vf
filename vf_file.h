//
// vf_file.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.4  1999/04/11 06:41:37  sam
// split FileEntity into a RamFileEntity and a base FileEntity that is used by
// the FileOcb, turns out most of the FileEntity is reuseable, just the actual
// read/write functions were specific to the RAM filesystem
//
// Revision 1.3  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
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
	VFOcb* Open(const String& path, _io_open* req, _io_open_reply* reply);
	int Stat(const String& path, _io_open* req, _io_fstat_reply* reply);
	int ChDir(const String& path, _io_open* req, _io_open_reply* reply);
	int Unlink();
	int MkDir(const String& path, _fsys_mkspecial* req, _fsys_mkspecial_reply* reply);

	bool Insert(const String& path, VFEntity* entity);
	struct stat* Stat();

	// API extensions for use by VFFileOcb
	virtual int Write(pid_t pid, size_t nbytes, off_t offset) = 0;
	virtual int Read(pid_t pid, size_t nbytes, off_t offset) = 0;

protected:
	struct stat stat_;
};

class VFFileOcb : public VFOcb
{
public:
	VFFileOcb(VFFileEntity* file);
	~VFFileOcb();

	int Write(pid_t pid, _io_write* req, _io_write_reply* reply);
	int Read(pid_t pid, _io_read* req, _io_read_reply* reply);
	int Seek(pid_t pid, _io_lseek* req, _io_lseek_reply* reply);
	int Stat(pid_t pid, _io_fstat* req, _io_fstat_reply* reply);
	int Chmod();
	int Chown();

	int ReadDir(pid_t pid, _io_readdir* req, _io_readdir_reply* reply);
	int RewindDir(pid_t pid, _io_rewinddir* req, _io_rewinddir_reply* reply);

private:
	VFFileEntity* file_;

	off_t offset_;
};

class VFRamFileEntity : public VFFileEntity
{
public:
	VFRamFileEntity(mode_t mode);
	~VFRamFileEntity();

	int Write(pid_t pid, size_t nbytes, off_t offset);
	int Read(pid_t pid, size_t nbytes, off_t offset);

private:
	char* data_; // pointer to buffer
	off_t dataLen_;  // length of data buffer
	off_t fileSize_; // size of file data (may be less than dataLen_)

	void InitStat(mode_t mode);
};

#endif

