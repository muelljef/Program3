//Author:           Jeff Mueller
//Date Created:     28-July-2015
//Description:      This is a basic shell program: that has 3 built in commands status, cd, and exit
//                  All arguments must be separated by spaces
//                  exit does not accept any other parameters
//                  cd accepts 0 or 1 parements, just cd will change directory to path specified by HOME
//                  status will output the last foreground process exit status
//                  The shell ignores blank lines and lines starting with # (as comments)
//                  The shell supports input and output redirection < and >
//                  The & will execute the process in the background

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

//Max buffer size and arguments as specified by the assignment
#define BUFFER 2048
#define ARGSIZE 512

//function declarations
void shSplit(char *line, char **args);
void freeArgs(char **args);
char* shRead();
void clearBuff(char buff[PATH_MAX + 1]);
void killChProcs(pid_t *allPid);
void checkBgProcs();
void shExe(char **args, int *fgStat, struct sigaction *fgChild, pid_t *allPid);
void cdFunc(char **args);
void safeStringCat(char *dest, const char *source, int destMax);

int main()
{
    char *line;                 //to take user input for commands
    char **args;                //to parse user input to separate commands
    int shLoop = 1;             //while loop boolean
    int fgStat = 0;             //status of last foreground execution
    int i;
    pid_t allPid[1000];         //array that holds all pids

    //initialize allPid's values to 0
    for(i = 0; i < 1000; i++)
    {
        allPid[i] = 0;
    }

    //Setting up the signal interrupt handler
    //to catch Ctrl-C
    struct sigaction act, fgChild;
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    //for returning to default
    fgChild.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    //set the sigaction to act handler
    sigaction(SIGINT, &act, &fgChild);

    do {
        //assistance for allocating the args array dynamically taken from the following site
        //http://stackoverflow.com/questions/7652293/how-do-i-dynamically-allocate-an-array-of-strings-in-c
        args = (char **) malloc(sizeof(char *) * ARGSIZE);
        if(args == NULL){
            //memory did not allocate properly
            perror("args malloc");
            exit(1);
        }

        //Check for background process completion
        checkBgProcs();

        //printing the prompt, reading input, and tokenizing input
        printf(": ");
        fflush(stdout);         //flush the stream
        line = shRead();        //read user input
        shSplit(line, args);    //parse the input into array of arguments

        //Handling blank input
        if (args[0] == NULL)
        {
            //free the line and arguments strings
            free(line);
            freeArgs(args);
            continue;
        }

        //Handle comments, builtins, then execute if none of those
        if (strcmp(args[0], "#") == 0)
        {
            //Handling comments
            //free the line and arguments strings
            free(line);
            freeArgs(args);
            continue;
        }
        else if(strncmp(args[0], "exit", strnlen(args[0], 5)) == 0)
        {
            //make sure all children processes have been killed
            killChProcs(allPid);
            //free the line and arguments strings
            free(line);
            freeArgs(args);
            exit(0);
        }
        else if (strncmp(args[0], "status", strnlen(args[0], 7)) == 0)
        {
            //referenced http://linux.die.net/man/2/waitpid
            //for example code and definitions
            //if exited normally
            if (WIFEXITED(fgStat))
            {
                //print the exit status
                printf("exit value %d\n", WEXITSTATUS(fgStat));
            }
            //if exited by signal
            else if (WIFSIGNALED(fgStat))
            {
                //print the signal that terminated
                printf("terminated by signal %d\n", WTERMSIG(fgStat));
            }
        }
        else if (strncmp(args[0], "cd", strnlen(args[0], 3)) == 0)
        {
            cdFunc(args);
        }
        else
        {
            shExe(args, &fgStat, &fgChild, allPid);
        }

        //free the line and arguments strings before looping through again.
        free(line);
        freeArgs(args);
    } while(shLoop);

    return 0;
}

//*****************************************************************************
//                         SHELL EXECUTION FUNCTION
//*****************************************************************************

