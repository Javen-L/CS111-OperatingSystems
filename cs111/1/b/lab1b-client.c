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
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mcrypt.h>


void error(char *msg)
{
    perror(msg);
    exit(0);
}

int logfd;

//global variables for mandatory --port option
int portFlag = 0;
char* portString;

//global variables for --log option
int logFlag = 0;
char* logFile;

//global variables for --encrypt option
int encryptFlag = 0;
char* encryptFile;
char* key;
char* iv;
MCRYPT cryptfd, decryptfd;

int child;
int exitReady = 0;

char rn[2] = {'\r', '\n'};


//used GNU Noncanonical Mode Example as code skeleton. Referenced in README
/* Use this variable to remember original terminal attributes. */

struct termios saved_attributes;

void reset_input_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode (void)
{
  struct termios newmode;

  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a terminal.\n");
      exit (EXIT_FAILURE);
    }
  
  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &newmode);
  newmode.c_iflag = ISTRIP;
  newmode.c_oflag = 0;
  newmode.c_lflag = 0;
  if(tcsetattr (STDIN_FILENO, TCSANOW, &newmode) == -1) {
    fprintf(stderr, "Error %s", strerror(errno));
  } 
}

int main (int argc, char **argv)
{

  //code for getting the shell option
  char c;
  static struct option long_options[] =
  {
      {"port",   1, 0, 'p'},
      {"encrypt", 1, NULL, 'e'},
      {"log", 1, NULL, 'l'},
      {0, 0, 0, 0}
  };

  while(1) {
    c = getopt_long(argc, argv, "p:", long_options, 0);
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
      case 'l':
        logFlag = 1;
        logFile = optarg;
        break;
        
      default:
        fprintf(stderr, "Correct usage: ./lab1b-client --port [--encrypt]\n");
        exit(1);
    }
   }

  if(!portFlag) {
    fprintf(stderr, "Error: port required\n");
    fprintf(stderr, "Correct usage: ./lab1b-client --port [--encrypt]\n");
    exit(1);
  }
  
  char buf[256];
  char ebuf[256];

  if(logFlag) {
    logfd = creat(logFile, 0666);
    if(logfd < 0) {
      fprintf(stderr, "Error calling creat: %s\n", strerror(errno));
      exit(1);
    }
  }
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
      error("Error opening dencrypt module");
    

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
  set_input_mode ();

  int sockfd, portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  portno = atoi(portString);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
        error("ERROR opening socket");
  server = gethostbyname("localhost");
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
  bzero(buf,256);
  
  struct pollfd pollfds[] = {
   {STDIN_FILENO, POLLIN, 0},
   {sockfd, POLLIN, 0}
  };


  int pollRet;
  while(1) {
    pollRet = poll(pollfds, 2,-1);
     
   if(pollRet < 0) {
      fprintf(stderr, "Error with poll: %s\n", strerror(errno));
      exit(1);
    }
    if(pollRet > 0) {
      //if we have input waiting from the keyboard
      if(pollfds[0].revents & POLLIN) {
        int i, rret, wret;
        rret = read(STDIN_FILENO, &buf, 256);
        if(rret < 0) {
          fprintf(stderr, "Error with read: %s\n", strerror(errno));
          exit(1);
        }
	
	bcopy(buf, ebuf, rret);


        //process input
        for(i = 0; i < rret; i++) {
          char curr = buf[i];
          if(curr == '\r' || curr == '\n') 
            write(STDOUT_FILENO, rn, 2);
          if(curr == 0x04) {
            if(write(STDOUT_FILENO, "^D", 2) < 0) {
              fprintf(stderr, "Write failed: %s\n", strerror(errno));
              exit(1);
            }
          }
          if(curr == 0x03) {
            if(write(STDOUT_FILENO, "^C",2) < 0) {
              fprintf(stderr, "Write failed: %s\n", strerror(errno));
              exit(1);
            }
          }
          else             
            write(STDOUT_FILENO, &curr, 1);          
        }

	//we want to encrypt the data BEFORE sending it to the server
	if(encryptFlag) 
	  mcrypt_generic(cryptfd, ebuf, 1);
	
	//send data to server, log if needed
        wret = write(sockfd, ebuf, rret);
        if(logFlag) {
          char temp[256];
          sprintf(temp, "SENT %d bytes: ", wret);
          write(logfd, temp, strlen(temp));
          write(logfd, ebuf, wret);
          write(logfd, "\n", 1);
        }
      }
      
      //if input is waiting from the socket
      if(pollfds[1].revents & POLLIN) {
        int rret;
        char cbuf[256];
	char dbuf[256];
        rret = read(sockfd, cbuf, 256);
	if(rret < 0) {
          fprintf(stderr, "Error with read: %s\n", strerror(errno));
          exit(1);
        }

	if(rret == 0) {
          fprintf(stderr, "Read 0 bytes from socket, socket closed.\r\n");
          exit(0);
        }

	bcopy(cbuf, dbuf, rret);
	if(logFlag) {
          char temp[256];
          sprintf(temp, "RECEIVED %d bytes: ", rret);
          write(logfd, temp, strlen(temp));
          write(logfd, cbuf, rret);
          write(logfd, "\n", 1);
        }
	
	if(encryptFlag)
	  mdecrypt_generic(decryptfd, dbuf, rret);

        if(write(STDOUT_FILENO, dbuf, rret) < 0) {
          fprintf(stderr, "Write failed inside of client: %s\n", strerror(errno));
          exit(1);
        }
      }
      
      /*
      if(pollfds[0].revents & (POLLHUP | POLLERR)) 
        exitReady = 1;
      */
      if(pollfds[1].revents & (POLLHUP | POLLERR)){
        if(pollfds[1].revents == POLLHUP)
          fprintf(stderr, "POLLHUP: %s\n", strerror(errno));
        if(pollfds[1].revents == POLLERR)
          fprintf(stderr, "POLLERR: %s\n", strerror(errno));
        exitReady = 1;
      }
    }
    if(exitReady)
      break;      
  }
  exit(0);
}
  
  
