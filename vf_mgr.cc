//
// vf_fsys.cc
//
// Copyright (c) 1998, Sam Roberts
// 
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 1, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  I can be contacted as sroberts@uniserve.com, or sam@cogent.ca.
//
// $Log$
// Revision 1.15  1999/07/11 11:28:47  sam
// ocb maps now use an index into a resizeable watcom vector, so should be
// 32-bit pointer safe
//
// Revision 1.14  1999/06/21 12:36:22  sam
// implemented sysmsg... version
//
// Revision 1.13  1999/06/21 10:28:20  sam
// fixed bug causing random return status of unsupported functions
//
// Revision 1.12  1999/06/20 13:42:20  sam
// Fixed problem with hash op[] inserting nulls, reworked the factory ifx,
// fixed problem with modes on newly created files, cut some confusion away.
//
// Revision 1.11  1999/06/20 10:08:13  sam
// dev_*() and tc*() messages now recognized.
//
// Revision 1.10  1999/06/18 15:00:12  sam
// doing some more error checking
//
// Revision 1.9  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.8  1999/04/24 21:13:28  sam
// implemented remove on top-level causing exit of vfsys
//
// Revision 1.7  1999/04/24 04:37:06  sam
// added support for symbolic links
//
// Revision 1.6  1998/04/28 07:23:50  sroberts
// changed Handle() to Service() - the name was confusing me - and added
// readable system message names to debug output
//
// Revision 1.5  1998/04/28 01:53:13  sroberts
// implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
// a directory tree into the virtual filesystem though, checking in anyhow
//
// Revision 1.4  1998/04/06 06:47:47  sroberts
// implimented dup() and write()
//
// Revision 1.3  1998/04/05 23:50:42  sroberts
// added mkdir()
//
// Revision 1.2  1998/03/19 07:41:25  sroberts
// implimented dir stat, open, opendir, readdir, rewinddir, close
//
// Revision 1.1  1998/03/09 06:07:25  sroberts
// Initial revision
//

#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/prfx.h>
#include <sys/psinfo.h>

#include <strstream.h>

#include <wcvector.h>

#include "vf_log.h"
#include "vf_mgr.h"

//
// VFOcbMap
//

// Seems impossible to declare this as a nested class... a weird Watcom bug?

	template <class T>
	class Pointer
	{
	public:
		Pointer(T* i = 0) : i_(i) { }
		Pointer(const Pointer& i) { i_ = i.i_; }

		operator = (T* i) { i_ = i; }

		operator T* () { return i_; }

	//  int integer() { return i_; }

	private:
		T* i_;
	};


class VFOcbMap
{
public:
	VFOcbMap()
	{
		ctrl_ = __init_fd(getpid());
		assert(ctrl_);
	}

	VFOcb* Get(pid_t pid, int fd)
	{
		int index = (int) __get_fd(pid, fd, ctrl_);

		VFOcb *ocb = ocbs_[index];

		assert(ocb != (VFOcb*) -1);

		if(ocb == (VFOcb*) 0)
		{
			errno = EBADF;
			return 0;
		}
		return ocb;
	}

	bool Map(pid_t pid, int fd, VFOcb* ocb)
	{
		// increment reference count, if not mapping to null
		if(ocb) { ocb->Refer(); }

		errno = EOK;

		// find a free index number, if we're mapping an ocb, the index
		// 0 is reserved to mean "not mapped"
		int index = 0;

		if(ocb)
		{
			index = 1;
			while(ocbs_[index]) { index++; }
		}

		ocbs_[index] = ocb;

		if(qnx_fd_attach(pid, fd, 0, 0, 0, 0, index) == -1)
		{
			VFLog(2, "VFOcbMap::Map(%d, %d) failed: [%d] %s",
				pid, fd, errno, strerror(errno));

			if(ocb) { ocb->Unfer(); }
			return false;
		}
		return true;
	}

	bool UnMap(pid_t pid, int fd)
	{
		VFOcb* ocb = Get(pid, fd);

		if(!ocb) { return false; } // attempt by client to close a bad fd

		ocb->Unfer();

		// zero the mapping to invalidate the fd
		Map(pid, fd, 0);

		return true;
	}

private:
	void* ctrl_;

