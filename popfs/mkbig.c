#include <stdio.h>
#include <unistd.h>

void Usage(FILE* file)
{
	fprintf(file, "usage: mkbig [-h] [-n size]\n");
}

#ifdef __USAGE
usage: mkbig [-h] [-n size]
#endif

unsigned strtoul_suf(char* s, unsigned* lp)
{
	char* e = 0;

	unsigned l = strtoul(s, &e, 0);

	switch(*e)
	{
	case '\0':
		break;
	case 'k':
	case 'K':
		l *= 1024;
		e++;
		break;
	case 'm':
	case 'M':
		l *= 1024 * 1024;
		e++;
		break;
	default:
		break;
	}
	if(*e != 0)
		return -1;

	*lp = l;
	return 0;
}

int main(int argc, char* argv[])
{
	int opt;
	unsigned	size = 0;
	int	nbytes = 0;

	while((opt = getopt(argc, argv, "hn:")) != -1)
	{
		switch(opt)
		{
		case 'h':
			Usage(stdout);
			exit(0);
		case 'n':
			strtoul_suf(optarg, &size);
			break;
		default:
			Usage(stderr);
			exit(1);
		}
	}

	size /= 32;

	while(nbytes++ < size)
	{
		printf("%31.31d\n", nbytes);
	}

	return 0;
}

