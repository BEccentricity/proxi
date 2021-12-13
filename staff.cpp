#include "Header.hpp"
#pragma once

extern size_t num_child;

long long int get_num(const char* text) {

    char **endptr = (char**) calloc(1, sizeof(char*));
    long long int num = strtoll(text, endptr, 10);

    if (num < 0) {
        fprintf(stderr, "Less 0");
        exit(EXIT_FAILURE);
    }
    if (num == LONG_MAX) {
        fprintf(stderr, "Big number");
        exit(EXIT_FAILURE);
    }
    if (**endptr != '\0') {
        fprintf(stderr, "Wrong format");
        exit(EXIT_FAILURE);
    }
    return num;
}

void child_run(int send_fd, int rcv_fd) {

    //fprintf(stderr, "I download from %d to %d\n", rcv_fd, send_fd);

    while (1) {
        //fprintf(stderr, "Splice\n");
        int len = splice(rcv_fd, NULL,
                                send_fd, NULL,
                                PIPE_BUF, SPLICE_F_MOVE);
        //fprintf(stderr, "From splice\n");
        if (len < 0) {
            perror("splice()");
            fprintf(stderr, "ERROR in fd - %d, %d", rcv_fd, send_fd);
            exit(EXIT_FAILURE);
        }
        if (len == 0) {

            //fprintf(stderr, "Exit Success\n");

            fflush(stdout);
            close(rcv_fd);
            close(send_fd);
            exit(EXIT_SUCCESS);
        }
    }
}

void loader_run(char* path, int send_fd) {
    int fd_file = open(path, O_RDONLY);
    $(fd_file)

    //fprintf(stderr, "I download from fd_file: %d to %d\n", fd_file, send_fd);

    while (1) {

        int len = splice(fd_file, NULL,
                                send_fd, NULL,
                                PIPE_BUF, SPLICE_F_MOVE);
//123456789012345678901234567890123456789012345678901234567890123456789012345678

        //fprintf(stderr, "Loader send %d bytes\n", lenbols);

        if (len < 0) {
            perror("splice");
            fprintf(stderr, "ERROR in fd - fd_file - %d, %d", fd_file, send_fd);
            exit(EXIT_FAILURE);
        }
        if (len == 0) {
            fflush(stdout);
            close(send_fd);
            close(fd_file);

            //fprintf(stderr, "Loader exit\n");

            exit(EXIT_SUCCESS);
        }
    }
}

size_t size_buf(size_t degree) {
    int size = 1;
    for (size_t i = 0; i < degree; ++i) {
        size *= 3;
    }
    return size * 1024;
}

void load_to_buff(struct Connection_t* buff) {
    if (buff->size == buff->capacity) {
        return;
    }
    if (buff->size == 0) {
        buff->offset_begin = buff->buff;
        buff->offset_end = buff->buff;
        int len = read(buff->rcv_fd, buff->buff, buff->capacity);
        if (len < 0) {
            perror("read ERROR");
            exit(EXIT_FAILURE);
        }
        buff->size += len;
        buff->offset_end += len;
        return;
    }

    while (buff->size != buff->capacity) {
        if (buff->offset_end == buff->buff + buff->capacity) {
            buff->offset_end = buff->buff;
        }
        if (buff->offset_begin == buff->buff + buff->capacity) {
            buff->offset_begin = buff->buff;
        }
        if (buff->offset_end - buff->offset_begin > 0) {
            size_t len_to_end = buff->buff + buff->capacity - buff->offset_end;
            int len = read(buff->rcv_fd, buff->offset_end, len_to_end);
            if (len < 0) {
                perror("read ERROR");
                exit(EXIT_FAILURE);
            }
            buff->size += len;
            buff->offset_end += len;

            if (len < len_to_end) {
                return;
            }

        } else {
            size_t free_data= buff->offset_begin - buff->offset_end;
            assert(free_data== buff->capacity - buff->size);
            int len = read(buff->rcv_fd, buff->offset_end, free_data);
            if (len < 0) {
                perror("read ERROR");
                exit(EXIT_FAILURE);
            }
            buff->size += len;
            assert(buff->size <= buff->capacity);
            buff->offset_end += len;
            if (len <= free_data) {
                return;
            }
        }
        assert(1 && "ooooops");
    }
}

void download_from_buff(struct Connection_t* buff) {
    if (buff->size == 0) {
        buff->offset_begin = buff->buff;
        buff->offset_end = buff->buff;
        return;
    }
    while (buff->size != 0) {
        if (buff->offset_begin == buff->buff + buff->capacity) {
            buff->offset_begin = buff->buff;
        }
        if (buff->offset_end == buff->buff + buff->capacity) {
            buff->offset_end = buff->buff;
        }
        if (buff->offset_end - buff->offset_begin > 0) {
            int len_data = buff->offset_end - buff->offset_begin;
            int len = write(buff->send_fd, buff->offset_begin, len_data);

            //fprintf(stderr, "%d send %d bytes\n", buff->rcv_fd, len);

            if (len < 0) {
                if (errno == EAGAIN) {
                    return;
                }
                perror("write1 ERROR");
                fprintf(stderr, "%zu/%zu fd - %d", buff->size, buff->capacity, buff->send_fd);
                exit(EXIT_FAILURE);
            }
            buff->offset_begin += len;
            buff->size -= len;
            assert(buff->size >= 0);
            if (len <= len_data) {
                return;
            }
        } else {
            int len_to_end = buff->buff + buff->capacity - buff->offset_begin;
            int len = write(buff->send_fd, buff->offset_begin, len_to_end);

            //fprintf(stderr, "%d send %d bytes\n", buff->rcv_fd, len);

            if (len < 0) {
                if (errno == EAGAIN) {
                    return;
                }
                perror("write2 ERROR");
                exit(EXIT_FAILURE);
            }
            buff->offset_begin += len;
            buff->size -= len;
            if (len < len_to_end) {
                return;
            }
        }
    }
}

void child_dead_handler(int signum) {
}

void print_all_buffers(struct Connection_t* buffs, size_t num) {
    fprintf(stderr, "------------------------------------------------\n");
    for (int i = 0; i < num; ++i) {
        fprintf(stderr, "rcv_fd - %d, send_fd - %d, size - %zu, capacity - %zu\n", buffs[i].rcv_fd, buffs[i].send_fd, buffs[i].size, buffs[i].capacity);
        fprintf(stderr, "Buff: [");
        for (int j = 0; j < buffs[i].capacity; ++j) {
            if (buffs[i].buff[j] == 0)
                continue;
            fprintf(stderr, "%c", buffs[i].buff[j]);
        }
        fprintf(stderr, "]\n");
    }
    fprintf(stderr, "------------------------------------------------\n");
}