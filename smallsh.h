/*
Smallsh program
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAXCHARS 2048
#define MAXARGS 512

// initialize global variable for toggling TSTP state
// (foreground-only mode: value of 1)
bool fgOnlyState = 0;

//smallsh command structure
typedef struct smallshCommand{
    int argc;
    char *argv[MAXARGS];
    char *input_file;
    char *output_file;
    bool bg;
} COMMAND;

// smallsh background process structure for linked link
typedef struct bgProcess{
    int pid;
    struct bgProcess *next;
    struct bgProcess *prev;
} PROCESS;

// custom TSTP signal handler
void TSTP_handler(int signo){
    char* message;
    if(fgOnlyState == 0){
        fgOnlyState = 1;
        message = "\nEntering foreground-only mode(& is now ignored)\n";
        write(STDOUT_FILENO, message, 49);
    }
    else{
        fgOnlyState = 0;
        message = "\nExiting foreground-only mode\n\n";
        write(STDOUT_FILENO, message, 30);
    }
    
}

// replace any '$$' found in an argument string to the pid number of the process
char* expandVar(char* argStr){
    // set up variables
    int pid = getpid();
    bool expand = false;
    char argBuf[MAXCHARS] = {'\0'};
    // loop through the characters of the argument given
    for(int i = 0; i < strlen(argStr); i++){
        // if '$' is enountered
        if(argStr[i] == '$'){
            // check if previous character was also '$', setting expand to true
            if(expand){
                // if so, replace '$$' with the pid number of process
                sprintf(argBuf + strlen(argBuf), "%d", pid);
                // reset expand to false and continue
                expand = false; 
            }
            // first encounter of '$', set expand to true and continue next character
            else{
                expand = true;
            }
        }
        // or if the current character is NOT '$' but the last character was, setting off expand flag
        else if (argStr[i] != '$' && expand){
            // add the '$' back to the argument string as well as current character
            sprintf(argBuf + strlen(argBuf), "$%c", argStr[i]);
            // pair not found, reset expand flag to false
            expand = false;
        }
        // otherwise '$' is not the current character, add to string and continue to next
        else{
            sprintf(argBuf + strlen(argBuf), "%c", argStr[i]);
        }
    }
    // set pointer of argStr to the expanded version contained in buffer
    argStr = argBuf;
    // return expanded argument
    return argStr;
}

/*
Takes in pointer to input buffer string, assigns arguments to array, and returns number of arguments parsed
@param pointer to input string
@param pointer to array of pointers
@returns int of arguments parsed
*/
COMMAND* processInput(char* inputBuffer) {
	// initialize arguement counter
    COMMAND* curCommand = malloc(sizeof(COMMAND));
    // if not able to allocate memory, alert user and exit
    if(curCommand == NULL){
        printf("Error: Unable to allocate memory on heap section\n");
        fflush(stdout);
        exit(1);
    }
    // set default values of command
    curCommand->argc = 0;
    curCommand->input_file = NULL;
    curCommand->output_file = NULL;
    curCommand->bg = false;
	// get first argument from input buffer
	char* curArg = strtok(inputBuffer, " ");
	// check if argument is a comment or empty commands
	if (inputBuffer[0] == '#' || curArg == NULL) {
		// set first argument as null
		curCommand->argv[0] = NULL;
		return curCommand;
	}
	// non-zero command arguments, loop from input buffer
    while(curArg != NULL) {
		
        // if argc reach limit, set first argument to null
		if (curCommand->argc == MAXARGS) {
			curCommand->argv[0] = NULL;
			printf("Error: Command line exceeded %d arguments\n", MAXARGS);
			fflush(stdout);
            return curCommand;
		}
        // check if prompt redirects to an input file
        else if(strcmp(curArg, "<") == 0){
            // get input file given
            curArg = strtok(NULL, " ");
            curArg = expandVar(curArg);
            curCommand->input_file = calloc(strlen(curArg) + 1, sizeof(char));
            strcpy(curCommand->input_file, curArg);
            curArg = strtok(NULL, " ");
            continue;
        }
        // check if prompt redirects to an output file
        else if(strcmp(curArg, ">") == 0){
            // get output file given
            curArg = strtok(NULL, " ");
            curArg = expandVar(curArg);
            curCommand->output_file = calloc(strlen(curArg) + 1, sizeof(char));
            strcpy(curCommand->output_file, curArg);
            curArg = strtok(NULL, " ");
            continue;
        }
        // check if prompt requested the command to be run in the background
        else if(strcmp(curArg, "&") == 0){
            // check to see if symbol is at end of prompt
            char* nextArg = strtok(NULL, " ");
            // if there is no next token, set background as true
            if(nextArg == NULL){
                curCommand->bg = true;
                break;
            }
            // otherwise treat '&' as normal text
            else{
                curCommand->argv[curCommand->argc] = calloc(strlen(curArg) + 1, sizeof(char));
                strcpy(curCommand->argv[curCommand->argc], curArg);
                curCommand->argc++;
                curArg = nextArg;
                continue;
                
            }
        }
        // otherwise add argument to command and expand any '$$'
        else{
            curArg = expandVar(curArg);
            curCommand->argv[curCommand->argc] = calloc(strlen(curArg) + 1, sizeof(char));
            strcpy(curCommand->argv[curCommand->argc], curArg);
            // printf("argument given: %s\n", curCommand->argv[curCommand->argc]);
            curCommand->argc++;
            // get next argument from input buffer
            curArg = strtok(NULL, " ");
            continue;
        }
		
	};
    // null terminate command argv
    curCommand->argv[curCommand->argc] = NULL;
	return curCommand;
}

