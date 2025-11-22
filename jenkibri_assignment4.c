/**
 * Author: Brice Jenkins
 * ONID: jenkibri
 * Class/Section: CS374 Operating Systems I
 * Assignment: Programming Assignment 4 - SMALLSH
 * Date: 11/22/2025
 * Description: Implements a shell, smallsh, that provides a prompt for
 *              running commands; executes three commands (exit, cd, and
 *              status) built into the shell; executes other shell commands
 *              by creating a new process using an exec() family function;
 *              supports input and output redirection; supports running
 *              commands in foreground and background processes; and contains
 *              custom handlers for SIGINT and SIGTSTP. 
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>


// Macros for max length of command and max number of arguments.
#define INPUT_LENGTH 2048
#define MAX_ARGS 512

// Set to true or false via Ctrl-Z (if true, background processes are not allowed).
bool fgOnlyMode = false;

/**
 * @struct commandLine
 * @brief Represents a shell command with arguments (including the command),
 * and input file and/or output file name (for file redirection) and whether
 * the command is to run in the background or not.
 */
typedef struct commandLine {
    char* argv[MAX_ARGS + 1];
    int argc;
    char* inputFile;
    char* outputFile;
    bool isBg;
} commandLine;

/**
 * parseInput - Reads command input from a user and parses the command into
 * a commandLine struct.
 * @returns A pointer to a commandLine struct containing the command data.
 */
commandLine *parseInput(void) {
    char input[INPUT_LENGTH];
    commandLine* currCommand = (commandLine*) calloc(1, sizeof(commandLine));

    // Get input from user.
    printf(": ");
    fflush(stdout);

    // To block errors returned when ^Z interrupts input.
    if(fgets(input, INPUT_LENGTH, stdin) != NULL) {
        // Tokenize the input and store in commandLine struct.
        char *token = strtok(input, " \n");

        while(token){
            if(!strcmp(token,"<")) {
                currCommand->inputFile = strdup(strtok(NULL," \n"));
            } else if(!strcmp(token,">")) {
                currCommand->outputFile = strdup(strtok(NULL," \n"));
            } else if(!strcmp(token,"&")) {
                if (!fgOnlyMode) currCommand->isBg = true;
            } else {
                currCommand->argv[currCommand->argc++] = strdup(token);
            }
            token=strtok(NULL," \n");
        }
    }
    return currCommand;
} 

/**
 * freeCommandLine - Frees dynamic memory allocated to a commandLine struct.
 * @param currCommand A pointer to a currCommand struct.
 */
void freeCommandLine(commandLine* currCommand) {
    // Loops through arguments array and frees strdup memory.
    for (int i = 0; currCommand->argv[i] != NULL; i++) {
        free(currCommand->argv[i]);
    }
    free(currCommand->inputFile);
    free(currCommand->outputFile);
    free(currCommand);
}

/**
 * handleSIGINT - Returns termination message for foreground processes stopped
 * by SIGINT.
 */
void handleSIGINT(int signo) {
    char message[] = "terminated by signal 2\n";
    write(STDOUT_FILENO, &message, 24); 
}

/**
 * handleSIGTSTP - When SIGTSTP is received, the global variable fgOnlyMode is
 * set to true or false and then the appropriate message is printed. 
 */
void handleSIGTSTP(int signo) {
    int childStatus;

    // Waits for foreground child processes to terminate.
    while(waitpid(-1, &childStatus, 0) < -1);

    if (!fgOnlyMode) {
        fgOnlyMode = true;
        char message[] = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, &message, 50);
    } else {
        fgOnlyMode = false;
        char message[] = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, &message, 31);
    }
}

/**
 * main - The main execution flow of the shell. Searches array of background
 * processes to check terminated processes and then presents user with prompt
 * to enter command. If not a built-in command (cd, status, or exit), then
 * a child process is forked to execute other shell commands via execvp().
 * Shell continues until 'exit' is entered.
 */
