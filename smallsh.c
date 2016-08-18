/************************************************************************
 *smallsh.c
 *Author: Joseph Fuerst
 *Description: This is a basic UNIX shell that allows a user to run programs, redirect input and output,
 * change directory, and other useful functions.
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

int status = 1; //contains the status of the last process
int bgp = 0; //indicates whether or not the next process will be a background process
int bgps[200]; //An array holding the pids of all running background processes
int numBgps = 0; //the number of running background processes
int statusCode; // The status code of the last process


void checkProcs(); //declaration of checkProcs function

/************************************************************************
 *ReadLine
 *Description: displays a prompt to the user and records what they want to do, splits the line into
 *parts, such as the command, arguments, what files to redirect into etc.
 ************************************************************************/
char** readLine() 
{
	char** inpArray = malloc(sizeof(char*) * 512); //array containing all arguments
	char* input = malloc(sizeof(char*) * 2048); //string containing the input line
	char* curWord = NULL; //current work being tokenized

	int size = 500;
	checkProcs(); //check for completed background processes before prompting the user
	printf(": "); //the prompt
	fgets(input, size, stdin); //get a line from the user
	curWord = strtok(input, "\n"); 
	if (curWord == NULL) //if nothing is entered, than there is nothing to put in the array.
	{
		inpArray[0] = NULL;
		return inpArray;
	}
	//get the next word in the line
	curWord = strtok(curWord, " ");
	int index = 0;
	while (curWord != NULL) //continue getting the words and storing them in inpArray until the end of the line
	{
		inpArray[index] = curWord;
		index++;
		curWord = strtok(NULL, " ");
	}
	index--; //move index back one to check for the background process indicator, because right now index points to NULL, not the last input word
	if (strcmp(inpArray[index], "&") == 0)
	{
		bgp = 1; //indicate that this will be a background process
		inpArray[index] = NULL; //delete this character from the array so we don't have to deal with it anymore
	}
	else
	{
		bgp = 0; //its not a background process
	}
	return inpArray; //return the array so it may be used elsewhere.
	
}
/************************************************************************
 *procExe
 *Description: Forks and executes the process, checks for redirection, and handles return values
 ************************************************************************/
int procExe(char** inpt)
{
	int status; //status of process
	char* command = inpt[0]; //the actual command to be executed
	int fd; //file descriptors to work with redirecting input and output
	int ifd;
	int ofd;
	int cifd;
	int cofd;
	int outCheck = 0; //boolean check for output redirection
	int inCheck = 0; //boolean check for input redirection
	int ioError = 0; //indicates an redirection error
	
	//Here are the built-in commands exit, cd, status, and the comment #
	if (strcmp(command, "exit") == 0)
	{
		return 1; //returns a value to not continue
	}
	else if (strcmp(command, "cd") == 0) //change directory
	{
		if (inpt[1] != NULL) //if the user indicated which directory to change into
		{
			//printf("Going to new directory: %s\n", inpt[1]);
			fflush(stdout);
			char* newPath = inpt[1];
			chdir(newPath);
			return 0;
			
		}
		else //the user did not indicate a directory, so go to the home directory
		{
			char* directory;
			directory = getenv("HOME");
			chdir(directory);
		}
	}
	else if (strcmp(command, "status") == 0) //the user requests the status code of the last program
	{
		printf("Status of last program: %i\n", statusCode); //print the status code
		fflush(stdout);
	}
	else if (strcmp(command, "#") == 0) //indicates the user is making a comment
	{
		return 0; //do nothing
	}
	
	//Otherwise the user entered a command that is not built in. Process this like normal.
	else
	{
		
		//go through the array of arguments and look for the redirection operators < and >
		int i = 0;
		while (inpt[i] != NULL)
		{
			
			if (strcmp(inpt[i], "<") == 0)
			{
				inCheck = 1; //indicate that input redirection is taking place
				
				if (access(inpt[i+1], R_OK) == -1) //check that the file is valid
				{
					
					printf("Cannot open input file.\n"); //if not, error
					fflush(stdout); 
					ioError = 1; //indicate that the command won't be successful
					
				}
				else  //the file is valid
				{
					ifd = open(inpt[i+1], O_RDONLY, 0); //open the file
					if (ifd == -1) //check that it can be opened
					{
						printf("Error: cannot open input file\n"); //else, error
						fflush(stdout);
						ioError = 1;
					}
					else //otherwise, delete the file and the operator from the array once its processed
					{
						inpt[i] = NULL; 
						inpt[i+1] = NULL;
					}
					
				}
				i++;
			}
			else if (strcmp(inpt[i], ">") == 0) //check for output redirection
			{
				outCheck = 1; //indicate redirection
				ofd = open(inpt[i+1], O_WRONLY|O_TRUNC|O_CREAT, 0644); //open the file and truncate, or make a new one if none present
				if (ofd == -1)
				{
					printf("Error: cannot open output file\n"); //indicate error if unable to open
					fflush(stdout);
					ioError = 1;
				}
				else //otherwise, set the values of filename and operator to null in teh array.
				{
					inpt[i] = NULL; 
					inpt[i+1] = NULL;
				}
				i++;
			}
			
			i++;
		}
		//printf("\n");
		//fflush(stdout);
		
		//pids of parent and child processes
		pid_t pid;
		pid_t ppid;
		if (inpt[0] == NULL) //there was nothing entered, return without doing anything
		{
			return 0;
		}
		//foreground process was indicated
		if(bgp == 0)
		{
			pid = fork(); //fork the process
			if(pid == 0) //If we're in the child process now
			{
				int i = 1;
				if (ioError == 1) //exit with error if ioError
				{
					//printf("Not executing due to io error.\n");
					fflush(stdout);
					exit(1);
				}
				if (inCheck == 1) //redirect input to the file descriptor indicated earlier
				{
					
					
					dup2(ifd, 0);
					close(ifd);
				}
				if (outCheck == 1) //redirect output to the file descriptor indicated earlier
				{
					
					fflush(stdout);
					cofd = dup2(ofd, 1); 
				}
				
				if (execvp(inpt[0], inpt) == -1) //execute the command. If it fails,exit with error
				{
					perror("Error executing command");
					fflush(stdout);
					exit(1);
				}		
			}
			//parent
			else if(pid > 0) //If we are in the parent
			{
				//wait for the child to finish executing
				waitpid(pid, &status, 0); 
				if(WIFEXITED(status))
				{
					//get the status code of the child process
					statusCode = WEXITSTATUS(status);
					
				}
				
				fflush(stdout);
				
			}
			else
			{
				//error
			}
		}
		//background process
		else if (bgp == 1)
		{
			
			pid = fork(); //fork the process
			if (pid == 0) //if in child process
			{
				int i = 1;
				if (ioError == 1) // indicate io error
				{
					printf("Not executing due to io error.\n");
					exit(1);
				}
				if (inCheck == 1) //set up input redirection
				{
					//printf("input redirected\n");
					fflush(stdout);
					 //cifd = dup2(ifd, 0);
					dup2(ifd, 0);
					close(ifd);
				}
				else //background process cant get input from stdin
				{
					fd = open("/dev/null", O_RDONLY);
					dup2(fd, 0);
					close(fd);
				}
				if (outCheck == 1) //set up output redirection
				{
					//printf("output redirected\n");
					fflush(stdout);
					cofd = dup2(ofd, 1);
				}
				else //process cannot send output to stdout
				{
					fd = open("/dev/null", O_WRONLY);
					dup2(fd, 1);
					close(fd);
				}
				
				if (execvp(inpt[0], inpt) == -1) //execute file, if fail, exit with error
				{
					perror("error executing new file...\n");
					exit(1);
				}
			}
			else //else we are in parent 
			{
				printf("background pid is %d\n", pid); //state the child pid
				fflush(stdout);
				bgps[numBgps] = pid; //add it to the array of active children
				numBgps++; //increase the number
				//wait for it to complete, but do not hang
				do{
					ppid = waitpid(-1, &status, WNOHANG);
				}while(!WIFEXITED(status) && !WIFSIGNALED(status));
				
			}
			//get the status code
			if(WIFEXITED(status)){
				statusCode = WEXITSTATUS(status);
			}
		}
		
		return 0;
	}
}
/************************************************************************
 *deleteInpt
 *Description: deletes the values in inpt then frees it
 ************************************************************************/