/*
This returns the built-in command number, with zero defaulting to not builtin
	@param pointer to command string name
	@return int
*/
int commandList(char* command) {
	// Define number of built-in commands supported by smallsh
	// (UPDATE as new commands are made)
	int numBuiltIn = 5;
	// Initialize built-in list of smallsh commands
	char* BuiltInCommands[numBuiltIn];
	// Initialize call number, default as zero (implying no match yet)
	int callNum = 0;
	// set smallsh current built-in commands
	// (ADD ADDITIONAL SMALLSH COMMANDS HERE)
	BuiltInCommands[0] = "exit";
	BuiltInCommands[1] = "cd";
	BuiltInCommands[2] = "status";
    BuiltInCommands[3] = "help";
    BuiltInCommands[4] = "docs";
	// Loop through the number of possible built-in commands
	for (int i = 0; i < numBuiltIn; i++) {
		// if provided command is a match
		if (strcmp(BuiltInCommands[i], command) == 0) {
			//Then return the command number based on the list above
			return i + 1;
		}
	}
	// else if no match was found, return 0
	return 0;
}

// loop through background processes and terminate them
void killProcList(PROCESS* procList){
    int exitStatus;
    // loop through processes in proccess list
    while(procList != NULL){
        kill(procList->pid, SIGKILL);
        // wait for termination of child process
        waitpid(procList->pid, &exitStatus, 0);
        // once caught, move to next child process
        procList = procList->next;
    }
    // once done, return to parent process (shell)
    return;
}


// return absolute path for relative path provided
char* convertToAbsolutePath(char* relativePath) {
    char buffer[MAXCHARS];
    char* absolutePath;
    // get current working directory path
    if (getcwd(buffer, sizeof(buffer)) != NULL) {
        // check if path uses local directory reference pointers
        if (relativePath[0] == '.') {
            // check if relative path is to parent directory
            if (strlen(relativePath) == 2) {
                if (relativePath[1] == '.') {
                    // step back to parent folder by removing current folder
                    char* ptr = strrchr(buffer, '/');
                    *ptr = '\0';
                }
            }
            
        }
        // use normal relative path
        else {
            sprintf(buffer + strlen(buffer), "/%s", relativePath);
        }
        absolutePath = buffer;
        return absolutePath;
    }
}


