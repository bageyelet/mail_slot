#include <linux/types.h>
#include <linux/sched.h> // serve per current->pid
#include <linux/version.h>  /* For LINUX_VERSION_CODE */
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
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

#define DEFAULT_MAX_MEX_LEN 512
#define MAX_INSTANCES 256

static int default_blocking_read = 1;
module_param(default_blocking_read,int,0666);

static int Major;

mex_list mail_spot_messages[MAX_INSTANCES];
mex_node head[MAX_INSTANCES];
mex_node tail[MAX_INSTANCES];

struct semaphore sems_blocking_read[MAX_INSTANCES];

typedef struct session_data {
    int minor;
    int blocking_read;
} session_data;

int get_users_mail_spot(int minor) {
    if (minor >= MAX_INSTANCES || minor < 0) {
        return -1;
    }
    return atomic_read(&mail_spot_messages[minor].count);
}
EXPORT_SYMBOL(get_users_mail_spot);

int get_max_mex_len_mail_spot(int minor) {
    if (minor >= MAX_INSTANCES || minor < 0) {
        return -1;
    }
    return mail_spot_messages[minor].max_mex_len;
}
EXPORT_SYMBOL(get_max_mex_len_mail_spot);

int get_number_messages_mail_spot(int minor) {
    if (minor >= MAX_INSTANCES || minor < 0) {
        return -1;
    }
    return atomic_read(&mail_spot_messages[minor].length);
}
EXPORT_SYMBOL(get_number_messages_mail_spot);

