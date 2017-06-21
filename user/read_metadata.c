#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "../module/actual_ms/ioctl_cmds.h"
 
int main(int argc, char const *argv[]) {

    int ret;

    int fd = open("/proc/mail_spot_metadata/get_number_messages", O_RDONLY);
    if(fd < 0)
        return 1;

    char data[20];

    ret = read(fd, data, 20);
    if (ret != 0) {
        data[ret] = '\0';
        printf("%s\n", data);
    }
    else 
        printf("read failed\n");

    ret = read(fd, data, 20);
    if (ret != 0) {
        data[ret] = '\0';
        printf("%s\n", data);
    }
    else 
        printf("read failed\n");

    ret = read(fd, data, 7);
    if (ret != 0) {
        data[ret] = '\0';
        printf("%s\n", data);
    }
    else 
        printf("read failed\n");

    close(fd);
    return 0;
}
