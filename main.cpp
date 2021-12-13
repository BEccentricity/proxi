#include "Header.hpp"
#pragma once



int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Incorrect input");
        exit(EXIT_FAILURE);
    }
    size_t num_child = get_num(argv[1]);

    if (argc > 3) {
        fprintf(stderr, "Incorrect input");
        exit(EXIT_FAILURE);
    }

    struct Connection_t *connect_arr = (Connection_t*) calloc(num_child, sizeof(struct Connection_t));


    for (size_t i = 0; i < num_child; ++i) {
        //Pipe Open C -> P
        int pipe_cp[2] = {-1, -1};
        $(pipe(pipe_cp))
        connect_arr[i].rcv_fd = pipe_cp[READ];
        connect_arr[i].size = 0;

        //Pipe Open P -> C
        int pipe_pc[2] = {-1, -1};
        if (i > 0) {
            $(pipe(pipe_pc))
            connect_arr[i - 1].send_fd = pipe_pc[WRITE];

        }
        if (i == num_child - 1) {
            connect_arr[i].send_fd = STDOUT_FILENO;

        }
        pid_t ppid = getpid();

        //Create children
        pid_t pid = fork();
        if (pid == 0) {
            $(prctl(PR_SET_PDEATHSIG, SIGKILL))
            if (ppid != getppid()) {
                fprintf(stderr, "Daddy DEAD!\n");
                exit(EXIT_FAILURE);
            }

//            Close signals and free heap
            for (int j = 0; j < i + 1; ++j) {
                assert(connect_arr[j].rcv_fd != -1);
                $(close(connect_arr[j].rcv_fd))
                if ((j != num_child - 1) && (i != 0)) {
                    assert(connect_arr[j].send_fd != -1);
                    $(close(connect_arr[j].send_fd))
                }
            }
            free(connect_arr);

            if (i == 0) {
                loader_run(argv[2], pipe_cp[WRITE]);
            } else {
                child_run(pipe_cp[WRITE], pipe_pc[READ]);
            }
        } else {
            if (i == 0) {
                assert(pipe_cp[WRITE] != -1);
                $(close(pipe_cp[WRITE]))
            } else {
                assert(pipe_pc[READ] != -1);
                $(close(pipe_cp[WRITE]))
                $(close(pipe_pc[READ]))
            }
        }
    }

    for (int i = 0; i < num_child; ++i) {
        //Create buff
        connect_arr[i].capacity = size_buf(num_child - i);
        connect_arr[i].size = 0;
        connect_arr[i].buff = (char*) calloc(connect_arr->capacity, sizeof(char));
        if (connect_arr[i].buff == NULL) {
            perror("calloc");
            exit(EXIT_FAILURE);
        }
        connect_arr[i].offset_begin = connect_arr[i].buff;
        connect_arr[i].offset_end = connect_arr[i].buff;
        $(fcntl(connect_arr[i].rcv_fd, F_SETFL, O_NONBLOCK))
        $(fcntl(connect_arr[i].send_fd, F_SETFL, O_NONBLOCK))
    }

/*
    fprintf(stderr, "ufdsW:[");
    for (int i = 0; i < num_child; ++i) {
        fprintf(stderr, "%d ", ufdsW[i]);
    }
    fprintf(stderr, "]\n");

    fprintf(stderr, "ufdsR:[");
    for (int i = 0; i < num_child; ++i) {
        fprintf(stderr, "%d ", ufdsR[i]);
    }
    fprintf(stderr, "]\n");

    fprintf(stderr, ">>>>>>>>>>>Parrent start work\n");

    print_all_buffers(connect_arr, num_child);
*/

    int num_dead = 0;
    struct Ufds_t ufds = {(pollfd*) calloc(2*num_child, sizeof(struct pollfd)),
                          (size_t*) calloc(2*num_child, sizeof(size_t)),
                           0, 0};
    while(1) {
        //Update ufds
//        if (ufds.nclose_ind == num_child - 1) {
//            return 1;
//        }
        ufds.size = 0;
        for (size_t i = ufds.nclose_ind; i < num_child; ++i) {
            if (connect_arr[i].size == 0) {
                ufds.data[ufds.size].fd = connect_arr[i].rcv_fd;
                ufds.data[ufds.size].events = POLLIN;
                ufds.buffs_ind[ufds.size] = i;
                ufds.size++;
                continue;
            }
            if (connect_arr[i].size == connect_arr[i].capacity || i == ufds.nclose_ind) {
                ufds.data[ufds.size].fd = connect_arr[i].send_fd;
                ufds.data[ufds.size].events = POLLOUT;
                ufds.buffs_ind[ufds.size] = i;
                ufds.size++;
                continue;
            }

            ufds.data[ufds.size].fd = connect_arr[i].rcv_fd;
            ufds.data[ufds.size].events = POLLIN;
            ufds.buffs_ind[ufds.size] = i;
            ufds.size++;

            ufds.data[ufds.size].fd = connect_arr[i].send_fd;
            ufds.data[ufds.size].events = POLLOUT;
            ufds.buffs_ind[ufds.size] = i;
            ufds.size++;
        }

        if (ufds.size == 0) {
            assert(num_dead == num_child);
            break;
        }
        //Poll
        int num_ready_fd = poll(ufds.data, ufds.size, 0);
        if (num_ready_fd < 0) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (num_ready_fd != 0) {
            for (size_t i = 0; i < ufds.size; ++i) {
                if (ufds.data[i].revents & POLLOUT) {
                    download_from_buff(&connect_arr[ufds.buffs_ind[i]]);
                }
                if (ufds.data[i].revents & POLLIN) {
                    load_to_buff(&connect_arr[ufds.buffs_ind[i]]);
                } else {
                    if (ufds.data[i].revents & POLLHUP || ufds.data[i].revents & POLLNVAL) {
                        close(connect_arr[ufds.buffs_ind[i]].rcv_fd);
                        if (connect_arr[i].size == 0) {
                            if (ufds.buffs_ind[i] != num_child - 1) {
                                close(connect_arr[ufds.buffs_ind[i]].send_fd);
                            }
                            ufds.nclose_ind++;
                        }
                    }
                }
                //print_all_buffers(connect_arr, num_child);
            }
        }

        int status = 0;
        int ret = waitpid(-1, &status, WNOHANG);
        if (ret < 0 && errno != ECHILD) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (errno != ECHILD && ret != 0) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "ERROR, Child DEAD");
                exit(EXIT_FAILURE);
            }
            if (WEXITSTATUS(status) == EXIT_SUCCESS) {
                num_dead++;
            }
        }
        fflush(stdout);
    }
    for (size_t i = 0; i < num_child; ++i) {
        free(connect_arr[i].buff);
    }
    free (connect_arr);
    free(ufds.buffs_ind);
    free(ufds.data);
}



