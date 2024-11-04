#ifndef HW2_H_
#define HW2_H_

#define _GNU_SOURCE
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#define PARENT_READ_FD STDIN_FILENO  // File descriptor for reading from parent
#define PARENT_WRITE_FD 3            // File descriptor for writing to parent

#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 9
#define MAX_FRIEND_INFO_LEN 12
#define MAX_FRIEND_NAME_LEN 9
#define MAX_CMD_LEN 128

#define ERR_EXIT(s) perror(s), exit(errno);

#ifdef DEBUG
#define CHECK(...) assert(__VA_ARGS__)
#define LOG(...)                                                   \
    fprintf(stderr, "[%d:%s(%d)] ", getpid(), __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                  \
    fprintf(stderr, "\n")
#else
#define CHECK(...) __VA_ARGS__
#define LOG(...)
#endif

typedef struct {
    pid_t pid;
    int read_fd;
    FILE* read_stream;
    int write_fd;
    FILE* write_stream;
    char info[MAX_FRIEND_INFO_LEN];
    char name[MAX_FRIEND_NAME_LEN];
    int value;
} Friend;

#define RESPONSE_OK 0
#define RESPONSE_NOT_FOUND 1

#endif  // HW2_H_
