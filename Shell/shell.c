#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int splitPipes(char* input, char *pros[]);
int splitArgs(char* input, char *args[]);
void closeAll(int fd[][2], int len);

int main(void) 
{
    while (true) {
        //get the user's input
        char input[1000] = {0};
        printf("jsh$ ");
        fgets(input, 1000, stdin);
        if(input[0] == '\n') {
            continue;
        }
        //remove /n in the end of input
        char *tmp = NULL;
        if ((tmp = strstr(input, "\n")))
        {
            *tmp = '\0';
        }
        //split input to pros[] by |
        char* pros[100];
        int prolen = splitPipes(input, pros);
        //create pipes
        int fd[prolen - 1][2];
        for (int i = 0; i < prolen; i++) {
            if (pipe(fd[i]) == -1) {
                printf("error in pipe%d\n", i);
                return 1;
            }
        }        
        //record PID and command
        int child[prolen];
        char *command[prolen];
        //run programs in order with pipe()
        for (int i = 0; i < prolen; i++) {
            //split pros to args[] by space
            char* args[100];
            int len = splitArgs(pros[i], args);        
            //command exit
            if (len == 1 && strcmp(args[0], "exit") == 0) {
                    return 0;
            }
            //fork new process
            int rc = fork();
            if (rc < 0) {
                fprintf(stderr, "fork failed\n");
                exit(1);
            } else if (rc == 0) {
                //child process: every process should redirect the STDOUT except the last one
                if (i < prolen - 1) {
                    dup2(fd[i][1], STDOUT_FILENO);
                }
                //every process should redirect the STDIN except the first one
                if (i > 0) {
                    dup2(fd[i - 1][0], STDIN_FILENO);
                }
                closeAll(fd, prolen - 1);
                if (execvp(args[0], args) == -1) {
                    exit(127);
                }
            } else {
                //parent process
                child[i] = rc;
                command[i] = args[0];
            }
        }
        closeAll(fd, prolen - 1);
        //parent process wait for all the children
        int finalreturn;
        for (int i = 0; i < prolen; i++) {
            int wstatus;
            waitpid(child[i], &wstatus, 0);
            int return_value = WEXITSTATUS(wstatus);
            if (return_value == 127) {
                printf("jsh error: Command not found: %s\n", command[i]);
            }
            if (i == prolen - 1) {
                finalreturn = return_value;
            }
        }       
        printf("jsh status: %d\n", finalreturn);         
    }   
    return 0;
}

//spilt input to pros[] by |
int splitPipes(char* input, char *pros[]) 
{
    char *arg;
    int len;
    for(len = 0; (arg = strsep(&input,"|")) != NULL; len++) {
        pros[len] = strdup(arg);
    }
    pros[len] = NULL;
    return len;
}

//spilt input to args[] by space
int splitArgs(char* input, char *args[]) 
{
    char *arg;
    int len;
    for(len = 0; (arg = strsep(&input," \t")) != NULL; len++) {
        if(*arg == '\0') {
            len--;
            continue;
        }
        args[len] = strdup(arg);
    }
    args[len] = NULL;
    return len;
}

//close all the file descriptor created by pipe
void closeAll(int fd[][2], int len)
{
    for (int i = 0; i < len; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}