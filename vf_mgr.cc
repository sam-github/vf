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
// Revision 1.19  1999/10/17 16:20:56  sam
// Added support for umount().
//
// Revision 1.18  1999/10/04 03:31:38  sam
// added mechanism to fork and wait for child to the manager
//
// Revision 1.17  1999/09/27 02:51:02  sam
// made servers priority float, and tweaked verbosity of a log message
//
// Revision 1.16  1999/08/09 15:12:51  sam
// To allow blocking system calls, I refactored the code along the lines of
// QSSL's iomanager2 example, devolving more responsibility to the entities,
// and having the manager and ocbs do less work.
//
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

#include <signal.h>
#include <stdlib.h>
#include <sys/kernel.h>
#include <sys/prfx.h>
#include <sys/psinfo.h>

#include <strstream.h>

#include <wcvector.h>

#include "vf.h"
#include "vf_log.h"
#include "vf_fdmap.h"
#include "vf_mgr.h"

//
// VFManager
//

VFManager::VFManager(const VFVersion& version) :
	version_(version), root_(0), mount_(0), parent_(-1)
{
}

int VFManager::Init(VFEntity* root, const char* mount, int verbosity)
{
	if(!root || !mount)	{ errno = EINVAL; return 0; }

	root_ = root;

	// set verbosity level

	VFLevel(mount, verbosity);

	// set our process flags
	long pflags =
		  _PPF_PRIORITY_REC		// receive requests in priority order
		| _PPF_SERVER			// we're a server, send us version requests
		| _PPF_PRIORITY_FLOAT	// float our priority to clients
		//| _PPF_SIGCATCH		// catch our clients signals, to clean up
		;

	if(qnx_pflags(pflags, pflags, 0, 0) == -1) { return 0; }

	// if our mount point is relative, prefix the cwd to it, and chop trailing
	// '/' characters from the mount point

	ostrstream os;

	if(mount[0] != '/')
	{
		os << getcwd(0, 0) << "/";
	}
	os << mount << '\0';

	char* m = os.str();
	while(strlen(m) > 0 && m[strlen(m) - 1] == '/') {
		m[strlen(m) - 1] = '\0';
	}
	mount_ = m;

	return 1;
}

void VFManager::Start(int nodaemon)
{
	if(nodaemon == 0)
	{
		pid_t	child = fork();

		switch(child)
		{
		case -1:
			VFLog(0, "VFManager::Start() fork() failed: [%d] %s", VFERR(errno));
			exit(1);

			// the child will become the daemon
		case 0:
			if(Receive(getppid(), 0, 0) == -1) {
				VFLog(0, "VFManager::Start() Receive(%d) failed: [%d] %s",
					getppid(), VFERR(errno));
				exit(1);
			}
			// do daemony stuff
			setsid();
			close(0);

			signal(SIGHUP, SIG_IGN);
			signal(SIGINT, SIG_IGN);

			// remember to notify parent when we Run()
			parent_ = getppid();

			break;

			// the parent will wait for the child to indicate that it has
			// started running
		default:
			if(Send(child, 0, 0, 0, 0) == -1) {
				VFLog(0, "VFManager::Start() Send() failed: [%d] %s",
					VFERR(errno));
				exit(1);
			}
			exit(0);
		}
	}
}

void VFManager::Run()
{
	VFLog(3, "VFManager::Run()");

	if(!mount_ || qnx_prefix_attach(mount_, 0, 0) == -1) {
		VFLog(0, "VFManager::Start() qnx_prefix_attach(%s) failed: [%d] %s",
			mount_, VFERR(errno));
		exit(1);
	}

	// free up our parent, if we have one
	if(parent_ != -1) { Reply(parent_, 0, 0); }

	pid_t pid;
	int  status;

	while(1)
	{
		pid = Receive(0, &msg_, sizeof(msg_));

		if(pid == -1) {
			if(errno != EINTR) {
				VFLog(2, "Receive() failed: [%d] %s", strerror(errno));
			}
			continue;
		}

		status = Service(pid, &msg_);

		VFLog(4, "VFManager::Run() serviced with status %d (%s)",
			status, status == -1 ? "none" : strerror(status));

		if(status >= EOK) {
			msg_.status = status;
			ReplyMsg(pid, &msg_, sizeof(msg_.status));
		}
	}
}

