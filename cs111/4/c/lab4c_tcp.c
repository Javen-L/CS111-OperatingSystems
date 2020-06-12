//Adam Young youngcadam@ucla.edu
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mraa.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <getopt.h>

#define TPIN 1
#define B 4275
#define R0 100000.0


//global variables used throughout the program
mraa_aio_context sensor;

int period = 1;
char temptype = 'F';
char* id;
char* hostname;
int hostflag = 0;
int idflag = 0;
FILE *fd = 0;
int logflag = 0;
int sockfd = 0;
float temp = 69.1;
void convert_set_temp(int reading);

//error function that prints a message and then the actual error code/string
void error(char str[]) {
  fprintf(stderr, "%s\n%s\n", str, strerror(errno));
  exit(2);
}

//modified time function from lab2 to include temperature part
void print_current_time(int reading) {
  struct timespec ts;
  struct tm * tm;
  convert_set_temp(reading);
  clock_gettime(CLOCK_REALTIME, &ts);
  tm = localtime(&(ts.tv_sec));
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
  dprintf(sockfd, "%s", buf);
  fputs(buf, fd);
}

//function for handling the shutdown sequence
void off() {
  char buf[20];
  struct timespec ts;
  struct tm * tm;
  clock_gettime(CLOCK_REALTIME, &ts);
  tm = localtime(&(ts.tv_sec));
  sprintf(buf, "%02d:%02d:%02d SHUTDOWN\nOFF\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
  dprintf(sockfd, "%s", buf);
  fputs(buf, fd);
  exit(0);
}


//set temperature according to reading (modified from function provided by Diyu)
void convert_set_temp(int reading) {
  float R = 1023.0/((float) reading) - 1.0;
  R = R0*R;  
  //temperature in Celcious
  temp = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;  
  if(temptype == 'F')
    temp = (temp * 9)/5;
}

//client connect function copied from Diyu, included some error checks
int client_connect(char * host_name, int port) {
  struct sockaddr_in serv_addr; //encode the ip address and the port for the
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
    error("Error opening socket.");
  // AF_INET: IPv4, SOCK_STREAM: TCP connection
  struct hostent *server = gethostbyname(host_name);
  if (!server)
    error("Error: failed to connect to host.");
  
  // convert host_name to IP addr
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET; //address is Ipv4
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  	
  //copy ip address from server to serv_ad
  serv_addr.sin_port = htons(port); //setup the port

  //initiate the connection to server, return correct socket fd
  if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0)
     error("Error: failed to connect to server.");
  return sockfd; 
 }

int main(int argc, char* argv[]) { 
  //close stdin and stdout
  close(STDIN_FILENO);
  close(STDOUT_FILENO);

  //getopt
  struct option options[] = {
    {"period", required_argument, NULL, 'p'},
    {"scale", required_argument, NULL, 's'},
    {"log", required_argument, NULL, 'l'},
    {"id", required_argument, NULL, 'i'},
    {"host", required_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) 
      {
      case 'p':
	period = atoi(optarg);
	break;
      case 'l':
	logflag = 1;										
	fd = fopen(optarg, "w+");
	break;
      case 'i':
	idflag = 1;
	id = optarg;
	break;
      case 'h':
	hostflag = 1;
	hostname = optarg;
	break;
      case 's': //moved this to the end so it falls through to a usage error with bad args
	if((optarg[0] == 'C' || optarg[0] == 'F') && strlen(optarg) == 1) {
	  temptype = optarg[0];      	
	  break;
	}
      default:
	fprintf(stderr, "Error: invalid arguments.\n");
	fprintf(stderr, "Correct usage: ./lab2_tcp [--period] [--scale] [--log] [--host] [--id]\n");
	fflush(stderr);
	exit(1);
      }
  }

  //check for mandatory arguments
  if(!logflag || !hostflag || !idflag) {
    fprintf(stderr, "Error: the mandatory options are --log, --host, --id.");
    exit(1);
  }

  //include networking components for project 4c; copied from Diyu's slides
  int port = atoi(argv[argc - 1]);
  if(port == 0)
    error("Error: port is a mandatory argument.");
  sockfd = client_connect(hostname, port);
  if(sockfd < 0)
    error("Error: socket value is less than 0.");
  char idstr[12];
  sprintf(idstr, "ID=%s\n", id);
  dprintf(sockfd, "%s", idstr);
  fputs(idstr, fd);

  //initialize sensor
  sensor = mraa_aio_init(TPIN);
  
  int start = 1; //used to detect START/STOP 
  
  //variables for polling input to see if user inputted any new settings
  struct pollfd fds;
  fds.fd = sockfd;
  fds.events = POLLIN | POLLERR | POLLHUP;
  
  //variables for making sure updates only happen every PERIOD seconds
  struct timespec ts;
  struct tm * tm;
  clock_gettime(CLOCK_REALTIME, &ts);
  tm = localtime(&(ts.tv_sec));
  int firstiter = 1;
  int time_new = tm->tm_sec;
  int time_old = time_new - 1;
  int elapsed = time_new - time_old;
  while(1) {
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    time_new= tm->tm_sec;
    elapsed = time_new - time_old;
    //deal with time and temp outputs
    if((start && ((elapsed >= period) || (elapsed < 0))) || firstiter) {
      firstiter = 0;
      int reading = mraa_aio_read(sensor);
      print_current_time(reading);  //this function takes care of all output now
      time_old= tm->tm_sec; //record the time of the last update
    }

    //call poll and check to see if there are inputs to process; if so, process them
    int ret = poll(&fds, 1, 0);
    if(ret < 0)
      error("Error calling poll.");
    if ((fds.revents & POLLIN)){
      FILE* fl = fdopen(sockfd, "r");     //fgets fails to read directly from sockfd, so I used a file pointer
      char buff[40];
      fgets(buff, 20, fl);
      if(strncmp(buff, "STOP", strlen("STOP")) == 0)
	start = 0;
      if(strncmp(buff, "START", strlen("START")) == 0)
	start = 1;
      if(strncmp(buff, "OFF", strlen("OFF")) == 0)
	off();
      if(strncmp(buff, "SCALE=F", strlen("SCALE=F")) == 0)
	temptype = 'F';
      if(strncmp(buff, "SCALE=C", strlen("SCALE=C")) == 0)
	temptype = 'C';
      if(strncmp(buff, "PERIOD=", strlen("PERIOD=")) == 0)
	period = atoi(&buff[7]);
      if(strncmp(buff, "LOG ", 4) == 0 && strlen(buff) > 4){}
      fputs(buff, fd);
    }    
  }

  mraa_aio_close(sensor);
  exit(0);
}
