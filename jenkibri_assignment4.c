#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


#define INPUT_LENGTH 2048
#define MAX_ARGS 512


typedef struct commandLine {
    char* argv[MAX_ARGS + 1];
    int argc;
    char* inputFile;
    char* outputFile;
    bool isBg;
} commandLine;


commandLine *parse_input() {
    char input[INPUT_LENGTH];
    commandLine* currCommand = (commandLine*) calloc(1, sizeof(commandLine));

    // Get input
    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);

    // Tokenize the input
    char *token = strtok(input, " \n");
    while(token){
        if(!strcmp(token,"<")) {
            currCommand->inputFile = strdup(strtok(NULL," \n"));
        } else if(!strcmp(token,">")) {
            currCommand->outputFile = strdup(strtok(NULL," \n"));
        } else if(!strcmp(token,"&")) {
            currCommand->isBg = true;
        } else {
            currCommand->argv[currCommand->argc++] = strdup(token);
        }
        token=strtok(NULL," \n");
    }

    return currCommand;
} 


void freeCommandLine(commandLine* currCommand) {
    
    for (int i = 0; currCommand->argv[i] != NULL; i++) {
        free(currCommand->argv[i]);
    }
    free(currCommand->inputFile);
    free(currCommand->outputFile);

    free(currCommand);
}


int main() {
    // Array to hold background process IDs.
    int bgProcessArray[512];
    int bgProcessCount = 0;

    while(true) {
        int childStatus;
        
        // Loop through array of background process IDs and check if they are done.
        if (bgProcessCount > 0) {
            for (int i = 0; i < bgProcessCount; i++) {
                pid_t bgPid = waitpid(bgProcessArray[i], &childStatus, WNOHANG);
                if (bgPid == -1) {
                    break;
                } else if (bgPid == 0) {
                    sleep(0.25);
                    continue;
                } else if (bgPid == bgProcessArray[i]) {
                    if(WIFEXITED(childStatus)) {
                        printf("background pid %d is done: exit value %d\n", bgPid, WEXITSTATUS(childStatus));
                    } else if (WIFSIGNALED(childStatus)) {
                        printf("background pid %d is done: terminated by signal %d\n", bgPid, WTERMSIG(childStatus));
                    }
                    for (int j = i; j < bgProcessCount - 1; j++) {
                        bgProcessArray[j] = bgProcessArray[j + 1];
                    }
                    bgProcessCount--;
                    break;
                }
            }
        }

        // Presents new prompt and parses command line input into a struct.
        commandLine* currCommand = parse_input();

        // If a blank line was entered.
        if (currCommand->argc == 0){
            continue;
        }

        char* command = currCommand->argv[0];
        char firstChar = command[0];

        // If a comment line was entered, do nothing.
        if (firstChar == '#') {   
            continue;
        } 
        // Handles "cd" commands with one optional argument.
        else if (strcmp(command, "cd") == 0) {
            if (currCommand->argv[1] != NULL) {
                char* newPath = currCommand->argv[1];
                if (chdir(newPath) != 0) {
                    perror("Error changing directory");
                } 
            } else {
                char* homeVar = getenv("HOME");
                if (chdir(homeVar) != 0) {
                    perror("Error changing directory");
                }
            }  
        } 
        // Handles "exit" commands.
        else if (strcmp(command, "exit") == 0) {
            continue;
        }
        // Handles "status" commands.
        else if (strcmp(command, "status") == 0) {
            if(WIFEXITED(childStatus)) {
                printf("exit value %d\n", WEXITSTATUS(childStatus));
            } else if (WIFSIGNALED(childStatus)) {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
            }
        } 
        // Handles all other non built-in commands.
        else {
            // Fork child process to run other commands.
            pid_t childPid = fork();
            if (childPid == -1) {
                // If fork unsuccessful.
                perror("fork()\n");
                exit(1);
                continue;
            } else if (childPid == 0) {
                // Redirect stdin if input file specified.
                if (currCommand->inputFile != NULL) {
                    int inSourceFD = open(currCommand->inputFile, O_RDONLY);
                    if (inSourceFD == -1) { 
                        printf("cannot open %s for input\n", currCommand->inputFile); 
                        exit(EXIT_FAILURE); 
                    }

                    int inRedirectFD = dup2(inSourceFD, 0);
                    if (inRedirectFD == -1) { 
                        perror("error redirecting input"); 
                        exit(EXIT_FAILURE); 
                    }

                    close(inSourceFD);
                }
                
                // Redirect stdout if output file specified.
                if (currCommand->outputFile != NULL) {
                    // Redirect stdout.
                    int outSourceFD = open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outSourceFD == -1) { 
                        printf("cannot open %s for output\n", currCommand->outputFile); 
                        exit(EXIT_FAILURE); 
                    }

                    int outRedirectFD = dup2(outSourceFD, 1);
                    if (outRedirectFD == -1) { 
                        perror("error redirecting output"); 
                        exit(EXIT_FAILURE); 
                    }

                    close(outSourceFD);
                }

                // Searches PATH variable for command and executes command.
                execvp(currCommand->argv[0], currCommand->argv);

                // If command execution results in error.
                printf("%s: no such file or directory\n", currCommand->argv[0]);   
                exit(EXIT_FAILURE);

                continue;
            } else {
                if (!currCommand->isBg) {
                    childPid = waitpid(childPid, &childStatus, 0);
                } else {
                    printf("background pid is %d\n", childPid);
                    bgProcessArray[bgProcessCount] = childPid;
                    bgProcessCount++;
                }
            }
        }
        freeCommandLine(currCommand);
    }
    return EXIT_SUCCESS;
}