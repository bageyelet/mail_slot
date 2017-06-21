#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "../module/actual_ms/ioctl_cmds.h"
 
int main(int argc, char const *argv[]) {

    int fd = open("/dev/ms1", O_RDWR);
    if(fd < 0) {
        printf("failed\n");
        return 1;
    }

    close(fd);
    return 0;
}
