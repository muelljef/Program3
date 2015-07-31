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

void shSplit(char *line, char **args)
{
    //container for the arguments
    char *token;
    int i;
    const char delimiters[] = " ";
    int pos = 0;

    //initialize the strings to NULL
    // TODO do I need this?
    for(i = 0; i < ARGSIZE; i++)
    {
        args[i] = NULL;
    }

    //tokenize all the arguments from the line
    token = strtok(line, delimiters);
    while (token != NULL)
    {
        //TODO: does this need to be something other than exit?
        if (pos >= ARGSIZE) exit(1);
        //TODO: make sure to free this string properly
        args[pos] = strdup(token);
        if(args[pos] == NULL) exit(1);   //string did not allocate properly
        //take the next string
        token = strtok(NULL, delimiters);
        //increment the count
        pos++;
    }

    return;
}

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

char* shRead()
{
    int c;
    int position = 0;
    char *buffer = malloc(sizeof(char) * BUFFER);

    if (!buffer)
    {
        fprintf(stderr, "smallsh: allocation error\n");
        exit(1);
    }

    while (1)
    {
        c = getchar();

        if (c == EOF || c == '\n')
        {
            buffer[position] = '\0';
            return buffer;
        }
        else
        {
            buffer[position] = c;
        }
        position++;

        //If we have exceeded the buffer exit
        if (position >= BUFFER)
        {
            printf("Exceeded buffer\n");
            return NULL;
        }
    }
}

void shExe(char **args, int *fgStat, struct sigaction *fgChild)
{
    pid_t spawnpid, exitpid, bgPid;
    int pos, fd, fd2, out, in, bgStat;
    int bg = 0;

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

    pos = 0;
    while(args[pos] != NULL)
    {
        if(strcmp(args[pos], "&") == 0)
        {
            bg = 1;
        }
        pos++;
    }

    spawnpid = fork();
    if (spawnpid == 0)
    {
        //CHILD
        sigaction(SIGINT, fgChild, NULL);
        //check for IO redirection, bg
        pos = 0;
        //initialize out and in to an invalid number
        out = -5;
        in = -5;

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
                //background process
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
        //printf("child: executing\n");
        execvp(args[0], args);
        perror(args[0]);
        exit(1);
    }
    else if (spawnpid > 0)
    {
        //PARENT
        if(bg == 1)
        {
            printf("background pid is %d\n", (int)spawnpid);
            //background process
        }
        else
        {
            if (strcmp(args[0], "kill") == 0)
            {
                exitpid = waitpid(-1, fgStat, 0);
                //check the exit status
                if (exitpid == -1)
                {
                    perror("fg kill wait 1");
                }
                else if(exitpid > 0)
                {
                    if (WIFSIGNALED(*fgStat))
                    {
                        printf("background pid %d is done: terminated by signal %d\n", (int)exitpid, WTERMSIG(*fgStat));
                    }
                }
                exitpid = waitpid(spawnpid, fgStat, 0);
                //check the exit status
                if (exitpid == -1)
                {
                    perror("fg kill wait 2");
                }
                else if(exitpid > 0)
                {
                    if (WIFSIGNALED(*fgStat))
                    {
                        printf("fg terminated by signal %d [pid=%d]\n", WTERMSIG(*fgStat), (int)exitpid);
                    }
                }
            }
            else
            {
                exitpid = waitpid(-1, fgStat, 0);
                //check the exit status
                if (exitpid == -1)
                {
                    perror("fg single wait");
                }
                else if(exitpid > 0)
                {
                /*
                    if (WIFEXITED(*fgStat))
                    {
                        printf("fg pid %d is done: exit status %d\n", (int)exitpid, WEXITSTATUS(*fgStat));
                    }
                */
                    if (WIFSIGNALED(*fgStat))
                    {
                        printf("fg terminated by signal %d [pid=%d]\n", WTERMSIG(*fgStat), (int)exitpid);
                    }
                }
            }
        }
    }
    else
    {
        printf("fork failed\n");
    }
}

void clearBuff(char buff[PATH_MAX + 1])
{
    int i = 0;
    for(;i < PATH_MAX + 1; i++)
    {
        buff[i] = '\0';
    }
}

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
    return 0;
}

int main()
{
    char *line;
    char **args;
    char buff[PATH_MAX + 1];
    int shLoop = 1;
    int upDir;
    pid_t bgPid;
    int bgStat;
    int fgStat = 0;


    struct sigaction act, fgChild;
    act.sa_handler = SIG_IGN;

    sigaction(SIGINT, &act, &fgChild);

    do {
        //assistance for allocating the args array dynamically taken from the following site
        //http://stackoverflow.com/questions/7652293/how-do-i-dynamically-allocate-an-array-of-strings-in-c
        args = (char **) malloc(sizeof(char *) * ARGSIZE);
        if(args == NULL) exit(1);   //memory did not allocate properly

        fflush(stdout);

        //Check for background process completion
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

        //printing the prompt, reading input, and tokenizing input
        fflush(stdout);
        printf(": ");
        fflush(stdout);
        line = shRead();
        shSplit(line, args);

        //Handling blank input
        if (args[0] == NULL)
        {
            freeArgs(args); //TODO: is this needed?
            continue;
        }

        //Handle comments, builtins, then execute if none of those
        if (strcmp(args[0], "#") == 0)
        {
            //Handling comments
            //this is not necessary as all code is in if else statements (shExe) will
            //not be called because it is in the else stmt of this block
            freeArgs(args);
            continue;
        }
        else if(strcmp(args[0], "exit") == 0)
        {
            //TODO work out how the loop should work with exit
            //shLoop = 0;
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
            if (args[1] == NULL)
            {
                //this needs to do what cd does
                chdir(getenv("HOME"));
            }
            else if (strcmp(args[1], "..") == 0)
            {
                //this needs to move up a path
                if (getcwd(buff, PATH_MAX) != NULL)
                {
                    upDir = prevDirPos(buff);
                    buff[upDir] = '\0';
                    chdir(buff);
                    clearBuff(buff);
                }
            }
            else
            {
                //chdir(args[1]);
                if (getcwd(buff, PATH_MAX) != NULL)
                {
                    strcat(buff, "/");
                    strcat(buff, args[1]);
                    chdir(buff);
                    clearBuff(buff);
                }
            }
        }
        else
        {
            shExe(args, &fgStat, &fgChild);
        }
        freeArgs(args);
    } while(shLoop);

    return 0;
}
