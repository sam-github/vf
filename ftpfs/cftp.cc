#include <ftp.h>

// command-line ftp file retrieval utility

extern "C" char * getpass(const char*);

int main (int ac, char** av)
{
  if (ac < 4) {
    cerr << "usage: " << av [0] << " hostname user directory [password]\n";
    return 1;
  }

  ftp f (&cout);

  // establish connection
  cout << 
  f->connect (av [1]);
  f->get_response (); // get the connection response

  // set access 
  f->user (av [2]);
  f->passwd (ac >= 5 ? av[4] : getpass ("passwd: "));

  // set representation type to image
  f->rep_type (ftp::rt_image);

  // get file
  f->list(av[3], 0);

  // quit
  f->quit ();

  return 0;
}

