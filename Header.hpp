#pragma once

#define _GNU_SOURCE

#define $(ret)                      \
    if (ret < 0) {                  \
        perror (#ret);              \
        fprintf(stderr, "ERROR");   \
        exit(EXIT_FAILURE);         \
    }                               \

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>

enum {TIMEOUT = 1, READ = 0, WRITE = 1};

struct Connection_t {
    int rcv_fd;
    int send_fd;
    char* buff;

    size_t capacity;
    size_t size;

    char* offset_begin;
    char* offset_end;
};

struct Ufds_t {
    struct pollfd *data;
    size_t* buffs_ind;
    size_t size;
    size_t nclose_ind;
};

long long int get_num(const char* text);
void child_run(int send_fd, int rcv_fd);
void parent_run();
void loader_run(char* path, int send_fd);

size_t size_buf(size_t degree);

void load_to_buff(struct Connection_t* buff);
void download_from_buff(struct Connection_t* buff);

void child_dead_handler(int signum);

void print_all_buffers(struct Connection_t* buffs, size_t num);






