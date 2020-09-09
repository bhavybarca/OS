//BHAVY SINGH 170212
/*
if path given is file then directly print the size
recurse for the directory
while the path given to the recursive function is a file keep adding it to the total size when a path is a directory then make afork and the parent will get the local size of the directory added to the total size and the child will go in recursion with local size initialsed to zero.
at each recursion there will be a counter for depth and only at depth 0 and 1 the output of total size will be printed which gives the size of root and N immediate sub directories.

this can also be done by a recursive fucntion that returns the size of every sub directory without forking and by doing so there will be exactly N forks but that doesn't seem like the approach this questions wanted us to learn so I have made a total of N*M forks where forks are made even at sub-sub directories.    
*/
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/types.h> 
#include <unistd.h>
#include<string.h>
#include <fcntl.h>
#include <dirent.h>
#include<sys/wait.h> 
#include <sys/stat.h>
#include <fcntl.h>

size_t getFilesize(const char* filename) { // returns file size
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;   
}
int isreg(const char *path) // check for regular file
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}


long long int Rec(char* directory,int depth)
{
    long long int total_size=0;
    struct dirent *currentDir;  
    struct stat buf;
    int current; 
    DIR *curdirect = opendir(directory);

    if (curdirect == NULL)
    {
        printf("Error: Could not open directory.\n");
        return 0;
    }
    char pathBuffer[1024];
    while ((currentDir = readdir(curdirect)) != NULL)
    {       
        snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", directory, currentDir->d_name);
        current = stat(pathBuffer,&buf);
        if (currentDir->d_type == DT_DIR && strcmp(currentDir->d_name, ".") != 0 && strcmp(currentDir->d_name, "..") != 0)
        {
            int fd[2];
            pipe(fd);
            pid_t pid = fork();

            if (pid == 0) //child
            {        
                long long int local_size = Rec(pathBuffer,depth+1); //recursively initialize local size for each directory
                close(fd[0]);// closing the input side of pipe
                dup2(fd[1],1);// sending to output side of pipe
                write(fd[1],&local_size,sizeof(local_size));
                exit(0);
            }
            else //parent
            {
                wait(NULL); //wait at end
                close(fd[1]);// closing the output side of pipe
                dup2(fd[0],0);// sending to input side of pipe
                long long int local_size=0 ; // initial value of local size will be 0
                read(fd[0],&local_size,sizeof(local_size));
                total_size+=local_size; // increasing total size by local size 
            }
        }
        else if (strcmp(currentDir->d_name, ".") != 0 && strcmp(currentDir->d_name, "..") != 0)
        {
            total_size += buf.st_size;  //adding the size of non directory files
        }
    }
    closedir(curdirect);
    if(depth==0 || depth ==1 ){     // printing only on recursion depth 1 and 0 to print only the root and immidiate N directories
        int j1=0;
        char dirname[1000000];
        int k=strlen(directory)-1;
        while (directory[k]=='/'){
            k--;
        }
        while(directory[k]!='/' && k>=0){ //error handling for path of directory
            dirname[j1]=directory[k];
            j1++;
            k--;
        }
        dirname[j1]!='\0';
        int k1=0;
        while(dirname[j1-k1-1]!='\0'){
            printf("%c",dirname[j1-k1-1]);
            k1++;
        }
        printf(" %lld\n",total_size);
        fflush(stdout); 
    }
    return total_size;  // recursively returning total size
}

int main(int argc,char *argv[])
{
    struct stat buf;
    if(argc==2)
    {
        if(stat(argv[1],&buf)==0)
            if ((isreg(argv[1]))){
                getFilesize(argv[1]); //printing directly if path is a file
            }
            else{
                Rec(argv[1],0); // recurse if path is a directory
            }
        else 
        {
            perror("stat()");
            exit(1);
        }
    }
    return 0;
}
