//
// vf_tarfile.h
//
// Copyright (c) 1999, Sam Roberts
// 
// $Log$
// Revision 1.1  1999/04/11 06:45:36  sam
// Initial revision
//

#ifndef VF_TARFILE_H
#define VF_TARFILE_H

#include <vf_file.h>

#include "tar_arch.h"

class VFTarFileEntity : public VFFileEntity
{
public:
	VFTarFileEntity(TarArchive::iterator file);
	~VFTarFileEntity();

	int Write(pid_t pid, size_t nbytes, off_t offset);
	int Read(pid_t pid, size_t nbytes, off_t offset);

private:
	TarArchive::iterator file_;

	static char buffer[BUFSIZ];
};

#endif

