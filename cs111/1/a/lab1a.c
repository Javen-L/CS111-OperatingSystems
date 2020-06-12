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

int child;
int pipeNotClosed = 1, exitReady = 0;

char rn[2] = {'\r', '\n'};

int shell = 0;
char* program;

//used GNU Noncanonical Mode Example as code skeleton. Referenced in README
/* Use this variable to remember original terminal attributes. */

struct termios saved_attributes;

void signalHandler(int sig) {
  if(sig == SIGPIPE) {
    exitReady = 1;
  }
}

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
  if(tcsetattr (STDIN_FILENO, TCSAFLUSH, &newmode) == -1) {
    fprintf(stderr, "Error %s", strerror(errno));
  } 
}

int main (int argc, char **argv)
{
  int toShell[2];
  int fromShell[2];

  //code for getting the shell option
  char c;
  char* program;
  static struct option long_options[] =
  {
      {"shell",   1, 0, 's'},
      {0, 0, 0, 0}
  };

  while(1) {
    c = getopt_long(argc, argv, "s:", long_options, 0);
    if(c == -1)
      break;

    switch(c){
      case 's':
        shell = 1;
        program = optarg;
        break;
        
      default:
        fprintf(stderr, "Correct usage: ./lab1a [--shell program]\n");
        exit(1);
    }
   }

  
  char buf[256];

  set_input_mode ();

  //code for if the shell flag is set
  if(shell) {
    //create a pipe to the shell and a pipe from the shell
    if(pipe(toShell) == -1) {
      fprintf(stderr, "Error creating pipe to shell:%s\n", strerror(errno));
      exit(1);
    }
    if(pipe(fromShell) == -1) {
      fprintf(stderr, "Error creating pipe from shell:%s\n", strerror(errno));
      exit(1);
    }

    signal(SIGPIPE, signalHandler);
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

      //use execl to run bash and our program of choice; tried execvp and had issues
      if(execl(program, program, (char *)NULL) == -1) {
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

      struct pollfd pollfds[] = {
        {STDIN_FILENO, POLLIN, 0},
        {fromShell[0], POLLIN, 0}
      };


      while(1) {
        poll(pollfds, 2,-1);

        //here we handle the stdin from the parent
        if(pollfds[0].revents & POLLIN) {
          //read input
          int i, rret;
          rret = read(STDIN_FILENO, &buf, 256);
          if(rret < 0) {
            fprintf(stderr, "Error with read: %s\n", strerror(errno));
            exit(1);
          }

          
          for(i = 0; i < rret; i++) {
            char curr = buf[i];
            //0x03 case
            if(buf[i] == 0x03){
              char prt[] = {'^' , 'C'};
              if(write(STDOUT_FILENO, prt, 2) < 0) {
                fprintf(stderr, "Error, couldn't write: %s\n",strerror(errno));
                exit(1);
              }
              if(kill(child, SIGINT) < 0) {
                fprintf(stderr, "Error, couldn't kill child: %s\n",strerror(errno));
                exit(1);
              }
            }
            //0x04 case
            else if(buf[i] == 0x04) {
              char prt[] = {'^','D'};
              if(write(STDOUT_FILENO, prt, 2) < 0) {
                fprintf(stderr, "Error, couldn't write: %s\n",strerror(errno));
                exit(1);
              }
              close(toShell[1]);
              int foo;
              waitpid(child, &foo, 0);
              fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", WIFSIGNALED(foo), WEXITSTATUS(foo));
              exit(0);
              
            }
            

            //carriage return and linefeed
            else if(curr == '\r' || curr == '\n') {
              write(STDOUT_FILENO, rn, 2);              
              write(toShell[1], rn + 1, 1);
            }
            else {
              write(STDOUT_FILENO, &curr, 1);
              write(toShell[1], &curr, 1);
            }
          }          
        }

        
        //here we deal with the shell input
        if(pollfds[1].revents & POLLIN) {
          int i ,rret;
          char cbuf[256];
          rret = read(fromShell[0], cbuf, 256);
          if(rret < 0) {
            fprintf(stderr, "Error with read: %s\n", strerror(errno));
            exit(1);
          }

          for(i = 0; i < rret; i++) {
            char curr = cbuf[i];
            //0x04 case
            if(curr == 0x04) {
              char prt[] = {'^', 'D'};
              if(write(STDOUT_FILENO, prt, 2) < 0) {
                fprintf(stderr, "Error with write: %s\n", strerror(errno));
                exit(1);
              }
              int foo;
              waitpid(child, &foo, 0);
              fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(foo), WEXITSTATUS(foo));
              exit(0);
              
            }

            //0x03 case
            else if(curr == '\n') {
              if(write(STDOUT_FILENO, rn, 2) < 0) {
                fprintf(stderr, "Error with write: %s\n", strerror(errno));
                exit(1);
              }
            }
            
            else if(write(STDOUT_FILENO, &curr, 1) < 0) {
                fprintf(stderr, "Error with write: %s\n", strerror(errno));
                exit(1);
            }
          }
        }
        

        if(pollfds[0].revents & (POLLHUP & POLLERR)) 
          exitReady = 1;

        if(pollfds[1].revents & (POLLHUP & POLLERR))
          exitReady = 1;

        if(exitReady)
          break;
      }
      
    
      //we are out of the loop but still in parent process so close pipes
      if(pipeNotClosed)
        close(toShell[1]);

      int exitStat;
      int wret = waitpid(child, &exitStat, 0);
      if(wret<0) {
        fprintf(stderr, "Error with wait: %s\n", strerror(errno));
        exit(1);
      }

      int sig = exitStat & 0x7f;
      int stat = WEXITSTATUS(exitStat);
      fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", sig, stat);
      exit(0);
      
    }
  }
  
  //code for copying keyboard input to the display
  int i, bytesRead;
  char temp = '*';
  while (1) {
    bytesRead = read(STDIN_FILENO, &buf, sizeof(buf));
    for(i = 0; i < bytesRead; i++) {
      if (buf[i] == '\004') {
        char potato[] = {'^', 'D', '\r','\n'};
        write(STDOUT_FILENO, potato, 4 * sizeof(char));
        exit(0);
      }
      else if(buf[i] == '\r' || buf[i] == '\n')
        write(STDOUT_FILENO, rn, 2 * sizeof(char));
      else {
        temp = buf[i];
        write(STDOUT_FILENO, &temp, sizeof(char));
      }
    }
  }
                    
  exit(0);
}

