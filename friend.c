#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hw2.h"

static const char kRootName[MAX_FRIEND_INFO_LEN] = "Not_Tako";  // root of tree
static char current_info[MAX_FRIEND_INFO_LEN];                  // current process info
static char current_name[MAX_FRIEND_NAME_LEN];                  // current process name
static int current_value = 100;                                 // current process value
static bool is_root = false;
Friend children[MAX_CHILDREN];
int children_size = 0;
FILE *parent_write_stream = NULL;

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
    // LOG("SetFriend; pid=%d, read_fd=%d, write_fd=%d, info=%s", pid, read_fd, write_fd, info);

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
void print_direct_meet(const char *const friend_name) {
    fprintf(stdout, "Not_Tako has met %s by himself\n", friend_name);
}

void print_indirect_meet(const char *const parent_friend_name, const char *const child_friend_name) {
    fprintf(stdout, "Not_Tako has met %s through %s\n", child_friend_name, parent_friend_name);
}

void print_fail_meet(const char *const parent_friend_name, const char *const child_friend_name) {
    fprintf(stdout, "Not_Tako does not know %s to meet %s\n", parent_friend_name, child_friend_name);
}

void print_fail_check(const char *const parent_friend_name) {
    fprintf(stdout, "Not_Tako has checked, he doesn't know %s\n", parent_friend_name);
}

void print_success_adopt(const char *const parent_friend_name, const char *const child_friend_name) {
    fprintf(stdout, "%s has adopted %s\n", parent_friend_name, child_friend_name);
}

void print_fail_adopt(const char *const parent_friend_name, const char *const child_friend_name) {
    fprintf(stdout, "%s is a descendant of %s\n", parent_friend_name, child_friend_name);
}

void print_compare_gtr(const char *const friend_name) {
    fprintf(stdout, "Not_Tako is still friends with %s\n", friend_name);
}

