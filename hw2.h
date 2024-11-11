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
#define MAX_TREE_DEPTH 7  // Root: depth 0

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

typedef int response_t;
#define RESPONSE_HANDLE_OK 1
#define RESPONSE_RELAY_OK 2
// Nothing has been printed.
#define RESPONSE_RELAY_OK_NO_PRINT 3
#define RESPONSE_SEARCH_FOUND 4
#define RESPONSE_SEARCH_NOT_FOUND 5
#define RESPONSE_EMPTY -1
// Continue DFS.
#define RESPONSE_NOT_FOUND -2

typedef int adopt_op_t;
// cmd == "adopt"
#define ADOPT_OPEN 0
// cmd == "Adopt"
#define ADOPT_READ 1

#define COMPARE_MOD 0
#define COMPARE_GRADUATE 1

#endif  // HW2_H_
