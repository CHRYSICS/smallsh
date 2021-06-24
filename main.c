/*
Assignment 3
Author: Christopher Eckerson
Date: 2/8/2021
Summary: smallsh program
*/
#include <stdio.h>
#include <stdlib.h>
#include "smallsh.h"

// get input and store in buffer to use in smallsh shell
void getInput(char* inputBuffer){
    int ch, i;
    i = 0;
    // loop through stdin until end of line 
    while ((ch = fgetc(stdin)) != '\n'){
        inputBuffer[i] = ch;
        i++;
        if(ferror(stdin)){
            clearerr(stdin);
            inputBuffer[0] = '\0';
            break;
        }
        // if input exceeds character limit
        if (i == MAXCHARS + 1){
            // reset input buffer to null
            inputBuffer[0] = '\0';
            // clear stdin buffer and print error message
            while((ch = fgetc(stdin)) != '\n' && ch != EOF){};
            printf("Error: Command line exceeded %d characters\n", MAXCHARS);
            fflush(stdout);
            break;
        }
    }
    // add termination character to input buffer
    inputBuffer[i] = '\0';
    return;
}


int main(){
    // initialize input variables
    char inputBuffer[MAXCHARS + 1];
    COMMAND *curCommand;
    PROCESS *procList = NULL;
    PROCESS *curProc;
    struct sigaction ignore_action = {0}, SIGTSTP_action = {0};
    int exitStatus = 0;
    int callNum;

    // fill out the SIFTSTP_action struct
    // Register handle_TSTP as the signal handler
    SIGTSTP_action.sa_handler = TSTP_handler;
    // block all catachable signals while TSTP_handler is running
    sigfillset(&SIGTSTP_action.sa_mask);
    // no flags set
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // The ignore_action is set to SIG_IGN as its signal handler
    ignore_action.sa_handler = SIG_IGN;
    // register the ignore_action as the handler for SIGINT
    sigaction(SIGINT, &ignore_action, NULL);

    printf("Smallsh: Terminal Shell Project\n");
    printf("(Enter 'help' to get usability documentation)\n");

    
    // loop terminal prompt and execution of commands
    while(1){
        // print prompt line symbol
        printf(": ");
        fflush(stdout);
        getInput(inputBuffer);
        curCommand = processInput(inputBuffer);
        // if command was a comment, blank or exceeded limit
        if(curCommand->argv[0] == NULL){
            continue;
        }
        callNum = commandList(curCommand->argv[0]);
        // determine which command is being called
        switch(callNum){
            case 1:
                // call exit built-in command
                killProcList(procList);
                exit(0);
                break;
            case 2:
                // call cd built-in command
                callCD(curCommand);
                break;
            case 3:
                // call status built-in command
                callStatus(exitStatus);
                break;
            case 4:
                // call help built-in command
                callDocs("HELP.txt");
                break;
            case 5:
                // call docs built-in command
                callDocs("README.txt");
                break;
            default:
                // call non-builtin command through exec
                exitStatus = execCommand(curCommand, exitStatus, &procList);       
                cleanProc(&procList);
                break;
        }
    }
    free(curCommand);
    return 0;
}