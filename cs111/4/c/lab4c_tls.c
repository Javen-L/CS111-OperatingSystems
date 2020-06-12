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
#include <openssl/ssl.h>
#include <openssl/err.h>

#define TPIN 1
#define B 4275
#define R0 100000.0


//global variables
mraa_aio_context sensor; //temperature sensor vars
char temptype = 'F';
float temp = 69.1;

int period = 1; //command line arguments
char *hostname = NULL; 
int hostflag = 0;
int idflag = 0;
char *id = NULL;
int logflag = 0;

FILE *fd = 0; //file descriptors
int sockfd = 0;

SSL_CTX *context = NULL; //openssl
SSL *ssl;

//set temperature according to reading (modified from function provided by Diyu)                                                                                   
void convert_set_temp(int reading) {
  float R = 1023.0/((float) reading) - 1.0;
  R = R0*R;
  //temperature in Celcius 
  temp = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
  if(temptype == 'F')
    temp = (temp * 9)/5;
}

//error function that prints a message and then the actual error code/string                                                                                       
void myerror(char str[]) {
  fprintf(stderr, "%s\n%s\n", str, strerror(errno));
  exit(2);
}



//SSL functions copied from Diyu's slides
SSL_CTX *ssl_init(void) {
  SSL_CTX *newContext = NULL;
  if(SSL_library_init() < 0)
    myerror("Error calling SSL_library_init().");
  SSL_load_error_strings(); //Initialize the error message
  OpenSSL_add_all_algorithms();
  newContext = SSL_CTX_new(TLSv1_client_method()); //Initialize the error message
  return newContext;
}

void attach_ssl_to_socket(int socket, SSL_CTX *contex) {
  ssl = SSL_new(contex);
  if(SSL_set_fd(ssl, socket) == 0)
    myerror("Error setting ssl to socket fd.");
  if(!SSL_connect(ssl))
    myerror("Error connecting ssl client.");
}

//modified time function from lab2 to include temperature part
void print_current_time(int reading) {
  struct timespec ts;
  struct tm *tm;
  convert_set_temp(reading);
  clock_gettime(CLOCK_REALTIME, &ts);
  tm = localtime(&(ts.tv_sec));
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
  //  dprintf(sockfd, "%s", buf);
  SSL_write(ssl, buf, strlen(buf));
  fputs(buf, fd);
}

//function for handling the shutdown sequence
void off() {
  char buf[20];
  struct timespec ts;
  struct tm *tm;
  clock_gettime(CLOCK_REALTIME, &ts);
  tm = localtime(&(ts.tv_sec));
  sprintf(buf, "%02d:%02d:%02d SHUTDOWN\nOFF\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
  //  dprintf(sockfd, "%s", buf);
  SSL_write(ssl, buf, strlen(buf));
  fputs(buf, fd);
  exit(0);
}

//client connect function copied from Diyu, included some error checks
int client_connect(char *host_name, int port) {
  struct sockaddr_in serv_addr; //encode the ip address and the port for the
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
    myerror("Error opening socket.");
  // AF_INET: IPv4, SOCK_STREAM: TCP connection
  struct hostent *server = gethostbyname(host_name);
  if (!server)
    myerror("Error: failed to connect to host.");
  
  // convert host_name to IP addr
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET; //address is Ipv4
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  	
  //copy ip address from server to serv_ad
  serv_addr.sin_port = htons(port); //setup the port

  //initiate the connection to server, return correct socket fd
  if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0)
     myerror("Error: failed to connect to server.");
  return sockfd; 
 }

void cleanup(void) {
  mraa_aio_close(sensor);
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(sockfd);
}


int main(int argc, char *argv[]) { 
  
  // close stdin and stdout
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
  while (1) {

    c = getopt_long(argc, argv, "", options, NULL);
    if(c == -1)
      break;
    
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
	fprintf(stderr, "Correct usage: ./lab2_tls [--period][--scale][--log][--host][--id] port\n");
	exit(1);
      }
  }

  int i = argc + 1;
  if(optind > i) {
    fprintf(stderr, "Correct usage: ./lab2_tls [--period][--scale][--log][--host][--id] port\n");
    exit(1);
  }

  //check for mandatory arguments
  if((!logflag) || (!hostflag) || (!idflag)) {
    fprintf(stderr, "Correct usage: ./lab2_tls [--period] [--scale] [--log] [--host] [--id] port\n");
    exit(1);
  }

  //include networking components for project 4c; copied from Diyu's slides
  int port = atoi(argv[argc - 1]);
  if(port == 0)
    myerror("Error: port is a mandatory argument.");
  sockfd = client_connect(hostname, port);
  if(sockfd < 0)
    myerror("Error: socket value is less than 0.");

  //  dprintf(sockfd, "ID=%s\n", id);
  //  SSL_write(ssl, id, strlen(id));
  //  fputs(id, fd);

  //initialize openssl
  context = ssl_init();
  if(!context) {
    myerror("Error: context was set to NULL when trying to initialize.");
  }
  attach_ssl_to_socket(sockfd, context); //connect SSL context to socket
  // ssl = SSL_new(context);
  char idstr[12];
  sprintf(idstr, "ID=%s\n", id);
  SSL_write(ssl, idstr, strlen(idstr) + 1);
  fputs(idstr, fd);
  
  //initialize sensor
  sensor = mraa_aio_init(TPIN);

  atexit(cleanup);
  
  int start = 1; //used to detect START/STOP 
  
  //variables for polling input to see if user inputted any new settings
  struct pollfd fds;
  fds.fd = sockfd;
  fds.events = POLLIN | POLLERR | POLLHUP;
  
  //variables for making sure updates only happen every PERIOD seconds
  struct timespec ts;
  struct tm *tm;
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
      myerror("Error calling poll.");
    if ((fds.revents & POLLIN)){
      char buff[40];
      //      fgets(buff, 20, fl);
      SSL_read(ssl, buff, strlen(buff));
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
      SSL_write(ssl, buff, strlen(buff) + 1);
      fputs(buff, fd);      
    }    
  }

  exit(0); //at exit, we'll cleanup
}
