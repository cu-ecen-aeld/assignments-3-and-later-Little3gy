#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>     // For system(), exit(), EXIT_SUCCESS, EXIT_FAILURE
#include <unistd.h>     // For fork(), execv(), getpid(), dup2(), close(), STDOUT_FILENO
#include <sys/wait.h>   // For waitpid(), WIFEXITED(), WEXITSTATUS()
#include <sys/types.h>  // For pid_t
#include <fcntl.h>      // For open(), O_WRONLY, O_CREAT, O_TRUNC
#include <stdio.h>      // For printf(), perror()

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
