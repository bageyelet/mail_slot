#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "../module/actual_ms/ioctl_cmds.h"
 
int main(int argc, char const *argv[]) {

    int fd = open("/dev/ms1", O_RDWR);
    if(fd < 0)
        return 1;

    char data[100];

    read(fd, data, 100);

    write(fd, "ciao", 5);

    read(fd, data, 100);

    printf("%s\n", data);

    close(fd);
    return 0;
}
