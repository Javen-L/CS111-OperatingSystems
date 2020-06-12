#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mcrypt.h>
#include <fcntl.h>
#include <sys/stat.h>

void error(char *msg) {
  perror(msg);
  exit(1);
}

int toShell[2];
int fromShell[2];
int sockfd, newsockfd;

int child;
int pipeNotClosed = 1, exitReady = 0;

char rn[2] = {'\r', '\n'};

char* portString;
int portFlag = 0;

//global variables for --encrypt option                                                                                    
int encryptFlag = 0;
char* encryptFile;
char* key;
char* iv;
MCRYPT cryptfd, decryptfd;



struct termios saved_attributes;

void signalHandler(int sig) {
  if(sig == SIGPIPE) {
    exit(0);
  }  
}

void harvest() {
  int status;
  if(waitpid(child, &status, 0) < 0)
    error("Error waiting for child");
  int sig = status & 0x7f;
  int stat = WEXITSTATUS(status);
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", sig, stat);
  close(sockfd);
  if(encryptFlag) {
    free(iv);
    free(key);
    mcrypt_generic_deinit(cryptfd);
    mcrypt_module_close(cryptfd);
    mcrypt_generic_deinit(decryptfd);
    mcrypt_module_close(decryptfd);
  }
  close(newsockfd);
}


