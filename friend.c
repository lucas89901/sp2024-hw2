#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hw2.h"

// somethings I recommend leaving here, but you may delete as you please
static const char kRootName[MAX_FRIEND_INFO_LEN] = "Not_Tako";  // root of tree
static char current_info[MAX_FRIEND_INFO_LEN];                  // current process info
static char current_name[MAX_FRIEND_NAME_LEN];                  // current process name
static int current_value = 100;                                 // current process value
static bool is_root = false;
Friend children[MAX_CHILDREN];
int children_size = 0;
FILE *parent_write_stream = NULL;
pid_t pid;
// char argv0_realpath[256];

void SplitInfo(const char *const info, char *const name, int *const value) {
    char *sep = strchr(info, '_');
    CHECK(sep != NULL);
    *sep = '\0';
    if (name != NULL) {
        strncpy(name, info, MAX_FRIEND_NAME_LEN);
    }
    if (value != NULL) {
        sscanf(sep + 1, "%d", value);
    }
    *sep = '_';
}

void InitializeFriend(Friend *const friend) {
    friend->pid = -1;
    friend->read_fd = -1;
    friend->read_stream = NULL;
    friend->write_fd = -1;
    friend->write_stream = NULL;
    memset(friend->info, MAX_FRIEND_INFO_LEN, 0);
    memset(friend->name, MAX_FRIEND_NAME_LEN, 0);
    friend->value = -1;
}

void SetFriend(Friend *const friend, pid_t pid, int read_fd, int write_fd, const char *const info) {
    LOG("SetFriend; pid=%d, read_fd=%d, write_fd=%d, info=%s", pid, read_fd, write_fd, info);

    friend->pid = pid;

    friend->read_fd = read_fd;
    friend->read_stream = fdopen(read_fd, "r");
    if (!friend->read_stream) {
        ERR_EXIT("fdopen");
    }
    setvbuf(friend->read_stream, NULL, _IONBF, 0);

    friend->write_fd = write_fd;
    friend->write_stream = fdopen(write_fd, "w");
    if (!friend->write_stream) {
        ERR_EXIT("fdopen");
    }
    setvbuf(friend->write_stream, NULL, _IONBF, 0);

    strncpy(friend->info, info, MAX_FRIEND_INFO_LEN);
    SplitInfo(friend->info, friend->name, &friend->value);
}

// a bunch of prints for you
void print_direct_meet(char *friend_name) {
    fprintf(stdout, "Not_Tako has met %s by himself\n", friend_name);
}

void print_indirect_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako has met %s through %s\n", child_friend_name, parent_friend_name);
}

void print_fail_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako does not know %s to meet %s\n", parent_friend_name, child_friend_name);
}

void print_fail_check(char *parent_friend_name) {
    fprintf(stdout, "Not_Tako has checked, he doesn't know %s\n", parent_friend_name);
}

void print_success_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s has adopted %s\n", parent_friend_name, child_friend_name);
}

void print_fail_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s is a descendant of %s\n", parent_friend_name, child_friend_name);
}

void print_compare_gtr(char *friend_name) {
    fprintf(stdout, "Not_Tako is still friends with %s\n", friend_name);
}

void print_compare_leq(char *friend_name) {
    fprintf(stdout, "%s is dead to Not_Tako\n", friend_name);
}

void print_final_graduate() {
    fprintf(stdout, "Congratulations! You've finished Not_Tako's annoying tasks!\n");
}

/* terminate child pseudo code
void clean_child(){
    close(child read_fd);
    close(child write_fd);
    call wait() or waitpid() to reap child; // this is blocking
}

*/

/* remember read and write may not be fully transmitted in HW1?
void fully_write(int write_fd, void *write_buf, int write_len);

void fully_read(int read_fd, void *read_buf, int read_len);

please do above 2 functions to save some time
*/

// For root:
// fd 0: Input from outside
// fd 1: Output to outside
// fd 2: Output to outside (debug messages)
// fd 3-: Children I/O
//
// For child:
// fd 0: Input from parent
// fd 1: Output to parent
// fd 2: Output to outside (debug messages)
// fd 3-: Children I/O

void HandleMeet() {
    char parent_name[MAX_FRIEND_NAME_LEN], new_child_info[MAX_FRIEND_INFO_LEN];
    scanf("%s %s", parent_name, new_child_info);
    LOG("Meet %s %s", parent_name, new_child_info);
    char new_child_name[MAX_FRIEND_NAME_LEN];
    SplitInfo(new_child_info, new_child_name, NULL);

    if (strncmp(parent_name, current_name, MAX_FRIEND_NAME_LEN) == 0) {
        int pipefds_to_child[2], pipefds_from_child[2];
        pipe2(pipefds_to_child, O_CLOEXEC);
        pipe2(pipefds_from_child, O_CLOEXEC);
        pid_t pid = fork();
        if (pid < 0) {
            ERR_EXIT("fork");
        } else if (pid == 0) {
            dup2(pipefds_to_child[0], PARENT_READ_FD);
            dup2(pipefds_from_child[1], PARENT_WRITE_FD);
            for (int i = 0; i < 2; ++i) {
                close(pipefds_to_child[i]);
                close(pipefds_from_child[i]);
            }
            execl("./friend", "./friend", new_child_info, NULL);
        }

        // Parent
        close(pipefds_to_child[0]);
        close(pipefds_from_child[1]);
        Friend *child = &children[children_size++];
        SetFriend(child, pid, pipefds_from_child[0], pipefds_to_child[1], new_child_info);
        if (is_root) {
            print_direct_meet(child->name);
        } else {
            fprintf(parent_write_stream, "%d\n", RESPONSE_OK);
        }
        return;
    }

    // Recursively search in children.
    for (int i = 0; i < children_size; ++i) {
        Friend *child = &children[i];
        LOG("Recursing into children %d (pid=%d, info=%s)", i, child->pid, child->info);
        fprintf(children[i].write_stream, "Meet %s %s\n", parent_name, new_child_info);
        int res = -1;
        fscanf(children[i].read_stream, "%d", &res);
        LOG("response=%d", res);
        if (res == RESPONSE_OK) {
            if (is_root) {
                print_indirect_meet(parent_name, new_child_name);
            } else {
                fprintf(parent_write_stream, "%d\n", RESPONSE_OK);
            }
            return;
        }
    }
    if (is_root) {
        print_fail_meet(parent_name, new_child_name);
    } else {
        fprintf(parent_write_stream, "%d\n", RESPONSE_NOT_FOUND);
    }
}