static int mail_spot_open(struct inode* inode, struct file* file) {

    dev_t info = inode->i_rdev;
    int minor = MINOR(info);
    if (minor >= MAX_INSTANCES) return -2;

    session_data* d = (session_data*)kmalloc(sizeof(session_data), __GFP_WAIT);
    if (!d) {
        printk("%s: allocation of session_data buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }
    
    d->minor = minor;
    d->blocking_read = default_blocking_read;

    file->private_data = d;

    atomic_inc(&(mail_spot_messages[minor].count));

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d opened by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return SUCCESS;
}

static int mail_spot_release(struct inode* inode, struct file* file) {
    dev_t info = inode->i_rdev;
    int minor = MINOR(info);

    kfree(file->private_data);
    atomic_dec(&(mail_spot_messages[minor].count));

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d released by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return SUCCESS;
}

static ssize_t mail_spot_write(struct file* filp, const char* buff, size_t len, loff_t* off) {

    if (buff == NULL) return -5;

    int minor = ((session_data*)filp->private_data)->minor;
    int max_mex_len = mail_spot_messages[minor].max_mex_len;

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d called write by pid=%d\n", MODNAME, minor, current->pid);
    #endif

    if (len > max_mex_len) {
        #ifdef DEBUG
        printk("%s: %d is trying to write too many bytes, minor=%d\n", MODNAME, current->pid, minor);
        #endif
        return -4;
    }
    mex_node* n = (mex_node*)kmalloc(sizeof(mex_node), __GFP_WAIT);
    if (!n) {
        printk("%s: allocation of node buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }
    char* mex = (char*)kmalloc(len+1, __GFP_WAIT);
    if (!mex) {
        kfree(n);
        printk("%s: allocation of mex buffer failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -2;
    }
    int ret = copy_from_user(mex, buff, len);
    if (ret != 0) {
        kfree(n); kfree(mex);
        printk("%s: copy_from_user failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -3;
    }
    mex[len] = '\0';

    #ifdef DEBUG
    printk("%s: the string to write is: %s\n",MODNAME, mex);
    #endif

    spin_lock(&mail_spot_messages[minor].lock);
    n->mex = mex;
    n->len = len;
    n->next = mail_spot_messages[minor].tail;
    n->prev = mail_spot_messages[minor].tail->prev;
    mail_spot_messages[minor].tail->prev->next = n;
    mail_spot_messages[minor].tail->prev = n;
    spin_unlock(&mail_spot_messages[minor].lock);

    up(&sems_blocking_read[minor]);

    atomic_inc(&mail_spot_messages[minor].length);

    #ifdef DEBUG
    printk("%s: succesfully wrote, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return len;
}

static ssize_t mail_spot_read(struct file* filp, char* buff, size_t len, loff_t* off) {

    int minor = ((session_data*)filp->private_data)->minor;
    int blocking_read = ((session_data*)filp->private_data)->blocking_read;

    int ret;
    mex_node* n;

    #ifdef DEBUG
    printk("%s: mail_spot with minor=%d called read by pid=%d\n", MODNAME, minor, current->pid);
    #endif


    if (!blocking_read) {
        ret = down_trylock(&sems_blocking_read[minor]);
        if (ret != 0) {
            return 0;
        }
    } else {
        ret = down_interruptible(&sems_blocking_read[minor]);
        if (ret != 0) {
            printk("%s: READ: down_interruptible on sem_blocking_read, sig arrived, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
            return -3;
        }
    }

    spin_lock(&mail_spot_messages[minor].lock);

    n = mail_spot_messages[minor].head->next;

    if (len < n->len) {
        spin_unlock(&mail_spot_messages[minor].lock);
        up(&sems_blocking_read[minor]);
        #ifdef DEBUG
        printk("%s: READ: %d is trying to read too few bytes, minor=%d\n", MODNAME, current->pid, minor);
        #endif
        return -4;
    }

    n->prev->next = n->next;
    n->next->prev = n->prev;
    spin_unlock(&mail_spot_messages[minor].lock);

    atomic_dec(&mail_spot_messages[minor].length);

    ret = copy_to_user(buff, n->mex, n->len);
    if (ret != 0) {
        printk("%s: copy_to_user failed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
        return -1;
    }

    ret = n->len;
    kfree(n->mex);
    kfree(n);

    #ifdef DEBUG
    printk("%s: succesfully readed, minor=%d, pid=%d\n", MODNAME, minor, current->pid);
    #endif

    return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int mail_spot_ioctl(struct inode* inode, struct file* file, unsigned int cmd, unsigned long arg)
#else
static long mail_spot_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#endif
    {

        int minor = ((session_data*)file->private_data)->minor;
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

            case CHANGE_MAX_MEX_LEN:

                if (arg < 0 || arg > MAX_UNSIGNED) return -1;
                mail_spot_messages[minor].max_mex_len = arg;

                #ifdef DEBUG
                printk("%s: ioctl changed max_mex_len with value=%lu, minor=%d, pid=%d\n", MODNAME, arg, minor, current->pid);
                #endif

                break;
        }

        return SUCCESS;
    }

static struct file_operations fops = {
    .owner = THIS_MODULE,
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

        mail_spot_messages[i].head = &head[i];
        mail_spot_messages[i].tail = &tail[i];
        head[i].next = &tail[i];
        tail[i].prev = &head[i];  
        mail_spot_messages[i].max_mex_len = DEFAULT_MAX_MEX_LEN;
        spin_lock_init(&mail_spot_messages[i].lock);
        atomic_set(&(mail_spot_messages[i].count), 0);
        atomic_set(&(mail_spot_messages[i].length), 0);

        sema_init(&sems_blocking_read[i], 0);
    }

    printk(KERN_INFO "%s: device registered, it is assigned major number %d\n", MODNAME, Major);
    return 0;
}

void cleanup_module(void) {

    int i;    
    for (i=0; i<MAX_INSTANCES; i++) {
        if (mail_spot_messages[i].head->next->mex == NULL) continue;

        #ifdef DEBUG
        printk(KERN_INFO "%s: cleaning list %d\n", MODNAME, i);
        #endif
        
        mex_node* tmp = mail_spot_messages[i].head->next;
        while (tmp->next->mex != NULL) {
            tmp = tmp->next;
            kfree(tmp->prev->mex);
            kfree(tmp->prev);
        }
        kfree(tmp->mex);
        kfree(tmp);
    }

    unregister_chrdev(Major, DEVICE_NAME);
    printk(KERN_INFO "%s: mail_spot succesfully unregistered, it was assigned major number %d\n", MODNAME, Major);
}
