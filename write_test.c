#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

const size_t size = 16 * 1024;

int main(int argc, char* argv[]) {
    srand(time(NULL));

    // char c1 = rand() % 128;
    // char c2 = rand() % 127;
    // char c3 = rand() % 126;
    // char c4 = rand() % 128;

    char* writebuf = (char*)malloc(size);
    // memset(writebuf, c1, 4096);
    // memset(writebuf + 4096, c2, 4096);
    // memset(writebuf + 8192, c3, 4096);
    // memset(writebuf + 12288, c4, 4096);

    int randomfd = open("/dev/urandom", O_RDONLY);
    assert(randomfd >= 0);
    assert(size == read(randomfd, writebuf, size));
    close(randomfd);

    int fd = open("./data", O_CREAT | O_RDWR, 0644);
    assert(fd >= 0);

    assert(size == write(fd, writebuf, size));

    char* readbuf = (char*)malloc(size);
    memset(readbuf, 0, size);

    assert(4096 == pread(fd, readbuf, 4096, 0));
    assert(4096 == pread(fd, readbuf + 4096, 4096, 4096));
    assert(4096 == pread(fd, readbuf + 8192, 4096, 8192));
    assert(4096 == pread(fd, readbuf + 12288, 4096, 12288));

    assert(0 == strncmp(writebuf, readbuf, size));

    fsync(fd);
    close(fd);
    return 0;
}