int VFManager::Service(pid_t pid, VFMsg* msg)
{
	VFLog(4, "VFManager::Service() pid %d type %s (%#x)",
		pid, MessageName(msg->type), msg->type);

	int status = -1;

	switch(msg->type)
	{
	case _IO_STAT:
		status = root_->Stat(pid, msg->open.path,
			msg->open.mode == S_IFLNK && !msg->open.eflag);
		break;

	case _IO_CHDIR:
		status = root_->ChDir(pid, msg->open.path);
		break;

	case _IO_HANDLE:
		// XXX handle must have different permissions checks than Open!
		// XXX the uname, chmod, chown calls should be supported!

		switch(msg->open.oflag)
		{
		case _IO_HNDL_RDDIR:
			status = root_->Open(pid, msg->open.path, msg->open.fd,
									msg->open.oflag, msg->open.mode);

			break;

		default:
			VFLog(1, "unknown msg type IO_HANDLE subtype %s (%d) path \"%s\"",
				HandleOflagName(msg->open.oflag), msg->open.oflag, msg->open.path);
			status = ENOSYS;
			break;
		}
		break;

	case _IO_OPEN:
		status = root_->Open(pid, msg->open.path, msg->open.fd,
								msg->open.oflag, msg->open.mode);

		break;

	case _IO_CLOSE:
		status = EOK;

		if(!VFFdMap::Fd().UnMap(pid, msg->close.fd)) {
			status = errno;
		}

		break;

	case _IO_DUP: {
		VFOcb* ocb = VFFdMap::Fd().Get(msg->dup.src_pid, msg->dup.src_fd);

		if(!ocb)
		{
			status = EBADF;
		}
		else if(!VFFdMap::Fd().Map(pid, msg->dup.dst_fd, ocb))
		{
			status = errno;
		}
		else
		{
			status = EOK;
		}

		} break;


	case _FSYS_MKSPECIAL:
		// decode this to CreateFile/Symlink/Directory/etc.
		status = root_->MkSpecial(pid, msg->mkspec.path, msg->mkspec.mode,
					msg->mkspec.path + PATH_MAX + 1);
		break;

	case _FSYS_UMOUNT:
	case _FSYS_REMOVE: {
		// not supported by entites yet, only accept if directed to top-level
		if(msg->remove.path[0] != '\0') {
			switch(msg->type)
			{
				case _FSYS_UMOUNT: status = EINVAL; break;
				case _FSYS_REMOVE: status = ENOTSUP; break;
			}
			break;
		}

		// reply with EOK, then exit
		msg->status = EOK;
		ReplyMsg(pid, msg, sizeof(msg->status));

		VFLog(2, "remove - exiting vfsys manager");
		exit(0);

		} break;

	case _FSYS_READLINK:
		status = root_->ReadLink(pid, msg->rdlink.path);
		break;

	// operations supported directly by ocbs
	case _IO_FSTAT:
	case _IO_WRITE:
	case _IO_READ:
	case _IO_LSEEK:
	case _IO_READDIR:
	case _IO_REWINDDIR:
	{
		// These messages all have the fd at the same offset, so the
		// following works:
		VFOcb* ocb = VFFdMap::Fd().Get(pid, msg->write.fd);

		if(!ocb)
		{
			status = EBADF;
			break;
		}

		switch(msg->type)
		{
		case _IO_FSTAT:
			status = ocb->Fstat(pid);
			break;
		case _IO_WRITE:
			status = ocb->Write(pid, msg->write.nbytes,
							&msg->write.data[0], sizeof(*msg) - sizeof(msg->write));
			break;
		case _IO_READ:
			status = ocb->Read(pid, msg->read.nbytes);
			break;
		case _IO_LSEEK:
			status = ocb->Seek(pid, msg->seek.whence, msg->seek.offset);
			break;
		case _IO_READDIR:
			status = ocb->ReadDir(pid, msg->readdir.ndirs);
			break;
		case _IO_REWINDDIR:
			status = ocb->RewindDir(pid);
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

			Reply(pid, msg, sizeof(msg->sysmsg_reply));

			break;
		
		default:
			VFLog(1, "unknown msg type SYSMSG subtype %s (%d)",
				SysmsgSubtypeName(subtype), subtype);
			status = ENOSYS;
			break;
		}
	} break;

	default:
		VFLog(1, "unknown msg type %s (%#x)", MessageName(msg->type), msg->type);

		status = ENOSYS;
		break;

	} // end switch(msg->type)

	return status;
}

void VFManager::ReplyMsg(pid_t pid, const void* msg, size_t size)
{
	if(Reply(pid, msg, size) == -1) {
		VFLog(1, "VFManager::Reply() Reply(%d) failed: [%d] %s",
			pid, errno, strerror(errno)
			);
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

