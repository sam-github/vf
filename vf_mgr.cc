//
// vf_fsys.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
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

#include <sys/kernel.h>
#include <sys/prfx.h>

#include <strstream.h>

#include "vf_log.h"
#include "vf_mgr.h"

//
// VFOcbMap
//

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
		VFOcb *ocb = (VFOcb*) __get_fd(pid, fd, ctrl_);
		if(ocb == (VFOcb*) 0 || ocb == (VFOcb*) -1)
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

		// the documentation says that we CAN'T use 32-bit pointers as the handle
		// since it's a 16 bit value... but they do it in the sample, so is it OK?
		errno = EOK;
		if(qnx_fd_attach(pid, fd, 0, 0, 0, 0, (unsigned) ocb) == -1)
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
};

//
// VFManager
//

int VFManager::Init(VFEntity* root, const char* mount, int verbosity)
{
	root_ = root;
	mount_ = mount;

	VFLevel(mount, verbosity);

	ocbMap_ = new VFOcbMap;

	if(!ocbMap_) { errno = ENOMEM; return false; }

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

int VFManager::Handle(pid_t pid, VFIoMsg* msg)
{
	VFLog(3, "VFManager::Handle(%d, %#x)", pid, msg->type);

	int size = -1;

	switch(msg->type)
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
			size = sizeof msg->open_reply;
			} break;

		default:
			msg->status = ENOSYS;
			size = sizeof msg->status;
			break;
		}
		break;

	case _IO_READDIR: {
		VFOcb* ocb = ocbMap_->Get(pid, msg->readdir.fd);

		if(!ocb)
		{
			msg->status = EBADF;
			size = sizeof(msg->status);
			break;
		}

		size = ocb->ReadDir(&msg->readdir, &msg->readdir_reply);

		} break;

	case _IO_OPEN: {
		VFOcb* ocb = root_->Open(msg->open.path, &msg->open, &msg->open_reply);

		if(ocb) {
			if(!ocbMap_->Map(pid, msg->open.fd, ocb)) {
				msg->open_reply.status = errno;
			}
		}

		size = sizeof(msg->open_reply);

		} break;

	case _IO_CLOSE: {
		msg->open_reply.status = EOK;

		if(!ocbMap_->UnMap(pid, msg->close.fd)) {
			msg->open_reply.status = errno;
		}

		size = sizeof(msg->close_reply);

		} break;

	case _FSYS_MKSPECIAL:
		size = root_->MkDir(msg->mkdir.path, &msg->mkdir, &msg->mkdir_reply);
		break;

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

	case _IO_WRITE: {
		VFOcb* ocb = ocbMap_->Get(pid, msg->write.fd);

		if(!ocb)
		{
			msg->status = EBADF;
			size = sizeof(msg->status);
			break;
		}

		size = ocb->Write(pid, &msg->write, &msg->write_reply);

		} break;

//	case _IO_READ: break;
//	case _IO_LSEEK: break;
//	case _IO_RENAME: break;
//	case _IO_GET_CONFIG: break;
//	case _IO_FSTAT: break;
//	case _IO_CHMOD: break;
//	case _IO_CHOWN: break;
//	case _IO_UTIME: break;
//	case _IO_FLAGS: break;
//	case _IO_LOCK: break;
//	case _IO_REWINDDIR: break;
//	case _IO_IOCTL: break;
//	case _IO_SELECT: break;
//	case _IO_QIOCTL: break;

	default:
		VFLog(1, "unknown msg type %#x", msg->type);

		msg->status = ENOSYS;
		size = sizeof(msg->status);
		break;

	} // end switch(msg->type)

	assert(size != -1);

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

		size = Handle(pid, &msg_);

		assert(size >= sizeof(msg_.status));

		if(Reply(pid, &msg_, size) == -1)
		{
			VFLog(1, "VFManager::Run() Reply(%d) failed: [%d] %s",
				pid, errno, strerror(errno)
				);
		}
	}
}

