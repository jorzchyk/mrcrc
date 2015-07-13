#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include "crc16.h"
#include "worker.h"

/* Block devices return zero size in struct stat,
   and sizes from /proc/partitions are in 1024-blocks */
off_t parsesize(const char* str)
{
	off_t sz = (*str == '+' ? atoll(str + 1) * 1024 : atoll(str));

	if(sz <= 0)
		errx(-1, "non-positive file size %li \"%s\"", sz, str);

	return sz;
}

off_t statfdsize(int fd, const char* filename)
{
	struct stat st;

	if(fstat(fd, &st))
		err(-1, "stat(%s)", filename);

	if(!st.st_size)
		errx(-1, "stat(%s) returns zero size", filename);

	return st.st_size;
}

int openx(const char* filename, int flags)
{
	int fd = open(filename, flags, 0644);

	if(fd < 0)
		err(-1, "open(%s)", filename);

	return fd;
}

int notempty(pid_t* children, int nchld)
{
	int i;

	for(i = 0; i < nchld; i++)
		if(children[i] > 0)
			return 1;

	return 0;
}

void terminate(pid_t* children, int nchld)
{
	int i;

	for(i = 0; i < nchld; i++)
		if(children[i] > 0)
			kill(children[i], SIGTERM);
}

int handledead(pid_t* children, int nchld, pid_t dead, int status)
{
	int i;

	for(i = 0; i < nchld; i++)
		if(children[i] == dead)
			children[i] = status ? -1 : 0;
	if(!status)
		return 0;

	warnx("worker process died unexpectedly, terminating");
	return -1;
}

int messy(pid_t* children, int nchld)
{
	int i;

	for(i = 0; i < nchld; i++)
		if(children[i] < 0)
			return 1;

	return 0;
}

void* mmapx(int fd, size_t size, int proto, int flags, const char* filename)
{
	uint8_t* ptr = mmap(NULL, size, proto, flags, fd, 0);

	if(ptr == MAP_FAILED)
		err(-1, "mmap(%s, %lu) failed", filename, size);

	return ptr;
}

uint8_t* mapinput(int fd, off_t size, const char* filename)
{
	uint8_t* ptr = mmapx(fd, size, PROT_READ, MAP_SHARED, filename);
	madvise(ptr, size, MADV_SEQUENTIAL);
	return ptr;
}

count_t* mapoutput(int fd, off_t size, const char* filename)
{
	if(ftruncate(fd, size))
		err(-1, "truncate(%s)", filename);

	count_t* ptr = mmapx(fd, size, PROT_WRITE, MAP_SHARED, filename);
	memset(ptr, 0, size);
	return ptr;
}

pid_t spawnworker(int i, int n, uint8_t* input, off_t inputsize, count_t* output)
{
	pid_t pid = fork();

	if(pid < 0)
		warn("fork");
	if(pid)
		return pid;

	exit(worker(i, n, input, inputsize, output));
}

int main(int argc, char** argv)
{
	int i;

	if(argc < 4)
		errx(-1, "missing arguments");

	int nproc = atoi(argv[1]);
	char* inputname = argv[2];
	char* outputname = argv[3];

	int inputfd = openx(inputname, O_RDONLY);
	int outputfd = openx(outputname, O_RDWR | O_CREAT);

	off_t inputsize = argc > 4 ? parsesize(argv[4]) : statfdsize(inputfd, inputname);
	off_t outputsize = DISTINCT_CRC_VALUES * sizeof(count_t);

	uint8_t* input = mapinput(inputfd, inputsize, inputname);
	count_t* output = mapoutput(outputfd, outputsize, outputname);
	
	pid_t children[nproc];
	pid_t dead;

	for(i = 0; i < nproc; i++)
		if((children[i] = spawnworker(i, nproc, input, inputsize, output)) < 0)
			break;

	int status;
	while(notempty(children, nproc))
		if((dead = wait(&status)) > 0)
			if(handledead(children, nproc, dead, status))
				break;

	terminate(children, nproc);
	return messy(children, nproc) ? -1 : 0;
}
