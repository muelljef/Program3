#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER 2048
#define ARGSIZE 512

//function declarations
void shSplit(char *line, char **args);
void freeArgs(char **args);
char* shRead();
void clearBuff(char buff[PATH_MAX + 1]);
int prevDirPos(char buff[PATH_MAX + 1]);
void killChProcs(pid_t *allPid);
void checkBgProcs();
void shExe(char **args, int *fgStat, struct sigaction *fgChild, pid_t *allPid);
void cdFunc(char **args);

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
        else if(strcmp(args[0], "exit") == 0)
        {
            //make sure all children processes have been killed
            killChProcs(allPid);
            //free the line and arguments strings
            free(line);
            freeArgs(args);
            exit(0);
        }
        else if (strcmp(args[0], "status") == 0)
        {
            if (WIFEXITED(fgStat))
            {
                printf("exit value %d\n", WEXITSTATUS(fgStat));
            }
            else if (WIFSIGNALED(fgStat))
            {
                printf("terminated by signal %d\n", WTERMSIG(fgStat));
            }
        }
        else if (strcmp(args[0], "cd") == 0)
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
//                              SHELL EXECUTION FUNCTION
//*****************************************************************************

//Entry:
//Exit:
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
            //printf("%s\n", args[pos]);
            if(strcmp(args[pos], "<") == 0)
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
            else if (strcmp(args[pos], ">") == 0)
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
            else if (strcmp(args[pos], "&") == 0)
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

// Entry:
// Exit:
// adjust code from website
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

// Entry:
// Exit:
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
        if (pos >= ARGSIZE)
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

void cdFunc(char **args)
{
    char buff[PATH_MAX + 1];    //to store path for cd command
    int upDir;                  //rel path boolean to move up a directory

    if (args[1] == NULL)
    {
        //cd command alone
        chdir(getenv("HOME"));
    }
    else if (strcmp(args[1], "..") == 0)
    {
        //moving up a path "cd .."
        if (getcwd(buff, PATH_MAX) != NULL)
        {
            //get the location of the last "/"
            upDir = prevDirPos(buff);
            //protect the root directory
            if(upDir > 0)
            {
                //replace it with null to set path up one folder
                buff[upDir] = '\0';
                //change directory
                chdir(buff);
                clearBuff(buff);
            }
            else if (upDir == 0)
            {
                //if upDir is 0, then change to root directory
                chdir("/");
            }
            else
            {
                //let user know they cannot move higher
                printf("In root directory, cannot move higher");
            }
        }
    }
    else
    {
        //moving down a folder (eg "cd cs344")
        if (getcwd(buff, PATH_MAX) != NULL)
        {
            //buff has the current working directory
            //append a "/"
            strcat(buff, "/");
            //append the folder name
            strcat(buff, args[1]);
            //change directory
            chdir(buff);
            clearBuff(buff);
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

// Entry: A string of size (PATH_MAX + 1)
// Exit: the position of the last "/" in the string
//          -1 if not found
int prevDirPos(char buff[PATH_MAX + 1])
{
    int i = strlen(buff);
    for(; i >= 0; i--)
    {
        if(buff[i] == '/')
        {
            return i;
        }
    }
    return -1;
}

//*****************************************************************************
//                              CLEANUP FUNCTIONS
//*****************************************************************************

// Entry:
// Exit:
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

// Entry:
// Exit:
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
