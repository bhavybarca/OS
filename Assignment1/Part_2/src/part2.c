//BHAVY SINGH 170212

/*
 2.1)   make a pipe fd
	do a fork
	in child take input from argv and output to pipe fd 
	in parent take input from pipe fd and output to stdout
 2.2)   make a pipe fd1
	do a fork
	we are in tee, in child of fork1
		make a pipe fd2
		do a fork2
		we are in grep , in child of fork2
		in child take input from argv and output to pipe fd1 	
		
		otherwise we are in tee , in parent of fork2
		set both the inputs and outputs of pipe fd1 and fd2
		implement tee by doing a write in the argrument file and stdout		
		
	otherwise we are in our external command, in parent of fork1
		input from pipe fd1 and output to stdout
*/
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


void part1( char *argv[]){
	int fd[2];
	pipe(fd);
	pid_t pid = fork();
	if(pid == 0 ){    //child process
		close(fd[0]);  // closing input side of pipe
		dup2(fd[1],1);   // sending to  output side of pipe
		execlp("grep", "grep", "-rF", argv[2], argv[3], NULL);
		exit(1);
	}
	else{			//parent process
		close(fd[1]);   // closing the output side of pipe
		dup2(fd[0],0);   // sending to input side of pipe
		execlp("wc", "wc", "-l", NULL);
		exit(2);
	}
	

}
void part2( int argc, char *argv[]){
	int fd1[2]; // pipe1
	int fd2[2]; // pipe2
	pipe(fd1);
	pid_t pid1 = fork();
	if(pid1 == 0 ){
		pipe(fd2);
		pid_t pid2 = fork();
		if(pid2 == 0 ){
			
			dup2(fd2[1],1);// sending to output side of pipe2
			close(fd2[0]);// closing the input side of pipe2
			close(fd2[1]);// closing the output side of pipe2
			execlp("grep", "grep", "-rF", argv[2], argv[3], NULL);
			exit(1);
		}
		else{
			
			dup2(fd2[0],0);// sending to input side of pipe2
			dup2(fd1[1],1);// sending to output side of pipe1
			close(fd2[0]);// closing the input side of pipe2
			close(fd2[1]);// closing the output side of pipe1
			int o_fd = open(argv[4], O_RDWR | O_CREAT | O_TRUNC , 0666);
			    char buffer[1024];			//writing in a buffer to increase speed
			    int nbytes;
			    while((nbytes = read(0, buffer, 1024)) > 0) {
				if(write(o_fd, buffer, nbytes) != nbytes) { //writing to the arguement file
				    perror("error writing to file");
				    _exit(1);
				}

				if(write(1, buffer, nbytes) != nbytes) { //writing to the pipe2
				    perror("error writing to pipe fd2");
				    _exit(1);
				}
			    }
			exit(2);	
		}
		exit(3);
	}
	else{
		dup2(fd1[0],0); // sending to input side of pipe1
		close(fd1[0]);// closing the input side of pipe1
		close(fd1[1]);// closing the output side of pipe1
    		execvp(argv[5], argv+5); //
		exit(4);
	}
	
}

int main(int argc,char *argv[])
{
	if(argv[1][0]=='@'){
  		part1(argv);  // @ case
	}
	else if(argv[1][0]=='$'){
		part2(argc,argv);  // $ case
	}
	else{
		printf("incorrect input"); // error handling	
	}
	return 0;
}
















