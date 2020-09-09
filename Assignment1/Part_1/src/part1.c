//BHAVY SINGH 170212
/*
    if the file is a regular file then call my grep function on it.
    if it is a directory then recurse on it.
    my grep takes a path of a file and a keyword to be searched reads 1 character at a time until '/n' is found
    then stores it in a line and checks if the keyword is present by using strstr.
    if found then print the full path.
*/

#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include<string.h>
#include <fcntl.h>
#include <dirent.h>

int isreg(const char *path) // checks for regular file
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

void mygrep(char *argv[], char *path, int flag)  // grep implementation
{
    int fd,r;
    long long int j=0;    
    char temp;
    char *line;
    line = (char *)malloc(sizeof(char));  // no limit on number of lines in the file
    if((fd=open(path,O_RDONLY)) != -1)  //opening in read-only mode
    {
        while((r=read(fd,&temp,sizeof(char)))!= 0)  //reading each character till file ends
        {
            if(temp!='\n')  // end of one line
            {
                line[j]=temp;
                j++;
                line = (char *)realloc(line,sizeof(char)*(j+1)); //reallocating line size to exactly how much memory is required
            }
            else
            {
                line[j]='\0';
                if(strstr(line,argv[1])!=NULL){ // substring check
                    if(flag != 0){
                        printf("%s:", path); // printing path
                    }
                    printf("%s\n",line); //printing line
                }
                memset(line,0,sizeof(line)); // filling with 0s
                j=0; // setting index to 0 for next line
            }

        }
    }   
}
void Rec(char *basePath,char *argv[])
{
    char path[100000]; 
    struct dirent *dp;
    DIR *dir = opendir(basePath);
    if (!dir)  
        return;
    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) // removing '.' and '..' directories to avoid infintie recursion
        {
            strcpy(path, basePath);
            if(path[strlen(path)-1]!= '/') // checking for '/' at the end of the path
                strcat(path, "/");
            strcat(path, dp->d_name);

            if ((isreg(path))){   // if file is found run my grep
                mygrep(argv,path,1);  
            }
            else{  // else recurse on a subdirectory
                Rec(path,argv); 
            }
        }
    }

    closedir(dir);  //closeing directory when work is done
}


int main(int argc,char *argv[])
{
    struct stat buf;
    if(argc==3)
    {
        if(stat(argv[2],&buf)==0)
            if ((isreg(argv[2]))){
                mygrep(argv,argv[2],0);// if my grep is used on a file then flag is set to 0 so it doesn't print the path
            }
            else{
                Rec(argv[2],argv); // my grep recursive call
            }
        else 
        {
            perror("stat()");
            exit(1);
        }
    }
    return 0;
}
