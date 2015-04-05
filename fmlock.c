#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int heat_the_cache(int fd)
{
	char buf[1024];
	int rv;
	do {
	retry_read:
		rv = read(fd, buf, sizeof(buf));
		if( -1 == rv
		 && EINTR == errno ) {
			goto retry_read;
		}
	} while( 0 < rv );

	return rv != 0;
}

int main(int argc, char *argv[])
{
	int i, fd_null, n_fileslocked;
	fd_set phony_fdset;
	char *buf;

	if( 2 > argc ) {
		fprintf(stderr,
			"Usage:\n%s [filenames]\n",
			argv[0] );
		return 1;
	}

	n_fileslocked = 0;
	for(i = 1; i < argc; ++i) {
		char const * const filename = argv[i];
		int fd;
		struct stat st;
		void *ptr;

	retry_open:
		fd = open(filename, O_RDONLY);
		if( -1 == fd ) {
			if( EINTR == errno ) {
				goto retry_open;
			} else
			{
				fprintf(stderr,
					"error open('%s'): %s\n",
					filename,
					strerror(errno) );
				continue;
			}
		}

		if( -1 == fstat(fd, &st) ) {
			fprintf(stderr,
				"error fstat(fd['%s']): %s\n",
				filename,
				strerror(errno) );
			goto finish_file;
		}

		ptr = mmap(
			NULL,
			st.st_size,
			PROT_READ,
			MAP_SHARED | MAP_LOCKED,
			fd,
			0 );
		if( MAP_FAILED == ptr ) {
			fprintf(stderr,
				"error mmap(fd['%s'], 0..%lld): %s\n",
				filename,
				(long long)st.st_size,
				strerror(errno) );
			goto finish_file;
		}

		if( -1 == mlock(ptr, st.st_size) ) {
			fprintf(stderr,
				"error mlock(ptr[fd['%s']]=%p): %s\n",
				filename,
				ptr,
				strerror(errno) );
			goto finish_file;
		}
		++n_fileslocked;

		heat_the_cache(fd);

	finish_file:
		close(fd);
	}

	if( !n_fileslocked ) {
		return 1;
	}

	fprintf(stderr, "Files locked and cache heated up. Going to sleep, .zZ...\n");

	/* At this point the program shall sleep until a terminating
	 * signal arrives. To do so a nice side effect of the definition
	 * of /dev/null behavior is used: read on a /dev/null fd always
	 * return 0, which correspond to EOF which is a "no content
	 * available for read (yet)" situation for which select waits.
	 * So by selecting for a read a fd on /dev/null we can put the
	 * process to sleep. */
	fd_null = open("/dev/null", O_RDONLY);
	if( -1 == fd_null ) {
		fprintf(stderr,
			"error open('/dev/null'): %s\n",
			strerror(errno) );
		return 1;
	}
	FD_ZERO(&phony_fdset);
	FD_SET(fd_null, &phony_fdset);
	if( -1 == select(fd_null, &phony_fdset, NULL, NULL, NULL) ) {
		fprintf(stderr,
			"error select(...): %s\n",
			strerror(errno) );
		return 1;
	}

	return 0;
}