//Entry:    requires an array of dynamiclaly allocated strings as arguments,
//          an address of an integer to keep the foreground status
//          an address of a struct sigaction to change child fg proc to default
//          an array of pid_t to hold all the child procs pid created
//Exit:     will execute based on args array a background or foreground process
//          adding the process to the allPid array, updating the fgStat.
//          function waits for fg procs and continues after launching bg procs
void shExe(char **args, int *fgStat, struct sigaction *fgChild, pid_t *allPid)
{
    //pos and i as iterators,
    //fd, fd2, out, in as file desc
    int pos, fd, fd2, out, in, i;
    pid_t spawnpid, exitpid;        //to catch fork() and waitpid() pid_t
    int bg = 0;                     //boolean for background process

    //loop through the arguments and set the bg flag if "&" found
    pos = 0;
    while(args[pos] != NULL)
    {
        if(strcmp(args[pos], "&") == 0)
        {
            bg = 1;
        }
        pos++;
    }

    //create a child process
    spawnpid = fork();
    if (spawnpid == 0)
    {
        //CHILD
        if (bg != 1)
        {
            //FOREGROUND PROCESS
            //change the siging action back to default
            //for the fg process
            sigaction(SIGINT, fgChild, NULL);
        }

        //initialize out and in fds to invalid numbers
        out = -5;
        in = -5;

        //check for IO redirection
        //set bg IO redirection if needed
        pos = 0;
        while(args[pos] != NULL)
        {
            if(strncmp(args[pos], "<", 1) == 0)
            {
                //input redirection
                fd2 = open(args[pos + 1], O_RDONLY);
                if(fd2 == -1)
                {
                    perror("file read");
                    exit(1);
                }
                in = dup2(fd2, 0);
                if(in == -1)
                {
                    perror("dup2");
                    exit(1);
                }
                //free the argument and set to null to prevent exec from using
                //the command
                free(args[pos]);
                args[pos] = NULL;
            }
            else if (strncmp(args[pos], ">", 1) == 0)
            {
                //output redirection
                fd = open(args[pos + 1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if(fd == -1)
                {
                    perror("file write");
                    exit(1);
                }
                out = dup2(fd, 1);
                if(out == -1)
                {
                    perror("dup2");
                    exit(1);
                }
                //free the argument and set to null to prevent exec from using
                //the command
                free(args[pos]);
                args[pos] = NULL;

            }
            else if (strncmp(args[pos], "&", 1) == 0)
            {
                //BACKGROUND PROCESS
                //since & must come last IO redirect will be set
                //if user specified any

                //if user did not specify out, redirect to /dev/null
                if (out < 0)
                {
                    fd = open("/dev/null", O_WRONLY);
                    if(fd == -1)
                    {
                        perror("bg file write");
                        exit(1);
                    }
                    out = dup2(fd, 1);
                    if(out == -1)
                    {
                        perror("bg dup2");
                        exit(1);
                    }
                }
                //if user did not specify in, redirect to /dev/null
                if (in < 0)
                {
                    fd2 = open("/dev/null", O_RDONLY);
                    if(fd2 == -1)
                    {
                        perror("bg file read");
                        exit(1);
                    }
                    in = dup2(fd2, 0);
                    if(in == -1)
                    {
                        perror("bg dup2");
                        exit(1);
                    }
                }
                //free the argument and set to null to prevent exec from using
                //the command
                free(args[pos]);
                args[pos] = NULL;
            }
            pos++;
        }
        //execute the arguments
        execvp(args[0], args);
        //if an error occurred with execution report the error
        perror(args[0]);
        //return from child process with exit status 1
        exit(1);
    }
    else if (spawnpid > 0)
    {
        //PARENT
        //find the next 0 value in the allPid array
        //and add the spawnid to the allPid array there
        for(i = 0; i < 1000; i++)
        {
            if(allPid[i] == 0)
            {
                allPid[i] = spawnpid;
                break;
            }
        }

        if(bg == 1)
        {
            //BACKGROUND PROCESS
            //print the bg proc id
            printf("background pid is %d\n", (int)spawnpid);
        }
        else
        {
            //FOREGROUND PROCESS
            //wait for the recently created spawnid as that is the
            //foreground processx
            exitpid = waitpid(spawnpid, fgStat, 0);
            //check the exit status
            //if greater than 0, then a process was waited on
            if (exitpid == -1)
            {
                perror("fg wait");
            }
            else if(exitpid > 0)
            {
                //if the process was terminate by a signal, notify the user which one
                if (WIFSIGNALED(*fgStat))
                {
                    printf("terminated by signal %d\n", WTERMSIG(*fgStat), (int)exitpid);
                }
            }
        }
    }
    else
    {
        printf("fork failed\n");
    }

    //check for all waiting background processes
    checkBgProcs();
}

//*****************************************************************************
//                              HELPER INPUT FUNCTIONS
//*****************************************************************************

// Entry:   none
// Exit:    return a pointer to a C-string
// adjusted read in code from http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
// to read in the max buffer size specified in the assignment specifications
char* shRead()
{
    int c;
    int position = 0;
    char *buffer = malloc(sizeof(char) * BUFFER);

    //if the buffer did not allocate
    //exit and report the error
    if (!buffer)
    {
        perror("smallsh");
        exit(1);
    }

    while (1)
    {
        //get a character from the input
        c = getchar();

        if (c == EOF || c == '\n')
        {
            //if newline or end of file
            //replace with null and return the input
            buffer[position] = '\0';
            return buffer;
        }
        else
        {
            //otherwise add the character to the string
            buffer[position] = c;
        }
        //increment position
        position++;

        //If we have exceeded the buffer exit
        if (position >= BUFFER)
        {
            printf("Exceeded buffer\n");
            return NULL;
        }
    }
}

// Entry:   A dynamically allocated string that has arguments separated by spaces
//          args must have the array of (char *) dynamically allocated prior to the call
// Exit:    line is tokenized into arguments separated by the spaces and copies of those
//          arguments are dynamically allocated strings pointed to by the args array.
//          Any empty spaces in the args array are filled with NULL
//          NOTE: function will exit if there are more arguments than ARGSIZE - 1 (511)
//          as execv functions require the array to end in a NULL.
void shSplit(char *line, char **args)
{
    //container for the arguments
    char *token;
    int i;
    const char delimiters[] = " ";
    int pos = 0;

    //initialize the strings to NULL
    for(i = 0; i < ARGSIZE; i++)
    {
        args[i] = NULL;
    }

    //tokenize all the arguments from the line
    token = strtok(line, delimiters);
    while (token != NULL)
    {
        //Exit if too many arguments
        if (pos >= ARGSIZE - 1)
        {
            perror("arguments");
            exit(1);
        }

        //copy the string with a
        args[pos] = strdup(token);
        if(args[pos] == NULL) exit(1);   //string did not allocate properly
        //take the next string
        token = strtok(NULL, delimiters);
        //increment the count
        pos++;
    }

    return;
}

//*****************************************************************************
//                              CD FUNCTIONS
//*****************************************************************************

//Entry:    A dynamically allocated array of strings where "cd" is the first arg
//Exit:     Working directory will be changed based on the 2nd argument if given
//          A "cd" only will change directory to path specified by HOME
void cdFunc(char **args)
{
    char buff[PATH_MAX + 1];    //to store path for cd command

    if (args[1] == NULL)
    {
        //cd command alone
        chdir(getenv("HOME"));
    }
    else
    {
        if(args[1][0] == '/')
        {
            //ABSOLUTE PATH
            //change to path in args 1
            chdir(args[1]);
        }
        else
        {
            //RELATIVE PATH
            //moving down a folder (eg "cd cs344")
            if (getcwd(buff, PATH_MAX) != NULL)
            {
                //buff has the current working directory
                //append a "/"
                safeStringCat(buff, "/", PATH_MAX + 1);
                //append the folder name
                safeStringCat(buff, args[1], PATH_MAX + 1);
                //change directory
                chdir(buff);
                clearBuff(buff);
            }
        }
    }
}

// Entry: A string of size (PATH_MAX + 1)
// Exit: A string of size (PATH_MAX + 1) with all values set to '\0'
void clearBuff(char buff[PATH_MAX + 1])
{
    int i = 0;
    for(;i < PATH_MAX + 1; i++)
    {
        buff[i] = '\0';
    }
}

//*****************************************************************************
//                              CLEANUP FUNCTIONS
//*****************************************************************************

// Entry:   A dynamically allocated (ARGSIZE) array of strings
// Exit:    Memory for the array is freed
void freeArgs(char **args)
{
    int i;
    for(i = 0; i < ARGSIZE; i++)
    {
        free(args[i]);
        args[i] = NULL;
    }
    free(args);
    args = NULL;
}

// Entry:   An array of child processes where all child processes are stored at
//          the beginning of the array and the rest of the array is filled with
//          0's
// Exit:    All proc id's will have the kill command called on them to ensure all
//          children processes have ended
void killChProcs(pid_t *allPid)
{
    int i;
    for(i = 0; i < 1000; i++)
    {
        if(allPid[i] == 0) break;
        kill(allPid[i], SIGKILL);
    }
}

//*****************************************************************************
//                        BACKGROUND PROCESS FUNCTIONS
//*****************************************************************************

// Entry: call to function
// Exit: when all waiting background processes have been waited on and status
//          printed to stdout
void checkBgProcs()
{
    pid_t bgPid;
    int bgStat;
    bgPid = 0;
    do {
        //Check for background processes
        bgPid = waitpid(-1, &bgStat, WNOHANG);
        if(bgPid > 0)
        {
            //referenced code on following page at bottom
            //http://man7.org/linux/man-pages/man2/wait.2.html
            if (WIFEXITED(bgStat))
            {
                printf("background pid %d is done: exit value %d\n", (int)bgPid, WEXITSTATUS(bgStat));
            }
            else if (WIFSIGNALED(bgStat))
            {
                printf("background pid %d is done: terminated by signal %d\n", (int)bgPid, WTERMSIG(bgStat));
            }
        }
    } while (bgPid > 0);
}

//*****************************************************************************
//                              STRING FUNCTIONS
//*****************************************************************************

//entry:    destination string, source string to append on destination string
//          and and integer indicating the maximum size of the destination string
//exit:     The source will be appended to the destination up to the destinations
//          max length - 1 where a null character is guaranteed
void safeStringCat(char *dest, const char *source, int destMax)
{
    size_t destLength, catLength;
    //size cannot be less than zero
    if( destMax < 0 ) exit(1);
    //get the length of the string in the destination
    destLength = strnlen(dest, destMax);
    //if the destination is smaller than it's max size
    if( destLength < destMax ) {
        //set the catLength to size - destLenght
        catLength = destMax - destLength;
        //Concat the string
        strncat(dest, source, catLength);
        //ensure the terminating null is in there
        dest[destMax - 1] = '\0';
    }
}
