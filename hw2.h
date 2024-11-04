#ifndef HW2_H_
#define HW2_H_

#include <sys/types.h>

#define PARENT_READ_FD 3
#define PARENT_WRITE_FD 4
#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 9
#define MAX_FRIEND_INFO_LEN 12
#define MAX_FRIEND_NAME_LEN 9
#define MAX_CMD_LEN 128

#define ERR_EXIT(s) perror(s), exit(errno);

typedef struct {
    pid_t pid;
    int read_fd;
    int write_fd;
    char info[MAX_FRIEND_INFO_LEN];
    char name[MAX_FRIEND_NAME_LEN];
    int value;
} Friend;

#endif  // HW2_H_
