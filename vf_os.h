//
// vf_os.h
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
// Revision 1.1  1999/09/27 02:52:43  sam
// Initial revision
//

#ifndef VF_OS_H
#define VF_OS_H

#if 0
//
// Receive wrapper that blocks a sigset except for during the Receive()
//

inline VFReceive(pid_t pid, void* data, int len, sigset_t mask)
{
	sigset_t	saved;
	sigset_t	masked;
}
#endif

//
// utility class to handle reaping of children
//

extern "C" char* strsignal(int);
extern "C" char* strerror(int);

#include <sys/wait.h>

/*

Child status is a 16-bit field:

|  exit status  | | term signal |
15             8 7 6           0

Exit status is 8-bit, term signal is 7-bit.

Examination of the macros and docs shows that wait will never return
a status indicating that a child has been stopped, the docs must just
be there for POSIX.1 compliance. Either the term signal is non-zero
and valid, in which case the child was terminated by a signal, or the
exit status is valid. The code below, however, does not rely on
this knowledge.

*/

class VFExitStatus
{
public:
    VFExitStatus(int status = 0, pid_t pid = -1) : status_(status), pid_(pid) {
    }

	int Wait() {
		return Wait(-1 /* any child */);
	}
	int Wait(pid_t pid) {
		pid_ = waitpid(pid, &status_, WNOHANG);
		return pid_;
	}

	int BlockingWait() {
		return Wait(-1 /* any child */);
	}
	int BlockingWait(pid_t pid) {
		pid_ = waitpid(pid, &status_, 0);
		return pid_;
	}

	int Ok() const {
		return Exited() && Number() == 0;
	}

    int Status() const {
        return status_;
    }
    pid_t Pid() const {
    	return pid_;
    }
    int Exited() const {
        return WIFEXITED(status_);
    }
    int Signaled() const {
        return WIFSIGNALED(status_);
    }
    int Stopped() const {
        return WIFSTOPPED(status_);
    }
    const char* Reason() const {
        if(Exited()) {
            return "exited";
        } else if(Signaled()) {
            return "signaled";
        } else if(Stopped()) {
            return "stopped";
        } else {
            return "unknown"; // could happen if not really a process status
        }
    }
    int Number() const {
        if(Exited()) {
            return WEXITSTATUS(status_);
        } else if(Signaled()) {
            return WTERMSIG(status_);
        } else if(Stopped()) {
            return WSTOPSIG(status_);
        } else {
            return status_; // could happen if not really a process status
        }
    }
    const char* Name() const {

        if(Exited()) {
            return strerror(Number());
        } else if(Signaled() || Stopped()) {
            return strsignal(Number());
        } else {
            return "unknown";
        }
    }

private:
	pid_t	pid_;
    int		status_;
};

#endif

