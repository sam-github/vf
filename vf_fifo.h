//
// vf_fifo.h
//
// Copyright (C) 1999 Sam Roberts
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
// Revision 1.2  1999/09/27 02:45:31  sam
// made some members const that should've been
//
// Revision 1.1  1999/08/09 15:12:51  sam
// Initial revision
//

#ifndef VF_FIFO_H
#define VF_FIFO_H

#include <wclist.h>


template <class T>
class VFFifo
{
public:
	VFFifo() {}
	~VFFifo() {}

	void Push(T* t)
	{
		imp_.append(t);
	}
	T* Pop()
	{
		return imp_.get();
	}
	T* Peek() const
	{
		return imp_.find();
	}
	int Size() const
	{
		return imp_.entries();
	}

	typedef WCSLink Link;

private:
	WCIsvSList< T > imp_;
};

#endif