void print_compare_leq(const char *const friend_name) {
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
// fd 3: /dev/null
// fd 4-: Children I/O
//
// For child:
// fd 0: Input from parent
// fd 1: Output to outside
// fd 2: Output to outside (debug messages)
// fd 3: Output to parent
// fd 4-: Children I/O

// Send message to `friend` and wait for response. Returns the response.
response_t Send(const Friend *const friend, const char *fmt, ...) {
    if (strchr(fmt, '\n') == NULL) {
        LOG("WARNING: format string %s does not contain newline", fmt);
    }

    va_list args;
    va_start(args, fmt);
    if (vfprintf(friend->write_stream, fmt, args) < 0) {
        ERR_EXIT("vfprintf");
    }
    va_end(args);

    response_t res = RESPONSE_EMPTY;
    if (fscanf(friend->read_stream, "%d", &res) == EOF) {
        LOG("WARNING: Friend died before getting response");
    }
#ifdef DEBUG
    char cmd[16];
    sscanf(fmt, "%s", cmd);
    LOG("%s got response from %s(%d): %d (cmd=%s)", current_info, friend->info, friend->pid, res, cmd);
#endif
    return res;
}

// Handle: ourself is the target handler for the command
// Relay: relay the command (unchanged) to children, using DFS to search for the target
//        and IPC to communicate DFS status

response_t HandleMeet(const char *const parent_friend_name, const char *const child_friend_info, int print) {
    char child_friend_name[MAX_FRIEND_NAME_LEN];
    SplitInfo(child_friend_info, child_friend_name, NULL);

    int pipefds_to_child[2], pipefds_from_child[2];
    pipe2(pipefds_to_child, O_CLOEXEC);
    pipe2(pipefds_from_child, O_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        ERR_EXIT("fork");
    } else if (pid == 0) {  // Child
        dup2(pipefds_to_child[0], PARENT_READ_FD);
        dup2(pipefds_from_child[1], PARENT_WRITE_FD);
        for (int i = 0; i < 2; ++i) {
            close(pipefds_to_child[i]);
            close(pipefds_from_child[i]);
        }
        execl("./friend", "./friend", child_friend_info, NULL);
    }

    // Parent
    close(pipefds_to_child[0]);
    close(pipefds_from_child[1]);

    Friend *child = &children[children_size++];
    SetFriend(child, pid, pipefds_from_child[0], pipefds_to_child[1], child_friend_info);
    if (print) {
        if (is_root) {
            print_direct_meet(child->name);
        } else {
            print_indirect_meet(parent_friend_name, child_friend_name);
        }
    }
    return RESPONSE_HANDLE_OK;
}

response_t RelayMeet(const char *const parent_friend_name, const char *const child_friend_info, int print) {
    char child_friend_name[MAX_FRIEND_NAME_LEN];
    SplitInfo(child_friend_info, child_friend_name, NULL);

    // DFS for target parent.
    for (int i = 0; i < children_size; ++i) {
        int res = Send(&children[i], "%ceet %s %s\n", "mM"[print], parent_friend_name, child_friend_info);
        if (res > 0) {
            return RESPONSE_RELAY_OK;
        }
    }

    if (is_root) {
        print_fail_meet(parent_friend_name, child_friend_name);
    }
    return RESPONSE_NOT_FOUND;
}

response_t HandleLevelPrint(int has_printed) {
    if (has_printed) {
        printf(" ");
    }
    printf("%s", current_info);
    return RESPONSE_HANDLE_OK;
}

// Tell nodes that are `level` levels deeper than current node to print their info.
// `has_printed` is a flag denoting whether anything about the target level has been printed.
response_t RelayLevelPrint(int level, int has_printed) {
    LOG("%s: childern_size=%d", current_name, children_size);
    for (int i = 0; i < children_size; ++i) {
        int res = Send(&children[i], "LevelPrint %d %d\n", level - 1, has_printed);
        if (res > 0 && res != RESPONSE_RELAY_OK_NO_PRINT) {
            has_printed = 1;
        }
    }
    if (has_printed) {
        return RESPONSE_RELAY_OK;
    }
    return RESPONSE_RELAY_OK_NO_PRINT;
}

response_t HandleCheck(const char *const parent_friend_name) {
    printf("%s\n", current_info);  // Equivalent to `HandleLevelPrint(0); printf("\n");`.

    for (int level = 1; level < MAX_TREE_DEPTH; ++level) {  // IDDFS.
#ifdef DEBUG
        // printf("(Level %d start)\n", level);
#endif
        response_t res = RelayLevelPrint(level, 0);
        if (res > 0 && res != RESPONSE_RELAY_OK_NO_PRINT) {
#ifdef DEBUG
            // printf("(Level %d end)", level);
#endif
            printf("\n");
        }
    }
    return RESPONSE_HANDLE_OK;
}

response_t RelayCheck(const char *const parent_friend_name) {
    // DFS for target parent.
    for (int i = 0; i < children_size; ++i) {
        int res = Send(&children[i], "Check %s\n", parent_friend_name);
        if (res > 0) {
            return RESPONSE_HANDLE_OK;
        }
    }

    if (is_root) {
        print_fail_check(parent_friend_name);
    }
    return RESPONSE_NOT_FOUND;
}

response_t HandleGraduate(const char *const friend_name) {
    for (int i = 0; i < children_size; ++i) {
        // Use `waitpid()` instead of IPC as response.
        fprintf(children[i].write_stream, "Graduate %s\n", children[i].name);
        int status;
        LOG("Waiting for %d to die", children[i].pid);
        waitpid(children[i].pid, &status, 0);
        LOG("%d died with status %d", children[i].pid, WEXITSTATUS(status));
    }
    if (is_root) {
        print_final_graduate();
    }
    _exit(0);
}

response_t RelayGraduate(const char *const friend_name) {
    for (int i = 0; i < children_size; ++i) {
        if (strncmp(children[i].name, friend_name, MAX_FRIEND_NAME_LEN) == 0) {
            fprintf(children[i].write_stream, "Graduate %s\n", friend_name);
            int status;
            LOG("Waiting for %d to die", children[i].pid);
            waitpid(children[i].pid, &status, 0);
            LOG("%d died with status %d", children[i].pid, status);

            // Remove this entry in the children table.
            for (int j = i + 1; j < children_size; ++j) {
                children[j - 1] = children[j];
            }
            InitializeFriend(&children[children_size--]);

            return RESPONSE_RELAY_OK;
        }

        int res = Send(&children[i], "Graduate %s\n", friend_name);
        if (res > 0) {
            return RESPONSE_RELAY_OK;
        }
    }
    return RESPONSE_NOT_FOUND;
}

response_t HandleSearch(const char *const child_friend_name) {
    if (strncmp(child_friend_name, current_name, MAX_FRIEND_NAME_LEN) == 0) {
        return RESPONSE_SEARCH_FOUND;
    }
    for (int i = 0; i < children_size; ++i) {
        int res = Send(&children[i], "S %s %s\n", children[i].name, child_friend_name);
        if (res == RESPONSE_SEARCH_FOUND) {
            return RESPONSE_SEARCH_FOUND;
        }
    }
    return RESPONSE_SEARCH_NOT_FOUND;
}

response_t RelaySearch(const char *const parent_friend_name, const char *const child_friend_name) {
    for (int i = 0; i < children_size; ++i) {
        response_t res = Send(&children[i], "S %s %s\n", parent_friend_name, child_friend_name);
        if (res > 0) {  // Target parent found.
            return res;
        }
    }
    return RESPONSE_NOT_FOUND;
}

response_t HandleAdoptPrint(const char *const child_friend_name) {
    LOG("AdoptPrint(%s)", child_friend_name);
    FILE *fifo_stream = fopen("Adopt.fifo", "w");
    if (fifo_stream == NULL) {
        perror("FIFO write end fopen");
    }

    for (int i = 0; i < children_size; ++i) {
        fprintf(fifo_stream, "%s %s\n", current_name, children[i].info);
        LOG("Written to Adopt.fifo: %s %s", current_name, children[i].info);
        response_t res = Send(&children[i], "P %s\n", children[i].name);
        CHECK(res == RESPONSE_HANDLE_OK);
    }
    if (fclose(fifo_stream) == EOF) {
        ERR_EXIT("fclose");
    }
    return RESPONSE_HANDLE_OK;
}

response_t RelayAdoptPrint(const char *const child_friend_name) {
    for (int i = 0; i < children_size; ++i) {
        // First time entering handle mode.
        if (strncmp(child_friend_name, children[i].name, MAX_FRIEND_NAME_LEN) == 0) {
            FILE *fifo_stream = fopen("Adopt.fifo", "w");
            if (fifo_stream == NULL) {
                perror("fopen");
            }
            setvbuf(fifo_stream, NULL, _IONBF, 0);
            fprintf(fifo_stream, "%d\n", children[i].value);
            fclose(fifo_stream);
        }

        response_t res = Send(&children[i], "P %s\n", child_friend_name);
        if (res > 0) {
            return RESPONSE_RELAY_OK;
        }
    }
    return RESPONSE_NOT_FOUND;
}

// Adopt the subtree according to information in FIFO.
response_t HandleAdopt() {
    // Only Adopt has to respond to parent early in handler and not in `main()`. This is due to blocking I/O
    // constraints about FIFOs.
    fprintf(parent_write_stream, "%d\n", RESPONSE_HANDLE_OK);

    FILE *fifo_stream = fopen("Adopt.fifo", "r");
    if (fifo_stream == NULL) {
        ERR_EXIT("FIFO read end fopen");
    }
    setvbuf(fifo_stream, NULL, _IONBF, 0);

    char meet_parent_name[MAX_FRIEND_NAME_LEN], meet_child_info[MAX_FRIEND_INFO_LEN];
    LOG("Ready to read from FIFO");
    while (fscanf(fifo_stream, "%s %s", meet_parent_name, meet_child_info) != EOF) {
        LOG("Read from Adopt.fifo: %s %s", meet_parent_name, meet_child_info);
        char meet_child_name[MAX_FRIEND_NAME_LEN];
        int meet_child_value;
        SplitInfo(meet_child_info, meet_child_name, &meet_child_value);
        meet_child_value %= current_value;
        snprintf(meet_child_info, MAX_FRIEND_INFO_LEN, "%s_%02d", meet_child_name, meet_child_value);

        response_t res;
        if (strncmp(current_name, meet_parent_name, MAX_FRIEND_NAME_LEN) == 0) {
            res = HandleMeet(meet_parent_name, meet_child_info, 0);
        } else {
            res = RelayMeet(meet_parent_name, meet_child_info, 0);
        }
        CHECK(res > 0);
    }

    if (fclose(fifo_stream) == EOF) {
        ERR_EXIT("fclose");
    }
    return RESPONSE_HANDLE_OK;
}

response_t RelayAdopt(const char *const parent_friend_name) {
    for (int i = 0; i < children_size; ++i) {
        response_t res = Send(&children[i], "Adopt %s UNUSED\n", parent_friend_name);
        if (res > 0) {
            return RESPONSE_RELAY_OK;
        }
    }
    return RESPONSE_NOT_FOUND;
}

response_t RootHandleAdopt(const char *const parent_friend_name, const char *const child_friend_name) {
    // Determine if child is a descendant of parent.
    response_t search_result;
    if (strncmp(current_name, child_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
        search_result = HandleSearch(parent_friend_name);
    } else {
        search_result = RelaySearch(child_friend_name, parent_friend_name);
    }
    assert(search_result == RESPONSE_SEARCH_FOUND || search_result == RESPONSE_SEARCH_NOT_FOUND);
    if (search_result == RESPONSE_SEARCH_FOUND) {
        print_fail_adopt(parent_friend_name, child_friend_name);
        return RESPONSE_EMPTY;
    }

    if (mkfifo("Adopt.fifo", S_IRWXU) < 0) {
        ERR_EXIT("mkfifo");
    }
    LOG("mkfifo() done");

    // Because opening a FIFO in nonblocking mode requires the read end to be opened first, we must first find the
    // target parent that will adopt nodes, as it opens the read end of FIFO.
    response_t res;
    if (strncmp(current_name, parent_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
        res = HandleAdopt();
    } else {
        res = RelayAdopt(parent_friend_name);
    }

    // Then we can open the FIFO for writes.
    FILE *fifo_stream = fopen("Adopt.fifo", "w");
    if (fifo_stream == NULL) {
        perror("fopen");
    }
    setvbuf(fifo_stream, NULL, _IONBF, 0);
    fprintf(fifo_stream, "%s %s_", parent_friend_name, child_friend_name);

    // Root will never be adopted, so it's always relay.
    res = RelayAdoptPrint(child_friend_name);
    CHECK(res > 0);
    res = RelayGraduate(child_friend_name);

    // Close this after `RelayAdoptPrint()` to ensure that at least one write end exists.
    // After this last write end closes, the read end at the target parent should close automatically.
    fclose(fifo_stream);
    if (unlink("Adopt.fifo") < 0) {
        ERR_EXIT("unlink");
    }
    print_success_adopt(parent_friend_name, child_friend_name);
    return res;
}

int main(int argc, char *argv[]) {
    // Hi! Welcome to SP Homework 2, I hope you have fun
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
    }
    parent_write_stream = fdopen(PARENT_WRITE_FD, "w");
    setvbuf(parent_write_stream, NULL, _IONBF, 0);
    LOG("Init done; current_name=%s, current_value=%d", current_name, current_value);

    char cmd[MAX_CMD_LEN];
    while (scanf("%s", cmd) != EOF) {
        LOG("%s got command: %s", current_info, cmd);
        response_t res = RESPONSE_EMPTY;
        int respond = 1;
        switch (toupper(cmd[0])) {
            case 'M': {
                char parent_friend_name[MAX_FRIEND_NAME_LEN], child_friend_info[MAX_FRIEND_INFO_LEN];
                scanf("%s %s", parent_friend_name, child_friend_info);
                if (strncmp(current_name, parent_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    res = HandleMeet(parent_friend_name, child_friend_info, isupper(cmd[0]) != 0);
                } else {
                    res = RelayMeet(parent_friend_name, child_friend_info, isupper(cmd[0]) != 0);
                }
                break;
            }
            case 'C': {
                char parent_friend_name[MAX_FRIEND_NAME_LEN];
                scanf("%s", parent_friend_name);
                if (strncmp(current_name, parent_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    res = HandleCheck(parent_friend_name);
                } else {
                    res = RelayCheck(parent_friend_name);
                }
                break;
            }
            case 'G': {
                char friend_name[MAX_FRIEND_NAME_LEN];
                scanf("%s", friend_name);
                if (is_root) {
                    if (strncmp(current_name, friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                        HandleCheck(friend_name);
                    } else if (RelayCheck(friend_name) < 0) {
                        break;
                    }
                }
                if (strncmp(current_name, friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    res = HandleGraduate(friend_name);
                } else {
                    res = RelayGraduate(friend_name);
                }
                break;
            }
            case 'A': {
                // respond_to_parent = false;
                char parent_friend_name[MAX_FRIEND_NAME_LEN], child_friend_name[MAX_FRIEND_NAME_LEN];
                scanf("%s %s", parent_friend_name, child_friend_name);
                if (is_root) {
                    res = RootHandleAdopt(parent_friend_name, child_friend_name);
                } else if (strncmp(current_name, parent_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    respond = 0;  // Don't respond here because we responded manually in `HandleAdopt()`.
                    res = HandleAdopt();
                } else {
                    res = RelayAdopt(parent_friend_name);
                }
                break;
            }

            // Custom commands.
            // LevelPrint
            case 'L': {
                int level, has_printed;
                scanf("%d %d", &level, &has_printed);
                if (level == 0) {
                    res = HandleLevelPrint(has_printed);
                } else {
                    res = RelayLevelPrint(level, has_printed);
                }
                break;
            }
            // Search: Search subtree of `parent` for `child`. Essentially "Check" without printing.
            case 'S': {
                char parent_friend_name[MAX_FRIEND_NAME_LEN], child_friend_name[MAX_FRIEND_NAME_LEN];
                scanf("%s %s", parent_friend_name, child_friend_name);
                if (strncmp(current_name, parent_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    res = HandleSearch(child_friend_name);
                } else {
                    res = RelaySearch(parent_friend_name, child_friend_name);
                }
                break;
            }
            // AdoptPrint: Recursively print all parent-child relationships to file `Adopt.fifo`.
            case 'P': {
                char child_friend_name[MAX_FRIEND_NAME_LEN];
                scanf("%s", child_friend_name);
                if (strncmp(current_name, child_friend_name, MAX_FRIEND_NAME_LEN) == 0) {
                    res = HandleAdoptPrint(child_friend_name);
                } else {
                    res = RelayAdoptPrint(child_friend_name);
                }
                break;
            }
        }
        if (respond) {
            fprintf(parent_write_stream, "%d\n", res);
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

    return 0;
}