void HandleCheck() {
}

void HandleGraduate() {
}
void HandleAdopt() {
}

int main(int argc, char *argv[]) {
    // Hi! Welcome to SP Homework 2, I hope you have fun
    pid = getpid();  // you might need this when using fork()
    // if (realpath(argv[0], argv0_realpath) < 0) {
    //     ERR_EXIT("realpath");
    // }
    if (argc != 2) {
        fprintf(stderr, "Usage: ./friend [friend_info]\n");
        return 0;
    }
    // prevent buffered I/O, equivalent to fflush() after each stdout, study this as you may need to do it for
    // other friends against their parents
    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        InitializeFriend(&children[i]);
    }

    strncpy(current_info, argv[1], MAX_FRIEND_INFO_LEN);
    if (strcmp(argv[1], kRootName) == 0) {
        strncpy(current_name, current_info, MAX_FRIEND_NAME_LEN);
        current_name[MAX_FRIEND_NAME_LEN - 1] = '\0';
        current_value = 100;
        is_root = true;
        int fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
        CHECK(fd == 3);
    } else {
        SplitInfo(current_info, current_name, &current_value);
        is_root = false;
        parent_write_stream = fdopen(PARENT_WRITE_FD, "w");
        setvbuf(parent_write_stream, NULL, _IONBF, 0);
    }
    LOG("Init done; current_name=%s, current_value=%d", current_name, current_value);

    char cmd[MAX_CMD_LEN];
    while (scanf("%s", cmd) != EOF) {
        LOG("%s got command: %s", current_info, cmd);
        switch (cmd[0]) {
            case 'M':
                HandleMeet();
                break;
            case 'C':
                HandleCheck();
                break;
            case 'G':
                HandleGraduate();
                break;
            case 'A':
                HandleAdopt();
                break;
        }
    }

    // TODO:
    /* you may follow SOP if you wish, but it is not guaranteed to consider every possible outcome

    1. read from parent/stdin
    2. determine what the command is (Meet, Check, Adopt, Graduate, Compare(bonus)), I recommend using strcmp() and/or char check
    3. find out who should execute the command (extract information received)
    4. execute the command or tell the requested friend to execute the command
        4.1 command passing may be required here
    5. after previous command is done, repeat step 1.
    */

    // Hint: do not return before receiving the command "Graduate"
    // please keep in mind that every process runs this exact same program, so think of all the possible cases and implement them

    /* pseudo code
    if(Meet){
        create array[2]
        make pipe
        use fork.
            Hint: remember to fully understand how fork works, what it copies or doesn't
        check if you are parent or child
        as parent or child, think about what you do next.
            Hint: child needs to run this program again
    }
    else if(Check){
        obtain the info of this subtree, what are their info?
        distribute the info into levels 1 to 7 (refer to Additional Specifications: subtree level <= 7)
        use above distribution to print out level by level
            Q: why do above? can you make each process print itself?
            Hint: we can only print line by line, is DFS or BFS better in this case?
    }
    else if(Graduate){
        perform Check
        terminate the entire subtree
            Q1: what resources have to be cleaned up and why?
            Hint: Check pseudo code above
            Q2: what commands needs to be executed? what are their orders to avoid deadlock or infinite blocking?
            A: (tell child to die, reap child, tell parent you're dead, return (die))
    }
    else if(Adopt){
        remember to make fifo
        obtain the info of child node subtree, what are their info?
            Q: look at the info you got, how do you know where they are in the subtree?
            Hint: Think about how to recreate the subtree to design your info format
        A. terminate the entire child node subtree
        B. send the info through FIFO to parent node
            Q: why FIFO? will usin pipe here work? why of why not?
            Hint: Think about time efficiency, and message length
        C. parent node recreate the child node subtree with the obtained info
            Q: which of A, B and C should be done first? does parent child position in the tree matter?
            Hint: when does blocking occur when using FIFO?(mkfifo, open, read, write, unlink)
        please remember to mod the values of the subtree, you may use bruteforce methods to do this part (I did)
        also remember to print the output
    }
    else if(full_cmd[1] == 'o'){
        Bonus has no hints :D
    }
    else{
        there's an error, we only have valid commmands in the test cases
        fprintf(stderr, "%s received error input : %s\n", friend_name, full_cmd); // use this to print out what you received
    }
    */

    // final print, please leave this in, it may bepart of the test case output
    if (is_root) {
        // sleep(10000);
        print_final_graduate();
    }
    return 0;
}
