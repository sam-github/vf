//
// vf_ptr.h
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
// Revision 1.1  1999/09/27 02:45:10  sam
// Initial revision
//

#ifndef VF_PTR_H
#define VF_PTR_H

//
// VFPointer differs from the normal C pointer in that it's default value
// is well-defined (0), making it appropriate to use for containers that
// create default initialized instances of their contained types.
//

template <class T>
class VFPointer
{
public:
	VFPointer(T* i = 0) : i_(i) { }
	VFPointer(const VFPointer& i) { i_ = i.i_; }

		operator	=	(T* i)	{ i_ = i; }
		operator	T*	()		{ return i_; }
	T*	operator	->	()		{ return i_; }

private:
	T* i_;
};

#endif

