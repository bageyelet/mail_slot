#define EXPORT_SYMTAB
#include <linux/types.h>
#include <linux/sched.h> // serve per current->pid
#include <linux/version.h>  /* For LINUX_VERSION_CODE */
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include "mexlist.h"
#include "ioctl_cmds.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Borzacchiello");
#define DEVICE_NAME "mail_spot"
#define MODNAME "MAIL SPOT"

#define SUCCESS 0

#define DEBUG

#define MAX_MEX_LEN 512
#define MAX_INSTANCES 256

#define DEFAULT_BLOCKING_READ 1

static int Major;
mex_list messages[MAX_INSTANCES];
mex_node head[MAX_INSTANCES];
mex_node tail[MAX_INSTANCES];

struct semaphore sems[MAX_INSTANCES];
struct semaphore sems_blocking_read[MAX_INSTANCES];

// static int blocking_read = 1;
// module_param(blocking_read,int,0666);

typedef struct session_data {
    int minor;
    int blocking_read;
} session_data;

static int mail_spot_open(struct inode* inode, struct file* file) {
    dev_t info = inode->i_rdev;
    int minor = MINOR(info);
    if (minor >= MAX_INSTANCES) return -1;

    session_data* d = (session_data*)kmalloc(sizeof(session_data), __GFP_WAIT);
    if (!d) {
        printk("%s: allocation of session_data buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }
    
    d->minor = minor;
    d->blocking_read = DEFAULT_BLOCKING_READ;

    file->private_data = d;

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d opened by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return SUCCESS;
}

static int mail_spot_release(struct inode* inode, struct file* file) {
    dev_t info = inode->i_rdev;
    int minor = MINOR(info);

    kfree(file->private_data);

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d released by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return SUCCESS;
}

static ssize_t mail_spot_write(struct file* filp, const char* buff, size_t len, loff_t* off) {

    int minor = ((session_data*)filp->private_data)->minor;
    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d wrote by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    if (len > MAX_MEX_LEN) len = MAX_MEX_LEN;
    mex_node* n = (mex_node*)kmalloc(sizeof(mex_node), __GFP_WAIT);
    if (!n) {
        printk("%s: allocation of node buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }
    char* mex = (char*)kmalloc(len+1, __GFP_WAIT);
    if (!mex) {
        printk("%s: allocation of mex buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -2;
    }
    int ret = copy_from_user(mex, buff, len);
    if (ret != 0) {
        printk("%s: copy_from_user failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -3;
    }
    mex[len] = '\0';

    #ifdef DEBUG
    printk("%s: the string to write is: %s\n",MODNAME, mex);
    #endif

    ret = down_interruptible(&sems[minor]);
    if (ret != 0) {
        printk("%s: WRITE: down_interruptible on sem, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -4;
    }
    n->mex = mex;
    n->len = len;
    n->next = messages[minor].tail;
    n->prev = messages[minor].tail->prev;
    messages[minor].tail->prev->next = n;
    messages[minor].tail->prev = n;
    messages[minor].length++;
    up(&sems[minor]);

    up(&sems_blocking_read[minor]);
    return len;
}

static ssize_t mail_spot_read(struct file* filp, char* buff, size_t len, loff_t* off) {

    int minor = ((session_data*)filp->private_data)->minor;
    int blocking_read = ((session_data*)filp->private_data)->blocking_read;
    int ret;
    mex_node* n;

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d readed by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    ret = down_interruptible(&sems[minor]);
    if (ret != 0) {
        printk("%s: READ: down_interruptible on sems, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -2;
    }

    if (messages[minor].head->next->mex == NULL) {
        #ifdef DEBUG
        printk("%s: no messages in queue, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        #endif
        if (!blocking_read) {
            up(&sems[minor]);
            return 0;
        }
        #ifdef DEBUG
        printk("%s: probably we will go to sleep, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        #endif
    } else {
        if (!blocking_read) {
            ret = down_interruptible(&sems_blocking_read[minor]);
            if (ret != 0) {
                printk("%s: READ: down_interruptible on sem_blocking_read, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
                return -3;
            }
            goto actual_write;
        }
    }
    up(&sems[minor]);
    ret = down_interruptible(&sems_blocking_read[minor]);
    if (ret != 0) {
        printk("%s: READ: down_interruptible on sem_blocking_read, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -3;
    }
    ret = down_interruptible(&sems[minor]);
    if (ret != 0) {
        printk("%s: READ: down_interruptible on sems, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -2;
    }

actual_write:

    n = messages[minor].head->next;

    n->prev->next = n->next;
    n->next->prev = n->prev;
    messages[minor].length--;
    up(&sems[minor]);

    int l = len<n->len?len:n->len;
    ret = copy_to_user(buff, n->mex, l);
    if (ret != 0) {
        printk("%s: copy_to_user failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }

    kfree(n->mex);
    kfree(n);

    #ifdef DEBUG
    printk("%s: succesfully readed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return l;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int mail_spot_ioctl(struct inode* inode, struct file* file, unsigned int cmd, unsigned long arg)
#else
static long mail_spot_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#endif
    {
        #ifdef DEBUG
        int minor = ((session_data*)file->private_data)->minor;
        #endif
        session_data* d = file->private_data;
        #ifdef DEBUG
        printk("%s: ioctl called, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        #endif

        switch(cmd) {
            case BLOCKING_READ:
                d->blocking_read = 1;

                #ifdef DEBUG
                printk("%s: ioctl blocking_read setted, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
                #endif

                break;

            case NON_BLOCKING_READ:
                d->blocking_read = 0;

                #ifdef DEBUG
                printk("%s: ioctl non_blocking_read setted, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
                #endif

                break;
        }

        return SUCCESS;
    }

static struct file_operations fops = {
    .write = mail_spot_write,
    .read = mail_spot_read,
    .open =  mail_spot_open,
    .release = mail_spot_release,
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = mail_spot_ioctl
    #else
    .unlocked_ioctl = mail_spot_ioctl
    #endif
};

int init_module(void) {
    Major = register_chrdev(0, DEVICE_NAME, &fops);
    if (Major < 0) {
        printk("%s: device registration failed\n", MODNAME);
        return Major;
    }

    int i;
    for (i=0; i<MAX_INSTANCES; i++) {
        head[i].mex = NULL; head[i].len = -1; head[i].next = NULL; head[i].prev = NULL;
        tail[i].mex = NULL; tail[i].len = -1; tail[i].next = NULL; tail[i].prev = NULL;

        messages[i].head = &head[i];
        messages[i].tail = &tail[i];
        head[i].next = &tail[i];
        tail[i].prev = &head[i];
        messages[i].length = 0;     

        sema_init(&sems[i], 1);
        sema_init(&sems_blocking_read[i], 0);
    }

    printk(KERN_INFO "%s: device registered, it is assigned major number %d\n", MODNAME, Major);
    return 0;
}

void cleanup_module(void) {

    // DA RIVEDERE
    
    int i;
    for (i=0; i<MAX_INSTANCES; i++) {
        if (messages[i].head->next->mex == NULL) continue;
        printk(KERN_INFO "%s: cleaning list %d\n", MODNAME, i);
        mex_node* tmp = messages[i].head->next;
        while (tmp->next->mex != NULL) {
            tmp = tmp->next;
            // printk(KERN_INFO "%s: cleaning mex %s\n", MODNAME, tmp->prev->mex);
            kfree(tmp->prev->mex);
            kfree(tmp->prev);
        }
        // printk(KERN_INFO "%s: cleaning mex %s\n", MODNAME, tmp->mex);
        kfree(tmp->mex);
        kfree(tmp);
    }

    unregister_chrdev(Major, DEVICE_NAME);
    printk(KERN_INFO "%s: device unregistered, it was assigned major number %d\n", MODNAME, Major);
}
