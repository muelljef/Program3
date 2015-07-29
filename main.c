#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

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

void shExe(char **args)
{
    pid_t spawnpid, exitpid;
    int exitMethod, pos;

    spawnpid = fork();
    if (spawnpid == 0)
    {
                //check for IO redirection, bg
        pos = 0;
        while(args[pos] != NULL)
        {
            //printf("%s\n", args[pos]);
            if(strcmp(args[pos], "<") == 0)
            {
                //TODO
            }
            else if (strcmp(args[pos], ">") == 0)
            {
                //TODO
            }
            else if (strcmp(args[pos], "&") == 0)
            {
                //TODO
            }
            pos++;
        }
        //printf("child: executing\n");
        execvp(args[0], args);
        printf("Exec error\n");
        exit(1);
    }
    else if (spawnpid > 0)
    {
        //printf("parent: waiting\n");
        exitpid = wait(&exitMethod);
        if (exitpid == -1)
        {
            printf("wait failed\n");
            exit(1);
        }
        if (WIFEXITED(exitMethod))
        {
            //printf("parent: child exited normally[%d]\n", exitMethod);
            //exitMethod = WEXITSTATUS(exitMethod);
            //printf("exit status was %d\n", exitMethod);
        }
        else
        {
            printf("Child terminated by signal\n");
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
    //int status;
    char buff[PATH_MAX + 1];
    int shLoop = 1;
    int upDir;

    do {
        //assistance for allocating the args array dynamically taken from the following site
        //http://stackoverflow.com/questions/7652293/how-do-i-dynamically-allocate-an-array-of-strings-in-c
        args = (char **) malloc(sizeof(char *) * ARGSIZE);
        if(args == NULL) exit(1);   //memory did not allocate properly

        //printing the prompt, reading input, and tokenizing input
        printf(": ");
        line = shRead();
        shSplit(line, args);

        //Handling blank input
        if (args[0] == NULL)
        {
            printf("args[0] is NULL\n");
            freeArgs(args); //TODO: is this needed?
            continue;
        }

        //Handle comments, builtins, then execute if none of those
        if (strcmp(args[0], "#") == 0)
        {
            //Handling comments
            printf("comment\n");
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
            printf("status goes here\n");
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
            shExe(args);
        }
        freeArgs(args);
    } while(shLoop);

    return 0;
}
