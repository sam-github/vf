//
// vf.cc
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
// Revision 1.2  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.1  1998/03/19 07:43:30  sroberts
// Initial revision
//

#include "vf.h"
#include "vf_log.h"

VFEntity::~VFEntity()
{
	VFLog(3, "VFEntity::~VFEntity()");
}

VFOcb::VFOcb() :
	refCount_(0)
{
	VFLog(3, "VFOcb::VFOcb()");
}

VFOcb::~VFOcb()
{
	VFLog(3, "VFOcb::~VFOcb()");
}

