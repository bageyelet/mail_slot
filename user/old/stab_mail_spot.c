#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/kdev_t.h>
#include "util.h"

#define MS_MAJOR 250

#define SUCCESS 1
#define ERR_MAX_MS -2

int curr_mailspot = 0;

// int initialize_mailspot() {
//     if (curr_mailspot > 255) return ERR_MAX_MS;

//     char minor[4];
//     itoa(curr_mailspot, minor);
//     char path[19];
//     strcpy(path,"/dev/ms");
//     strcat(path, minor);
    
//     int   status;
//     status  = mknod(path, S_IFCHR | 0666 , (dev_t)MKDEV(MS_MAJOR, curr_mailspot));
//     curr_mailspot++;
//     printf("%d\n", status);
// }

int read() {

}

int write() {

}

int main(int argc, char const *argv[])
{
    initialize_mailspot();
    return 0;
}