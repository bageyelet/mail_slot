#ifndef MEXLIST_H
#define MEXLIST_H

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
    int max_mex_len;
    atomic_t count;
} mex_list;

#endif