void deleteInpt(char** inpt)
{
	int i = 0;
	while (inpt[1] != NULL) //delete each value
	{
		free(inpt[i]);
		i++;
	}
	free(inpt); //free the array
}
/************************************************************************
 *clearInpt
 *Description: clears the input so that a user cna enter another command
 ************************************************************************/
void clearInpt(char** inpt)
{
	int i = 0;
	while (inpt[1] != NULL) //set each one to null again
	{
		inpt[i] = NULL;
		i++;
	}
	
}

void checkProcs() //checks for the status of each process to see if it terminates
{
	int i;
	for(i = 0; i < numBgps; i++) //go through the array of processes for their pids
	{
		if (waitpid(bgps[i], &status, WNOHANG) > 0)
		{
			//if it exited by itself
			if (WIFEXITED(status))
			{
				printf("Process %i has exited\n", bgps[i]);
				fflush(stdout);
				printf("Exit status: %d\n", WEXITSTATUS(status));
				fflush(stdout);
			}
			//this bit of code gave me weird errors and doubled with the one below it, so i've removed it for now. It didn't seem to do too much anyways.
			/*
			if(WIFSIGNALED(status))
			{
				fprintf(stdout, "process %i exited from signal %i\n", bgps[i], WSTOPSIG(status));
				//printf("process %i exited from signal\n", bgps[i]);
				fflush(stdout);
				//printf("Exit status: %d\n", WEXITSTATUS(status));
				//fflush(stdout);
			}
			*/
			//if it was terminated
			if (WTERMSIG(status))
			{
				fprintf(stdout, "process %i terminated from signal %i\n", bgps[i], WTERMSIG(status));
				fflush(stdout);
			}
		}
	}
}

void trap()
{
	//ignore for the parent process, not the children.
}

//runs the program
int main(){
	int cnt = 0; //boolean to indicate whether to continue or exit the shell
	//int process; 
	int status; //status of the last process
	//sigset_t my_sig_set;
	//sigaddset(ny_sig_set, SIGINT);
	struct sigaction saCheck; 
	saCheck.sa_handler = trap; //function that happens instead of killing the shell
	saCheck.sa_flags = 0;
	sigaction(SIGINT, &saCheck, NULL);
	//loop through running commands and getting commands while user does not enter exit
	do{
		char** inpt; //the input array form command and args
		
		checkProcs(); //check the background processes
		
		inpt = readLine(bgp); //read a line from stdin and save it in the array
		if (inpt[0] != NULL)
		{
			cnt = procExe(inpt);
		}
		clearInpt(inpt);
		//delete(inpt);
		
		
	} while(cnt == 0);
	return 0;
	
	
}

