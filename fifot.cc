// tfifo: tests the FIFO class

#ifdef __USAGE
usage: fifot
  Runs automatic test of the VFFifo container class.
#endif

#include <stdio.h>

#include "vf_fifo.h"

struct Link: public VFFifo<Link>::Link
{
	Link(char* s) : s(s) {
		printf("Link(%s)\n", s);
	}

	char* s;
};

typedef VFFifo<Link> LinkFifo;

int main(int argc, char* argv[], char* envp[])
{
	argc = argc, argv = argv;

	LinkFifo fifo;

	for(int i = 0; envp[i]; i++) {
		fifo.Push(new Link(envp[i]));
	}

	while(fifo.Size()) {
		Link* l;

		l = fifo.Peek();
		printf("peek: %s\n", l->s);

		l = fifo.Pop();
		printf("pop: %s\n", l->s);
	}

	return 0;
}

