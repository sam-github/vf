//
// vf.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
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

