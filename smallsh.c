#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER 2048 
#define ARGSIZE 512 

int minStrLen(char *string1, char *string2)
{
    int a = 0;
    int b = 0;

    a = strlen(string1);
    b = strlen(string2);

    if (a < b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

int shOpCommands(char args[ARGSIZE][256])
{
    
}

//referenced http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
//to get started
int shExe(char args[ARGSIZE][256])
{
    int size = 0;
    int pos = 0;

    while (args[pos][0] != '\0')
    {

        if (strncmp(args[pos], "exit", minStrLen(args[pos], "exit")) == 0)
        {
            exit(0);
        }
        else if (strncmp(args[pos], "cd", minStrLen(args[pos], "cd")) == 0)
        {
            printf("CD\n");
        }
        else if (strncmp(args[pos], "status", minStrLen(args[pos], "status")) == 0)
        {
            printf("STATUS\n");
        }
        else if (strncmp(args[pos], "#", minStrLen(args[pos], "#")) == 0)
        {
            printf("comment char, BREAK\n");
            //break;
        }
        else if (strncmp(args[pos], "<", minStrLen(args[pos], "<")) == 0)
        {
            printf("INPUT\n");
        }
        else if (strncmp(args[pos], ">", minStrLen(args[pos], ">")) == 0)
        {
            printf("OUTPUT\n");
        }
        else if (strncmp(args[pos], "&", minStrLen(args[pos], "&")) == 0)
        {
            printf("BACKGROUND?\n");
        }
        else
        {
            printf("%s\n", args[pos]);
        }
        pos++;
    }

    return 1;
}

void shSplit(char *line, char args[ARGSIZE][256])
{
    //container for the arguments
    // TODO: change 256 to char * and malloc if needed
    char copy[BUFFER];
    char *token;
    const char delimiters[] = " ";
    int pos = 0;

    strncpy(copy, line, BUFFER);
    copy[BUFFER - 1] = '\0';

    token = strtok(copy, delimiters);
    while (token != NULL)
    {
        if (pos >= 512) exit(1); // too many arguments
        strncpy(args[pos], token, strlen(token));
        token = strtok(NULL, delimiters);
        pos++;
    }
    
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

//safeStringCopy?

//referenced http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
//to get started
void shLoop()
{
    char *line;
    char args[ARGSIZE][256];
    int status = 0;
    int i, j;

    do {
        //initialize or clear the args
        for (i = 0; i < ARGSIZE; i++)
        {
            for (j = 0; j < 256; j++)
            {
                args[i][j] = '\0';
            }
        }
        //print the prompt, run the loop
        printf(": ");
        line = shRead();
        shSplit(line, args);
        status = shExe(args);
        //need to free line because we allocated memory in shRead
        free(line);
    } while (status);
}

int main(int argc, char ** argv)
{
    shLoop();
    return 0;
}