int main (int argc, char **argv)
{

  //code for getting the shell option
  char c;
  char* bash = "/bin/bash";
  static struct option long_options[] =
  {
      {"port",   1, 0, 'p'},
      {"encrypt", 1, 0, 'e'},
      {0, 0, 0, 0}
  };

  while(1) {
    c = getopt_long(argc, argv, "p:e:", long_options, 0);
    if(c == -1)
      break;
    
    switch(c){
      case 'p':
        portFlag = 1;
        portString = optarg;
        break;
      case 'e':
	encryptFlag = 1;
	encryptFile = optarg;        
	break;
      default:
        fprintf(stderr, "Correct usage: ./lab1b-server --port [--encrypt]\n");
        exit(1);
    }
   }

  if(!portFlag) {
    fprintf(stderr, "Error: port required\n");
    fprintf(stderr, "Correct usage: ./lab1b-server --port [--encrypt]\n");
    exit(1);
  }

  atexit(harvest);
  
  signal(SIGPIPE, signalHandler);

  //create the server socket, bind port, listen for  client, and then accept connection
  int portno;
  socklen_t clilen;
  char buf[256];
  struct sockaddr_in serv_addr, cli_addr;
  portno = atoi(portString);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr)); //zero out serv_addr
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");
  listen(sockfd,5);
  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0) 
    error("ERROR on accept");
  bzero(buf, 256);
  
  //create a pipe to the shell and a pipe from the shell
  if(pipe(toShell) == -1) {
    fprintf(stderr, "Error creating pipe to shell:%s\n", strerror(errno));
    exit(1);
  }
  if(pipe(fromShell) == -1) {
    fprintf(stderr, "Error creating pipe from shell:%s\n", strerror(errno));
    exit(1);
  }
  
  child = fork();
  if(child < 0) {
    fprintf(stderr, "Error calling fork(): %s\n", strerror(errno));
    exit(1);
  }
  
  //child process
  if(child == 0) {
    //close the write of pipe to shell, read from shell, and stdin
    close(toShell[1]);
    close(fromShell[0]);
    close(STDIN_FILENO);
    
    //now we can duplicate the pipe to fd 0
    dup(toShell[0]);
    close(toShell[0]);
    
    //similarly, reroute stdout and stderr using close and dup
    close(STDOUT_FILENO);
    dup(fromShell[1]); //this replaces stdout
    close(STDERR_FILENO);
    dup(fromShell[1]); //this replaces stderr
    close(fromShell[1]);
    
    //use execl to run bash
    if(execl(bash, bash, (char *)NULL) == -1) {
      fprintf(stderr, "Error running execl: %s\n", strerror(errno));
      exit(1);
    }
  }
  
  
  //parent process
  else {
    if(close(toShell[0]) < 0) {
      fprintf(stderr, "Error closing pipe toShell: %s\n", strerror(errno));
      exit(1);
    }
    if(close(fromShell[1]) < 0) {
      fprintf(stderr, "Error closing pipe fromShell:%s\n", strerror(errno));
      exit(1);
    }
    
    if(encryptFlag) { 
      int keyfd;
      int keySize;
      struct stat keyfileStatus;
      int ivSize;
      if(encryptFlag) {
	keyfd = open(encryptFile, O_RDONLY);
	if(keyfd < 0)
	  error("error opening key file");
	if(fstat(keyfd, &keyfileStatus))
	  error("Error, couldn't get key file status from fstat.");

	keySize = keyfileStatus.st_size;
	key = malloc(keySize);
	if(read(keyfd, key, keySize) < 0)
	  error("Error reading keyfile");
	cryptfd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
	if (cryptfd == MCRYPT_FAILED)
	  error("Error opening encrypt module");
	decryptfd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
	if (decryptfd == MCRYPT_FAILED)
	  error("Error opening decrypt module");


	ivSize = mcrypt_enc_get_iv_size(cryptfd);
	iv = (char*) malloc(ivSize);
	int i;
	for (i = 0; i < ivSize; i++)
	  iv[i] = '\0';

	i = mcrypt_generic_init(cryptfd, key, keySize, iv);
	if (i < 0)
	  error("Error: couldn't initialize encrypt module");
	i = mcrypt_generic_init(decryptfd, key, keySize, iv);
	if (i < 0)
	  error("Error: couldn't initialize decrypt module");
      }
    }
    
    struct pollfd pollfds[] = {
      {newsockfd, POLLIN, 0},
      {fromShell[0], POLLIN, 0}
    };
    
    int pollRet;
    while(1) {
      pollRet = poll(pollfds, 2,-1);
      
      if(pollRet < 0) {
        fprintf(stderr, "Error with poll: %s\n", strerror(errno));
        exit(1);
      }
      if(pollRet > 0) {
        //here we handle the stdin from the client to the shell
        if(pollfds[0].revents & POLLIN) {
          //read input
          int i, rret;
          rret = read(newsockfd, &buf, 256);
          if(rret < 0) {
            fprintf(stderr, "Error with read: %s\n", strerror(errno));
            exit(1);
          }
	 
	  //deal with decryption for inputs to the shell
	  if(encryptFlag) 
	    mdecrypt_generic(decryptfd, buf, rret);
	  
	
          for(i = 0; i < rret; i++) {
            char curr = buf[i];
            //0x03 case
            if(curr == 0x03) {
              if(kill(child, SIGINT) < 0) {
                fprintf(stderr, "Error, couldn't kill child: %s\n",strerror(errno));
                exit(1);
              }
              if(pipeNotClosed) {
                close(toShell[1]);
                pipeNotClosed = 0;
              }
            }
            //EOF case
            else if(curr == 0x04) {            
              if(pipeNotClosed) {
                close(toShell[1]);
                pipeNotClosed = 0;
              }
            }
            //carriage return and linefeed
            else if(curr == '\r') 
              write(toShell[1], "\n", 1);            
            else 
              write(toShell[1], &curr, 1);
          }          
        }
             
        //here we deal with the input from the shell to the client display
        if(pollfds[1].revents & POLLIN) {
          int i ,rret;
          char cbuf[256];
	  char temp[2];
          rret = read(fromShell[0], cbuf, 256);
          if(rret < 0) {
            fprintf(stderr, "Error with read: %s\n", strerror(errno));
            exit(1);
          }
          for(i = 0; i < rret; i++) {
            char curr = cbuf[i];
            //0x04 case
            if(curr == 0x04) 
	      exit(0);

            //rn case
            else if(curr == '\n') {
	      char temp[2];
	      temp[0] = '\r';
	      temp[1] = '\n';
	      if(encryptFlag) 
		mcrypt_generic(cryptfd, temp, 2);	      
	      if(write(newsockfd, temp, 2) < 0) {
		fprintf(stderr, "Error with write: %s\n", strerror(errno));
		exit(1);
	      }
            }
            
            else {
	      temp[0] = curr;
	      if(encryptFlag)
		mcrypt_generic(cryptfd, temp, 1);
	      if(write(newsockfd, &temp, 1) < 0) {
		fprintf(stderr, "Error with write: %s\n", strerror(errno));
		exit(1);
	      }
	    }
          }
        }
        
        //error from client to shell
        if(pollfds[0].revents & (POLLHUP | POLLERR)) {
          exitReady = 1;
          if(pipeNotClosed){
            close(toShell[1]);
            pipeNotClosed = 0;
          }
        }
        if(pollfds[1].revents & (POLLHUP | POLLERR)) {
          exit(0);
        }                  
      }  
      if(exitReady)
        break;      
    }
    exit(0);    
  }                
  return 0;
}