// calls the built-in cd function of the smallsh program
void callCD(COMMAND* curCommand) {
	// Initialize variables
	int flag;
    char* filePath;
	char* PWD = "PWD";
    // check if proper number of arguments were given
    if(curCommand->argc > 2){
        // alert user of sytax error with cd command
		printf("Error: Incorrect Sytax for cd command\n");
        fflush(stdout);
        return;
    }
	// otherwise if no path is specified, change to HOME path
	if (curCommand->argc == 1) {
		// get path to home from environment
		filePath = getenv("HOME");
	}
	// OR path is specified, check for absolute or relative paths
	else if (curCommand->argc == 2) {
		// If path is absolute, beginning with '/' 
		if (curCommand->argv[1][0] == '/') {
			filePath = curCommand->argv[1];
		}
		// if path is relative, not beginning with '/'
		else {
            // convert relative path to absolute path
            filePath = convertToAbsolutePath(curCommand->argv[1]);
		}
	}
    // use filePath to change directory
    flag = chdir(filePath);
    // if unsuccessful, alert user
    if (flag) {
        printf("Error: chdir() to %s failed\n", filePath);
        fflush(stdout);
        return;
    }
    // set PWD environment variable to home path, overwriting exiting value
    if(setenv(PWD, filePath, 1) == -1){
        printf("failure to set env 'PWD' variable to %s\n", filePath);
        fflush(stdout);
    };
    // alert user of changed directory
    printf("Changed to directory: %s\n", getenv(PWD));
    return;
}


// print the signal or exit status of last process
void callStatus(int exitStatus){
    // print prior exit status if command exited normally
    if(WIFEXITED(exitStatus)){
        printf("exit status %d\n", WEXITSTATUS(exitStatus));
        fflush(stdout);
    }
    // else command exited abnornmally, return signal
    else{
        printf("terminated by signal %d\n", WTERMSIG(exitStatus));
        fflush(stdout);
    }
    return;
}

void callDocs(char* filename) {
    FILE* fp;
    char ch;
    fp = fopen(filename, "r");
    if (fp)
    {
        while ((ch = fgetc(fp)) != EOF)
        {
            printf("%c", ch);
        }
    }
    fclose(fp);
    return;
}


// add process pid to the proccess linked list
void addProc(int newPid, PROCESS** procList){
    // pointer to value at address of process list 
    PROCESS *curProcess = *procList;
    // allocate memory for new process structure 
    PROCESS *newProcess = malloc(sizeof(PROCESS));
    // assign process pid to pid provided
    newProcess->pid = newPid;
    newProcess->next == NULL;
    // if the process list is empty, add to head
    if(curProcess == NULL){
        *procList = newProcess;
        newProcess->prev = NULL;
        return;
    }
    // otherwise go to end of list and add new process
    else{
        while(curProcess->next != NULL){
            curProcess = curProcess->next;
        }
        curProcess->next = newProcess;
        newProcess->prev = curProcess;
        return;
    }
}

// clean up zombie processes between terminal calls and remove terminated ones from list
void cleanProc(PROCESS** procList){
    int status, pid;
    
    // loop through background process list
    while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0){
        // get exit status if command exited normally
        if(WIFEXITED(status)){
            status = WEXITSTATUS(status);
        }
        // else command exited abnornmally, get signal
        else{
            status = WTERMSIG(status);
        }
        printf("Background pid %d is done: exit value %d\n", pid, status);
        fflush(stdout);
        // remove from process list
        PROCESS *prev = NULL;
        PROCESS *cur = *procList;
        while(cur != NULL){
            // if the current process matches zombie pid, remove from list
            if(cur->pid == pid){
                // first process of process list finished
                if(prev == NULL){
                    *procList = cur->next;
                    free(cur);
                    break;
                }
                // last process of process list finished 
                else if(cur->next == NULL){
                    cur->prev->next = NULL;
                    free(cur);
                    break;
                }
                // otherwise a process within list has finished, remove from list
                else{
                    PROCESS *temp = cur->next;
                    cur->prev->next = cur->next;
                    temp->prev = cur->prev;
                    free(cur);
                    break;
                }
            }
            // otherwise the zombie pid doesn't match, move to next process in list
            else{
                prev = cur;
                cur = cur->next;
                continue;
            }
        } 
    }
}