	WCValVector< Pointer<VFOcb> >	ocbs_;
};

//
// VFManager
//

VFManager::VFManager(const VFVersion& version) :
		version_(version)
{
}

int VFManager::Init(VFEntity* root, const char* mount, int verbosity)
{
	if(!root || !mount)	{ errno = EINVAL; return false; }

	root_ = root;
	mount_ = mount;

	// set verbosity level

	VFLevel(mount, verbosity);


	// initialize ocb map

	ocbMap_ = new VFOcbMap;

	if(!ocbMap_) { errno = ENOMEM; return false; }

	// declare ourselves as a server, and get messages in priority order

	long pflags = _PPF_PRIORITY_REC | _PPF_SERVER;

	if(qnx_pflags(pflags, pflags, 0, 0) == -1) { return false; }

	// attach prefix

	ostrstream os;

	if(mount[0] != '/')
	{
		os << getcwd(0, 0) << "/";
	}
	os << mount << '\0';

	assert(os.str()[os.pcount() - 1] == '\0');

	if(qnx_prefix_attach(os.str(), 0, 0) == -1) { return false; }

	VFLog(2, "VFManager::Init() attached %s", os.str());

	return true;
}

int VFManager::Service(pid_t pid, VFIoMsg* msg)
{
	VFLog(3, "VFManager::Service() pid %d type %s (%#x)",
		pid, MessageName(msg->type), msg->type);

	int size = -1;

	msg_t type = msg->type;

	msg->status = ENOTSUP;

	switch(type)
	{
	case _IO_STAT:
		size = root_->Stat(msg->open.path, &msg->open, &msg->fstat_reply);
		break;

	case _IO_CHDIR:
		size = root_->ChDir(msg->open.path, &msg->open, &msg->open_reply);
		break;

	case _IO_HANDLE:
		switch(msg->open.oflag)
		{
		case _IO_HNDL_RDDIR: {
			VFOcb* ocb = root_->Open(msg->open.path, &msg->open, &msg->open_reply);

			if(ocb) {
				if(!ocbMap_->Map(pid, msg->open.fd, ocb)) {
					msg->open_reply.status = errno;
				}
			}
			size = sizeof(msg->open_reply);

			// An open may cause path rewriting if entity is a symbolic link.
			if(msg->status == EMORE)
			{
				size = sizeof(msg->open) + PATH_MAX;
			}

			} break;

		default:
			VFLog(1, "unknown msg type IO_HANDLE subtype %s (%d) path \"%s\"",
				HandleOflagName(msg->open.oflag), msg->open.oflag, msg->open.path);
			msg->status = ENOSYS;
			size = sizeof(msg->status);
			break;
		}
		break;

	case _IO_OPEN: {
		VFOcb* ocb = root_->Open(msg->open.path, &msg->open, &msg->open_reply);

		if(ocb) {
			if(!ocbMap_->Map(pid, msg->open.fd, ocb)) {
				msg->open_reply.status = errno;
			}
		}

		size = sizeof(msg->open_reply);

		// An open may cause path rewriting if entity is a symbolic link.
		if(msg->status == EMORE)
		{
			size = sizeof(msg->open) + PATH_MAX;
		}

		} break;

	case _IO_CLOSE: {
		msg->open_reply.status = EOK;

		if(!ocbMap_->UnMap(pid, msg->close.fd)) {
			msg->open_reply.status = errno;
		}

		size = sizeof(msg->close_reply);

		} break;

	case _IO_DUP: {
		VFOcb* ocb = ocbMap_->Get(msg->dup.src_pid, msg->dup.src_fd);
		if(!ocb)
		{
			msg->dup_reply.status = EBADF;
		}
		else if(!ocbMap_->Map(pid, msg->dup.dst_fd, ocb))
		{
			msg->dup_reply.status = EBADF;
		}
		else
		{
			msg->dup_reply.status = EOK;
		}

		size = sizeof(msg->dup_reply);

		} break;

	case _FSYS_MKSPECIAL:
		size = root_->MkSpecial(msg->mkspec.path, &msg->mkspec, &msg->mkspec_reply);
		break;

	case _FSYS_REMOVE: {
		// not supported by entites yet, only accept if directed to top-level
		if(msg->remove.path[0] != '\0') { break; }

		// reply with EOK, then exit
		msg->status = EOK;
		Reply(pid, msg, sizeof(msg->status));

		VFLog(2, "remove - exiting vfsys manager");
		exit(0);

		} break;

	case _FSYS_READLINK:
		size = root_->ReadLink(msg->rdlink.path, &msg->rdlink, &msg->rdlink_reply);
		break;

	// operations supported directly by ocbs
	case _IO_FSTAT:
	case _IO_WRITE:
	case _IO_READ:
	case _IO_LSEEK:
	case _IO_READDIR:
	case _IO_REWINDDIR:
	{
		VFOcb* ocb = ocbMap_->Get(pid, msg->write.fd);

		if(!ocb)
		{
			msg->status = EBADF;
			size = sizeof(msg->status);
			break;
		}

		switch(type)
		{
		case _IO_FSTAT:
			size = ocb->Stat(pid, &msg->fstat, &msg->fstat_reply);
			break;
		case _IO_WRITE:
			size = ocb->Write(pid, &msg->write, &msg->write_reply);
			break;
		case _IO_READ:
			size = ocb->Read(pid, &msg->read, &msg->read_reply);
			break;
		case _IO_LSEEK:
			size = ocb->Seek(pid, &msg->seek, &msg->seek_reply);
			break;
		case _IO_READDIR:
			size = ocb->ReadDir(pid, &msg->readdir, &msg->readdir_reply);
			break;
		case _IO_REWINDDIR:
			size = ocb->RewindDir(pid, &msg->rewinddir, &msg->rewinddir_reply);
			break;
		}
	}	break;

//	case _IO_RENAME: break;
//	case _IO_GET_CONFIG: break;
//	case _IO_CHMOD: break;
//	case _IO_CHOWN: break;
//	case _IO_UTIME: break;
//	case _IO_FLAGS: break;
//	case _IO_LOCK: break;
//	case _IO_IOCTL: break;
//	case _IO_SELECT: break;
//	case _IO_QIOCTL: break;

	case _SYSMSG: {
		short unsigned subtype = msg->sysmsg.hdr.subtype;

		switch(subtype)
		{
		case _SYSMSG_SUBTYPE_VERSION:
			msg->sysmsg_reply.hdr.status	= EOK;
			msg->sysmsg_reply.hdr.zero		= 0;
			msg->sysmsg_reply.body.version	= version_;

			size = sizeof(msg->sysmsg_reply);

			break;
		
		default:
			VFLog(1, "unknown msg type SYSMSG subtype %s (%d)",
				SysmsgSubtypeName(subtype), subtype);
			msg->status = ENOSYS;
			size = sizeof(msg->status);
			break;
		}
	} break;

	default:
		VFLog(1, "unknown msg type %s (%#x)", MessageName(type), type);

		msg->status = ENOSYS;
		size = sizeof(msg->status);
		break;

	} // end switch(msg->type)

	// returning 0 is allowed, its an ok way of saying just return the default
	assert(size >= 0); // we should never see a -1 here...
	if(size <= 0) {
		size = sizeof(msg->status);
	}

	return size;
}

