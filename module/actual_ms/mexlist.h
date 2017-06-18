#ifndef MEXLIST_H
#define MEXLIST_H

#include <linux/spinlock.h>
#include <linux/module.h>

#define MAX_UNSIGNED 4294967295

typedef struct mex_node {
    char* mex;
    int len;
    struct mex_node* next;
    struct mex_node* prev;
} mex_node;

typedef struct mex_list {
    struct mex_node* head;
    struct mex_node* tail;
    int length;
    unsigned int max_mex_len;
    atomic_t count;
    spinlock_t lock;
} mex_list;

#endif