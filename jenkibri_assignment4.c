#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


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
    while(true) {
        // Parses the command line into a struct.
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
        } else if (strcmp(command, "cd") == 0) {
            continue;
        } else if (strcmp(command, "exit") == 0) {
            continue;
        } else if (strcmp(command, "status") == 0) {
            continue;
        } else {
            // Fork child process to run other commands.
            int childStatus;
            pid_t spawnPid = fork();
            switch(spawnPid) {
                case -1:
                    // If fork unsuccessful.
                    perror("fork()\n");
                    exit(1);
                    break;
                case 0:
                    // Searches PATH variable for command and executes command.
                    execvp(currCommand->argv[0], currCommand->argv);

                    // If command execution results in error.
                    perror("execv");   
                    exit(EXIT_FAILURE);
                    break;
                default:
                    spawnPid = waitpid(spawnPid, &childStatus, 0);
            }
        }
        freeCommandLine(currCommand);
    }
    return EXIT_SUCCESS;
}