int main() {
    // Array to hold background process IDs.
    int bgProcessArray[1000];
    int bgProcessCount = 0;

    while(true) {
        int childStatus;
        struct sigaction SIGINTparent, SIGTSTPparent;

        // Ignores SIGINT if entered in the shell prompt.
        SIGINTparent.sa_handler = SIG_IGN;
        sigemptyset(&SIGINTparent.sa_mask);
        SIGINTparent.sa_flags = SA_NODEFER;

        sigaction(SIGINT, &SIGINTparent, NULL);

        // Handles SIGTSTP if entered in the shell prompt.
        SIGTSTPparent.sa_handler = handleSIGTSTP;
        sigemptyset(&SIGTSTPparent.sa_mask);
        SIGTSTPparent.sa_flags = 0;

        sigaction(SIGTSTP, &SIGTSTPparent, NULL);

        // Loop through array of background process IDs and check if they are terminated.
        if (bgProcessCount > 0) {
            usleep(100000);

            for (int i = 0; i < bgProcessCount; i++) {
                pid_t bgPid = waitpid(bgProcessArray[i], &childStatus, WNOHANG);
                if (bgPid == -1) {
                    break;
                } else if (bgPid == 0) {
                    continue;
                } else if (bgPid == bgProcessArray[i]) {
                    // Print appropriate termination status.
                    if(WIFEXITED(childStatus)) {
                        printf("background pid %d is done: exit value %d\n", bgPid, WEXITSTATUS(childStatus));
                        fflush(stdout);
                    } else if (WIFSIGNALED(childStatus)) {
                        printf("background pid %d is done: terminated by signal %d\n", bgPid, WTERMSIG(childStatus));
                        fflush(stdout);
                    }
                    
                    // Remove terminated PID from array.
                    for (int j = i; j < bgProcessCount - 1; j++) {
                        bgProcessArray[j] = bgProcessArray[j + 1];
                    }

                    bgProcessCount--;
                    break;
                }
            }
        }

        // Presents new prompt and parses command line input into a struct.
        commandLine* currCommand = parseInput();

        char* command = currCommand->argv[0];
        char firstChar = command[0];

        // If a blank line or comment line is entered.
        if (currCommand->argc == 0 || firstChar == '#') continue;

        // For other user inputs.
        if (strcmp(command, "cd") == 0) {
            // If cd has specified path argument.
            if (currCommand->argv[1] != NULL) {
                // Builds path in case there are spaces in folder names.
                char newPath[INPUT_LENGTH];
                strcpy(newPath, currCommand->argv[1]);

                for (int i = 2; currCommand->argv[i] != NULL; i++) {
                    strcat(newPath, " ");
                    strcat(newPath, currCommand->argv[i]);
                }

                // If directory change results in an error.
                if (chdir(newPath) != 0) {
                    perror("Error changing directory");
                    fflush(stdout); 
                } 
            } else {
                // If only 'cd' is entered.
                char* homeVar = getenv("HOME");
                if (chdir(homeVar) != 0) {
                    perror("Error changing directory");
                    fflush(stdout); 
                }
            }

        } else if (strcmp(command, "exit") == 0) {
            // Terminate any background child processes running.
            if (bgProcessCount > 0) {
                for (int i = bgProcessCount - 1; bgProcessCount > 0; i--) {
                    if (kill(bgProcessArray[i], 15) == 0) {
                        bgProcessCount--;
                    } else {
                        printf("Failed to end process %d", bgProcessArray[i]);
                        fflush(stdout);
                    }
                }
            }
            // Terminate smallsh.
            int shellPid = getpid();
            if(kill(shellPid, 15) != 0) {
                printf("Failed to end process %d", shellPid);
                fflush(stdout);
            }
            // Terminate parent process.
            int parentPid = getppid();
            if(kill(parentPid, 15) != 0) {
                printf("Failed to end process %d", parentPid);
                fflush(stdout);
            }

        } else if (strcmp(command, "status") == 0) {
            if(WIFEXITED(childStatus)) {
                printf("exit value %d\n", WEXITSTATUS(childStatus));
                fflush(stdout);
            } else if (WIFSIGNALED(childStatus)) {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
                fflush(stdout);
            }

        } else {
            // To run other commands.
            pid_t childPid = fork();
            if (childPid == -1) {
                // If fork unsuccessful.
                perror("fork()\n");
                fflush(stdout);
                exit(1);
                continue;

            } else if (childPid == 0) {
                struct sigaction SIGINTchild, SIGTSTPchild;
                // SIGINT: default for foreground child processes but ignored for background.
                if (!currCommand->isBg) {
                    SIGINTchild.sa_handler = SIG_DFL;
                } else {
                    SIGINTchild.sa_handler = SIG_IGN;
                }
                sigemptyset(&SIGINTchild.sa_mask);
                SIGINTchild.sa_flags = 0;

                sigaction(SIGINT, &SIGINTchild, NULL);

                // SIGTSTP: ignored for foreground and background processes.
                SIGTSTPchild.sa_handler = SIG_IGN;
                sigemptyset(&SIGTSTPchild.sa_mask);
                SIGTSTPchild.sa_flags = 0;

                sigaction(SIGTSTP, &SIGTSTPchild, NULL);

                // Redirect stdin if input file specified.
                if (currCommand->inputFile != NULL) {
                    int inSourceFD = open(currCommand->inputFile, O_RDONLY);
                    if (inSourceFD == -1) { 
                        printf("cannot open %s for input\n", currCommand->inputFile);
                        fflush(stdout); 
                        exit(EXIT_FAILURE); 
                    }
                    int inRedirectFD = dup2(inSourceFD, 0);
                    if (inRedirectFD == -1) { 
                        perror("error redirecting input");
                        fflush(stdout); 
                        exit(EXIT_FAILURE); 
                    }
                    // Close original file descriptor.
                    close(inSourceFD);
                }
                
                // Redirect stdout if output file specified.
                if (currCommand->outputFile != NULL) {
                    // Redirect stdout.
                    int outSourceFD = open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outSourceFD == -1) { 
                        printf("cannot open %s for output\n", currCommand->outputFile);
                        fflush(stdout); 
                        exit(EXIT_FAILURE); 
                    }
                    int outRedirectFD = dup2(outSourceFD, 1);
                    if (outRedirectFD == -1) { 
                        perror("error redirecting output");
                        fflush(stdout); 
                        exit(EXIT_FAILURE); 
                    }
                    // Close original file descriptor.
                    close(outSourceFD);
                }

                // Searches PATH variable for command and executes command.
                execvp(currCommand->argv[0], currCommand->argv);

                // If command execution results in error.
                printf("%s: no such file or directory\n", currCommand->argv[0]);
                fflush(stdout);   
                exit(EXIT_FAILURE);
                continue;

            } else {
                if (!currCommand->isBg) {
                    // Install handler on SIGINT to deliver termination message
                    SIGINTparent.sa_handler = handleSIGINT;
                    SIGINTparent.sa_flags = SA_RESTART;

                    sigaction(SIGINT, &SIGINTparent, NULL);

                    // Wait for child to terminate.
                    while(waitpid(childPid, &childStatus, 0) < -1);

                } else {
                    // PIDs of background child processes are added to an array.
                    printf("background pid is %d\n", childPid);
                    fflush(stdout);
                    bgProcessArray[bgProcessCount] = childPid;
                    bgProcessCount++;
                }
            }
        }
        freeCommandLine(currCommand);
    }
    return EXIT_SUCCESS;
}