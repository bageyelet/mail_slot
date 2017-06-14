#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "../module/ioctl_cmds.h"
 
int main(int argc, char const *argv[]) {

    int fd = open("/dev/ms1", O_RDWR);
    if(fd < 0)
        return 1;

    char data[100];

    ioctl(fd, CHANGE_MAX_MEX_LEN, 2);

    write(fd, "ciao", 5); // it'll fail

    write(fd, "c", 2); // OK

    ioctl(fd, CHANGE_MAX_MEX_LEN, 5);

    write(fd, "ciao", 5); // OK

    read(fd, data, 2); // OK the first message is "c"
    printf("%s\n", data);

    read(fd, data, 5); // OK the message is "ciao"
    printf("%s\n", data);

    close(fd);
    return 0;
}
