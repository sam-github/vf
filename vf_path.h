//
//  vf_path.h
//
// Copyright (c) 1999, Sam Roberts
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
// Revision 1.1  1999/12/05 01:50:24  sam
// Initial revision
//

#ifndef VF_PATH_H
#define VF_PATH_H

#include <assert.h>
#include <string.h>

class Path
{
private:
	char* p_;

	void Dtor()
	{
		delete[] p_;
		p_ = 0;
	}
	void Ctor(const char* p, size_t l = -1)
	{
		if(!p) {
			p = "";
		}
		size_t sz = strlen(p);

		if(l == -1 || l > sz) {
			l = sz;
		}
		p_ = new char[l + 1];
		assert(p_);
		strncpy(p_, p, l);
		p_[l] = '\0'; // strncpy doesn't null terminate...
	}

public:
	Path(const char* p = "") : p_(0)
	{
		Ctor(p);
	}
	Path(const char* p, size_t l) : p_(0)
	{
		Ctor(p, l);

	}
	Path(const Path& p) : p_(0)
	{
		assert(p.p_);
		Ctor(p.p_);
	}
	~Path()
	{
		Dtor();
	}
	Path & operator = (const char* p)
	{
		Dtor();
		Ctor(p);
		return *this;
	}
	Path & operator = (const Path& p)
	{
		Dtor();
		Ctor(p.p_);
		return *this;
	}
	size_t size() const
	{
		return strlen(p_);
	}
	const char* c_str() const
	{
		return p_;
	}
	int index(const char* p) const
	{
		char* s = 0;
		if(p && p_) {
			s = strstr(p_, p);
		}
		return s ? s - p_ : -1;
	}
	int index(const Path& p) const
	{
		return index(p.p_);
	}
	Path operator () (size_t b, size_t w) const
	{
		size_t	l = p_ ? strlen(p_) : 0;

		const char* s = p_;

		if(!l || b >= l) {
			b = 0;
			w = 0;
		}
		s += b;

		if(w == -1) {
			w = l - b;
		}
		return Path(s, w);
	}

	//Path & operator += (const char* p);
	//Path & operator += (const Path& p);

	int compare(const char* s) const
	{
		const char* p = p_;

		if(!s) { s = ""; }
		if(!p) { p = ""; }
		
		return ::strcmp(p, s);
	}
	int compare(const Path& p) const
	{
		return compare(p.p_);
	}

#if 0

	char & operator () ( size_t __pos );        // reference-to-char operator
	char const & operator () ( size_t __pos ) const;  // reference-to-char operator
	char & operator [] ( size_t __pos );        // subscript operator
	char const & operator [] ( size_t __pos ) const;  // subscript operator

		// concatenate string operators
	friend Path operator + (Path const &, Path const & );	// Path + Path
	friend Path operator + (char, Path const & );		// character + Path
#endif
};

//
// iostream operators
//


/*
inline istream& operator >> (istream& is, Path& p)
{
	return is;
}
*/

class ostream;

inline ostream& operator << (ostream& os, const Path& p)
{
	return os << p.c_str();
}

//
// operator == ()
//

inline int operator == (const Path& l, const Path& r)
{
	return l.compare(r) == 0;
}
inline int operator == (const Path& l, const char* r)
{
	return l.compare(r) == 0;
}
inline int operator == (const char* l, const Path& r)
{
	return r.compare(l) == 0;
}

//
// operator != ()
//

inline int operator != (const Path& l, const Path& r)
{
	return !(l == r);
}
inline int operator != (const Path& l, const char* r)
{
	return !(l == r);
}
inline int operator != (const char* l, const Path& r)
{
	return !(l == r);
}

#endif