void VFManager::Run()
{
	VFLog(3, "VFManager::Run()");

	pid_t pid;
	int  size;

	while(1)
	{
		pid = Receive(0, &msg_, sizeof(msg_));

		size = Service(pid, &msg_);

		assert(size >= sizeof(msg_.status));

		if(Reply(pid, &msg_, size) == -1)
		{
			VFLog(1, "VFManager::Run() Reply(%d) failed: [%d] %s",
				pid, errno, strerror(errno)
				);
		}
	}
}

static const char* VFManager::MessageName(msg_t type)
{
	switch(type)
	{
	case 0x0000: return "SYSMSG";

	case 0x0101: return "IO_OPEN";
	case 0x0102: return "IO_CLOSE";
	case 0x0103: return "IO_READ";
	case 0x0104: return "IO_WRITE";
	case 0x0105: return "IO_LSEEK";
	case 0x0106: return "IO_RENAME";
	case 0x0107: return "IO_GET_CONFIG";
	case 0x0108: return "IO_DUP";
	case 0x0109: return "IO_HANDLE";
	case 0x010A: return "IO_FSTAT";
	case 0x010B: return "IO_CHMOD";
	case 0x010C: return "IO_CHOWN";
	case 0x010D: return "IO_UTIME";
	case 0x010E: return "IO_FLAGS";
	case 0x010F: return "IO_LOCK";
	case 0x0110: return "IO_CHDIR";
	case 0x0112: return "IO_READDIR";
	case 0x0113: return "IO_REWINDDIR";
	case 0x0114: return "IO_IOCTL";
	case 0x0115: return "IO_STAT";
	case 0x0116: return "IO_SELECT";
	case 0x0117: return "IO_QIOCTL";

	case 0x0202: return "FSYS_MKSPECIAL";
	case 0x0203: return "FSYS_REMOVE";
	case 0x0204: return "FSYS_LINK";
	case 0x0205: return "FSYS_MOUNT_RAMDISK";
	case 0x0206: return "FSYS_UNMOUNT_RAMDISK";
	case 0x0207: return "FSYS_BLOCK_READ";
	case 0x0208: return "FSYS_BLOCK_WRITE";
	case 0x0209: return "FSYS_DISK_GET_ENTRY";
	case 0x020A: return "FSYS_SYNC";
	case 0x020B: return "FSYS_MOUNT_PART";
	case 0x020C: return "FSYS_MOUNT";
	case 0x020D: return "FSYS_GET_MOUNT";
	case 0x020E: return "FSYS_DISK_SPACE";
	case 0x020F: return "FSYS_PIPE";
	case 0x0210: return "FSYS_TRUNC";
	case 0x0211: return "FSYS_OLD_MOUNT_DRIVER";
	case 0x0212: return "FSYS_XSTAT";
	case 0x0213: return "FSYS_MOUNT_EXT_PART";
	case 0x0214: return "FSYS_UMOUNT";
	case 0x0215: return "FSYS_RESERVED";
	case 0x0216: return "FSYS_READLINK";
	case 0x0217: return "FSYS_MOUNT_DRIVER";
	case 0x0218: return "FSYS_FSYNC";
	case 0x0219: return "FSYS_INFO";
	case 0x021A: return "FSYS_FDINFO";
	case 0x021B: return "FSYS_MOUNT_DRIVER32";

	case 0x0310: return "DEV_TCGETATTR";
	case 0x0311: return "DEV_TCSETATTR";
	case 0x0312: return "DEV_TCSENDBREAK";
	case 0x0313: return "DEV_TCDRAIN";
	case 0x0314: return "DEV_TCFLUSH";
	case 0x0315: return "DEV_TCFLOW";
	case 0x0316: return "DEV_TCGETPGRP";
	case 0x0317: return "DEV_TCSETPGRP";
	case 0x0318: return "DEV_INSERTCHARS";
	case 0x0319: return "DEV_MODE";
	case 0x031A: return "DEV_WAITING";
	case 0x031B: return "DEV_INFO";
	case 0x031C: return "DEV_ARM";
	case 0x031D: return "DEV_STATE";
	case 0x031E: return "DEV_READ";
	case 0x031F: return "DEV_WRITE";
	case 0x0320: return "DEV_FDINFO";
	case 0x0321: return "DEV_TCSETCT";
	case 0x0322: return "DEV_TCDROPLINE";
	case 0x0323: return "DEV_SIZE";
	case 0x0324: return "DEV_READEX";
	case 0x0325: return "DEV_OSIZE";
	case 0x0326: return "DEV_RESET";

	default: return "UNKNOWN";
	}
}

static const char* VFManager::HandleOflagName(short int oflag)
{
	switch(oflag)
	{
	case 1:		return "IO_HNDL_INFO";
	case 2:		return "IO_HNDL_RDDIR";
	case 3:		return "IO_HNDL_CHANGE";
	case 4:		return "IO_HNDL_UTIME";
	case 5:		return "IO_HNDL_LOAD";
	case 6:		return "IO_HNDL_CLOAD";
	default:	return "undefined";
	}
}

static const char* VFManager::SysmsgSubtypeName(short unsigned subtype)
{
	switch(subtype)
	{
	case 0:		return "DEATH";
	case 1:		return "SIGNAL";
	case 2:		return "TRACE";
	case 3:		return "VERSION";
	case 4:		return "SLIB";
	default:	return "undefined";
	}
}