// executes command, setting any redirects and set/return exit status
int execCommand(COMMAND* curCommand, int exitStatus, PROCESS **procList){
    int inputFD, outputFD, saved_stdin, saved_stdout;
    int childStatus;
    struct sigaction SIGINT_action = {0}, ignore_action;
    // fill out the SIGINT_action
    // Register handle_SIGINT as the signal handler
    SIGINT_action.sa_handler = SIG_DFL;
    sigset_t oldMask;
    sigset_t blockset;
    sigfillset(&oldMask);
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGTSTP);
    // for a new process
    pid_t spawnPid = fork();
    
    switch(spawnPid){
        case -1:
            // process failed to fork properly, alert user and give exit status of 1
            perror("fork() failed");
            fflush(stdout);
            exit(1);
        case 0:
            // child process path
            // The ignore_action is set to SIG_IGN as its signal handler
            ignore_action.sa_handler = SIG_IGN;
            // register the ignore_action as the handler for SIGTSTP
            sigaction(SIGTSTP, &ignore_action, NULL);
            // register SIGINT_handler to child process if foreground
            if(!curCommand->bg && !fgOnlyState){
                // install the signal handler
                sigaction(SIGINT, &SIGINT_action, NULL);
            }
            // handle any input redirection
            // open input file 
            if ((curCommand->input_file) != NULL) {
                inputFD = open(curCommand->input_file, O_RDONLY);
                // if the input file fails to open, alert user and give exit status of 1
                if (inputFD == -1){
                    printf("cannot open %s to input\n", curCommand->input_file);
                    fflush(stdout);
                    exit(1);
                }
                // make sure that input descriptor is set to close for successful execution
                fcntl(inputFD, F_SETFD, FD_CLOEXEC);
                // save current stdin to return to later...
                saved_stdin = dup(0);
                // duplicate and redirect to inputFD rather than stdin
                if(dup2(inputFD, 0) == -1){
                    // if fail to redirect, alert user and given exit status of 1
                    printf("Error: dup2() for input\n");
                    fflush(stdout);
                    exit(1);
                }
            }

            // handle any output redirection
            // open output file 
            if ((curCommand->output_file) != NULL) {
                outputFD = open(curCommand->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                // if the output file fails to open, alert user and give exit status of 1
                if (outputFD == -1) {
                    printf("cannot open %s to output\n", curCommand->output_file);
                    fflush(stdout);
                    exit(1);
                }
                // make sure that output descriptor is set to close for successful execution
                fcntl(outputFD, F_SETFD, FD_CLOEXEC);
                // save current stdout to return to later...
                saved_stdout = dup(1);
                // duplicate and redirect to outputFD rather than stdout
                if(dup2(outputFD, 1) == -1){
                    // if fail to redirect, alert user and given exit status of 1
                    printf("Error: dup2() for output\n");
                    fflush(stdout);
                    exit(1);
                }
            }
            
            // execute command given now that redirects are handled
            execvp(curCommand->argv[0], curCommand->argv);
            
            // if exec fails, reset redirects to stdin and stdout and close duplicate file descriptors
            printf("%s: execution of command failed\n", curCommand->argv[0]);
            fflush(stdout);
            if ((curCommand->input_file) != NULL) {
                dup2(saved_stdin, 0);
                close(inputFD);
                close(saved_stdin);
            }
            if ((curCommand->output_file) != NULL) {
                dup2(saved_stdout, 1);
                close(outputFD);
                close(saved_stdout);
            }
            // since exec failed, set exit status to 1
            exit(1);
        default:
            // parent process path (shell)
            sigprocmask(SIG_BLOCK, &blockset, &oldMask);
            // Check if command was set to be in background
            if(curCommand->bg && !fgOnlyState){
                // if so, print child pid and give the no wait signal for waitpid
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
                int returnPid = waitpid(spawnPid, &childStatus, WNOHANG);
                // create new process to add to process list
                addProc(spawnPid, procList);
                // don't change exit status, leave as previous foreground process
                return exitStatus;
            }
            // otherwise command is run in the foreground, waiting for child process to finish
            else{
                spawnPid = waitpid(spawnPid, &exitStatus, 0);
                sigprocmask(SIG_UNBLOCK, &blockset, &oldMask);
                // return exit status if command exited normally
                if(WIFEXITED(exitStatus)){
                    return exitStatus;
                }
                // else command exited abnornmally, return signal
                else{
                    childStatus = WTERMSIG(exitStatus);
                    printf("terminated by signal %d\n", childStatus);
                    return exitStatus;
                }
            }
            
    }
}
