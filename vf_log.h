//
// vf_log.h
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.1  1998/03/15 22:10:32  sroberts
// Initial revision
//

#ifndef VF_LOG_H
#define VF_LOG_H

void VFLog (int level, const char* format, ...);
void VFLevel (const char* tag, int level);

#endif

