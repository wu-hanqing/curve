#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int fd = open("./data", O_RDWR | O_CREAT, 0644);
    char buf[4];

    assert(fd >= 0);

    lseek(fd, 0, SEEK_SET);

    lseek(fd, 4, SEEK_CUR);
    memset(buf, 0xbb, 4);
    assert(sizeof(buf) == write(fd, buf, sizeof(buf)));

    lseek(fd, 4, SEEK_CUR);
    memset(buf, 0xdd, 4);
    assert(sizeof(buf) == write(fd, buf, sizeof(buf)));

    lseek(fd, 8, SEEK_SET);
    memset(buf, 0xcc, 4);
    assert(sizeof(buf) == write(fd, buf, sizeof(buf)));

    lseek(fd, 0, SEEK_SET);
    memset(buf, 0xaa, 4);
    assert(sizeof(buf) == write(fd, buf, sizeof(buf)));

    fsync(fd);
    close(fd);

    return 0;
}