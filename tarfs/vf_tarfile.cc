//
// vf_tarfile.cc
//
// Copyright (c) 1999, Sam Roberts
// 
// $Log$
// Revision 1.2  1999/04/24 04:41:46  sam
// added support for symbolic links, and assorted bugfixes
//
// Revision 1.1  1999/04/11 06:45:36  sam
// Initial revision
//

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/sendmx.h>
#include <unistd.h>
#include <time.h>

#include <vf_log.h>
#include <vf_file.h>

#include "vf_tarfile.h"

//
// VFTarFileEntity
//

char VFTarFileEntity::buffer[BUFSIZ];

VFTarFileEntity::VFTarFileEntity(const TarArchive::iterator& file) :
	file_	(file)
{
//	file_ = file;

	if(!file_.Stat(&stat_)) {
		VFLog(0, "loading stat for %s failed: [%d] %s",
			file_.Path(), file_.ErrorNo(), file_.ErrorString());
		exit(1);
	}
	// need to massage the stat structure, see <sys/stat.h>
	stat_.st_ouid = stat_.st_uid;
	stat_.st_ogid = stat_.st_gid;
}

VFTarFileEntity::~VFTarFileEntity()
{
	VFLog(3, "VFTarFileEntity::~VFTarFileEntity()");
}

int VFTarFileEntity::Write(pid_t pid, size_t nbytes, off_t offset)
{
	pid = pid; nbytes = nbytes; offset = offset;

	VFLog(2, "VFTarFileEntity::Write()");

	errno = ENOSYS;
	return -1;
}

int VFTarFileEntity::Read(pid_t pid, size_t nbytes, off_t offset)
{
	VFLog(2, "VFTarFileEntity::Read() nbytes %ld, offset %ld", nbytes, offset);

	if(nbytes > BUFSIZ) { nbytes = BUFSIZ; }

	file_.Seek(offset);

	unsigned ret = file_.Read(buffer, nbytes);

	if(ret == -1) {
		errno = file_.ErrorNo();
	} else {
		// ready to write nbytes from the data buffer to the offset of the
		// "data" part of the read reply message
		size_t dataOffset = offsetof(struct _io_read_reply, data);
		ret = Writemsg(pid, dataOffset, buffer, ret);
	}

	if(ret != -1)
	{
		VFLog(5, "VFTarFileEntity::Read() read \"%.*s\"", ret, buffer);
	}

	return ret;
}

