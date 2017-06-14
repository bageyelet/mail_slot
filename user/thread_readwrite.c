#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "../module/ioctl_cmds.h"

#define N 50
#define M 5000
int blocking = 1;

void *write_fun(void *args) {
    int fd = *(int*)args;
    int i;
    for (i=0; i<M; i++) {
        write(fd, "ciao\n", 6);
        // sleep(1);
    }
}

void *read_fun(void* args) {
    int fd = *(int*)args;
    char data[512];
    int i;
    for (i=0; i<M; i++) {
        read(fd, data, 512);
        // sleep(1);
    }
}

 
int main(int argc, char const *argv[]) {
    int i;

    if (argc > 1) blocking = 0;

    int fd = open("/dev/ms1", O_RDWR);
    if(fd < 0)
        return 1;

    ioctl(fd, CHANGE_MAX_MEX_LEN, 512);

    if (!blocking)
        ioctl(fd, NON_BLOCKING_READ);
    else
        ioctl(fd, BLOCKING_READ);

    pthread_t threads_write[N];
    pthread_t threads_read[N];
    for (i=0; i<N; i++) {
        if(pthread_create(&threads_write[i], NULL, write_fun, &fd)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        if(pthread_create(&threads_read[i], NULL, read_fun, &fd)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for (i=0; i<N; i++) {
        if(pthread_join(threads_write[i], NULL)) { 
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
        if(pthread_join(threads_read[i], NULL)) { 
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
    }

    close(fd);
    return 0;
}
