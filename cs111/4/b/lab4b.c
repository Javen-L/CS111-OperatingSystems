//Adam Young youngcadam@ucla.edu
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mraa.h>
#include <poll.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <getopt.h>

#define TPIN 1
#define BPIN 60
#define B 4275
#define R0 100000.0


//global variables used throughout the program
mraa_aio_context sensor;
mraa_gpio_context button;

int period = 1;
char temptype = 'F';
FILE *fd = 0;
int logflag = 0;

float temp = 69.1;


//error function that prints a message and then the actual error code/string
void error(char str[]) {
  fprintf(stderr, "%s\n%s\n", str, strerror(errno));
  exit(1);
}


//modified time function provided by Diyu so that it prints time + space
void print_current_time(void) {
	struct timespec ts;
	struct tm * tm;
	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&(ts.tv_sec));
	char buf[10];
	sprintf(buf, "%02d:%02d:%02d ", tm->tm_hour, tm->tm_min, tm->tm_sec);
	printf("%s", buf);
	if(logflag)
		fputs(buf, fd);
}

//function for handling the shutdown sequence
void off() {
	char buf[20];
	print_current_time();
	sprintf(buf,"SHUTDOWN\n");
	printf("%s",buf);
	if(logflag)
		fputs(buf, fd);
	exit(0);
}


//set temperature according to reading (modified from function provided by Diyu)
void convert_set_temp(int reading)
{
	float R = 1023.0/((float) reading) - 1.0;
	R = R0*R;
	//temperature in Celcious
	temp = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
	
	if(temptype == 'F')
		temp = (temp * 9)/5;
}



int main(int argc, char* argv[]) { 
 
  struct option options[] = {
    {"period", required_argument, NULL, 'p'},
    {"scale", required_argument, NULL, 's'},
    {"log", required_argument, NULL, 'l'},
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
			case 's': //moved this to the end so it falls through to a usage error with bad args
      	if((optarg[0] == 'C' || optarg[0] == 'F') && strlen(optarg) == 1) {
					temptype = optarg[0];      	
					break;
				}
	default:
		fprintf(stderr, "Error: invalid argiments. ");
		fprintf(stderr, "Correct usage: ./lab2_list [--period][--scale][--log]\n");
		exit(1);
		}
	}
	
	//initialize sensors and buttons
	sensor = mraa_aio_init(TPIN);
	button = mraa_gpio_init(BPIN);
	if (button == NULL)
	{
		fprintf(stderr, "Failed to initialize the button\n");
		exit(1);
	}
	mraa_gpio_dir(button, MRAA_GPIO_IN);
	mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &off, NULL);
	int start = 1;	

	//variables for polling input to see if user inputted any new settings
	struct pollfd fds;
	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;

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
		if((start && ((elapsed >= period) || (elapsed < 0))) || firstiter){
			firstiter = 0;
			char buf[20];		
			int reading = mraa_aio_read(sensor);
			convert_set_temp(reading);
			print_current_time();
			sprintf(buf,"%.1f\n", temp);
			printf("%s", buf);
			if(logflag)
				fputs(buf, fd);
			time_old= tm->tm_sec; //record the time of the last update
		}

		int ret = poll(&fds, 1, 0);
		if(ret < 0)
			error("Error with poll.");

		if ((fds.revents & POLLIN)){
			char buff[20];
			fgets(buff, 20, stdin);
			if(strncmp(buff, "STOP", strlen("STOP")) == 0)
				start = 0;
			if(strncmp(buff, "START", strlen("START")) == 0)
				start = 1;
			if(strncmp(buff, "OFF", strlen("OFF")) == 0){
				if(logflag)
					fputs(buff, fd);
				off();
			}
			if(strncmp(buff, "SCALE=F", strlen("SCALE=F")) == 0)
				temptype = 'F';
			if(strncmp(buff, "SCALE=C", strlen("SCALE=C")) == 0)
				temptype = 'C';
			if(strncmp(buff, "PERIOD=", strlen("PERIOD=")) == 0)
				period = atoi(&buff[7]);
			if(strcmp(buff, "LOG") == 0){}
			if(logflag)
				fputs(buff, fd);
		}
		
		if (mraa_gpio_read(button))
			off();
			
	}

	mraa_aio_close(sensor);
	mraa_gpio_close(button);

  exit(0);
}
