//
// vf_fdmap.h
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
// Revision 1.1  1999/08/09 15:12:51  sam
// Initial revision
//

#ifndef VF_FDMAP_H
#define VF_FDMAP_H

#include <stdlib.h>
#include <sys/fd.h>

#include <wcvector.h>

#include "vf.h"
#include "vf_log.h"

//
// VFPointer differs from the normal C pointer in that it's default value
// is well-defined (0), making it appropriate to use for containers that
// create default initialized instances of their contained types.
//
// Seems impossible to declare this as a nested class of VFFdMap... a
// weird Watcom bug?

	template <class T>
	class VFPointer
	{
	public:
		VFPointer(T* i = 0) : i_(i) { }
		VFPointer(const VFPointer& i) { i_ = i.i_; }

		operator = (T* i) { i_ = i; }

		operator T* () { return i_; }

	//  int integer() { return i_; }

	private:
		T* i_;
	};


class VFFdMap
{
public:
	static VFFdMap& Fd();

	VFOcb* Get(pid_t pid, int fd)
	{
		int index = (int) __get_fd(pid, fd, ctrl_);

		VFOcb *ocb = map_[index];

		assert(ocb != (VFOcb*) -1);

		if(ocb == 0)
		{
			errno = EBADF;
			return 0;
		}
		return ocb;
	}

	int Map(pid_t pid, int fd, VFOcb* ocb)
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
			while(map_[index]) { index++; }
		}


		if(qnx_fd_attach(pid, fd, 0, 0, 0, 0, index) == -1)
		{
			VFLog(1, "VFFdMap::Map() qnx_fd_attach(pid %d fd %d) failed: [%d] %s",
				pid, fd, errno, strerror(errno));

			if(ocb) { ocb->Unfer(); }
			return 0;
		}

		// XXX check for failure?
		map_[index] = ocb;

		return 1;
	}

	int UnMap(pid_t pid, int fd)
	{
		VFOcb* ocb = Get(pid, fd);

		if(!ocb) { return 0; } // attempt by client to close a bad fd

		// zero the mapping to invalidate the fd
		if(!Map(pid, fd, 0)) { return 0; };

		// map no longer has this reference to the ocb
		ocb->Unfer();

		return 1;
	}

private:

	VFFdMap()
	{
		ctrl_ = __init_fd(getpid());

		if(!ctrl_) {
			VFLog(0, "VFFdMap::VFFdMap() failed: __init_fd() returned null");
			exit(1);
		}

	}

	VFFdMap(const VFFdMap&); // explicitly undefined
	VFFdMap& operator = (const VFFdMap&); // explicitly undefined

	void* ctrl_;

	WCValVector< VFPointer<VFOcb> >	map_;
};

#endif

