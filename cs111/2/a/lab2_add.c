// Written by Adam Young youngcadam@ucla.edu
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

//error function for convenient error closing
void error(const char* msg) {
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

//define global variables/flags used throughout the program
long long count = 0;

int nthreads = 1;
int threadflag = 0;

int niter = 1;
int iterflag= 0;

int syncflag = 0;
pthread_mutex_t mutex;
int spinlock = 0;

char test[20] = "";
char synctype = 'n';


//add function provided
int opt_yield;
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    sched_yield();
  *pointer = sum;
}

//compare and swap atomic exchange
void addcas(long long *pointer, long long value) {
  long long old;
  do {
    old = count;
    if (opt_yield) {
      sched_yield();
    }
  } while (__sync_val_compare_and_swap(pointer, old, old+value) != old);
}

//function for thread to call
void *threadFun() {  
  int i;
  int value = 1;
  switch(synctype)
    {
    case 'n':
      for(i = 0; i < niter; i++) 
	add(&count, value);
      value = -1; //now add -1
      for (i = 0; i < niter; i++) 
	add(&count, value);     
      break;
    
    case 'm':
      for(i = 0; i < niter; i++) {
	pthread_mutex_lock(&mutex);
        add(&count, value);
	pthread_mutex_unlock(&mutex);
      }
      value = -1; //now add -1                                                                                                             
      for(i = 0; i < niter; i++) {
	pthread_mutex_lock(&mutex);
        add(&count, value);
	pthread_mutex_unlock(&mutex);
      }
      break;
    
    case 's':   //spin lock
      for(i = 0; i < niter; i++) {
	while (__sync_lock_test_and_set(&spinlock, 1))
	  ; //spin until we can get the lock
	add(&count, value);
	__sync_lock_release(&spinlock);
      }
      value = -1; //now add -1
      for(i = 0; i < niter; i++) {
	while (__sync_lock_test_and_set(&spinlock, 1))
          ; //spin until we can get the lock
        add(&count, value);
        __sync_lock_release(&spinlock);
      }
      break;
    
    case 'c':
      ;
      for(i = 0;i < niter; i++)
	addcas(&count, value);
      value = -1;
      for(i = 0;i < niter; i++)
	addcas(&count, value);
      break;
    }
  return NULL;
}

int main(int argc, char *argv[])
{
  //process options with getopt_long
  static struct option long_options[] = {
    {"threads", 1, NULL, 't'},
    {"iterations", 1, NULL, 'i'},
    {"yield", 0, NULL, 'y'},
    {"sync", 1, NULL, 's'},
    {0,0,0,0}
  };
  //index, usage message, and flags for getopt arguments
  int index;
  char* correct_usage = "Correct usage: ./lab2-add [--threads][--iterations][--yield][--sync]\n";

  int c;

  //get opts loop
  while (1) {
    c = getopt_long(argc, argv, "", long_options, &index);    
    if (c == -1)
      break;
    
    switch(c)
      {
      //threads
      case 't':           
	strcat(test, "add");
	threadflag = 1;
	nthreads = atoi(optarg);
	break;
      //iterations
      case 'i':
	iterflag = 1;
	niter = atoi(optarg);
	break;
      //yield
      case 'y':
	strcat(test, "-yield");
	opt_yield = 1;
	break;
      //sync
      case 's':
	syncflag = 1;
	//mutex
	if (optarg[0] == 'm') 
	{
	  pthread_mutex_init(&mutex, NULL);
	  strcat(test, "-m");
	  synctype = 'm';
	  break;
	}
	//spin
	else if (optarg[0] == 's')
	{
	  strcat(test, "-s");
	  synctype = 's';
	  break;
	}
	//compare-and-swap
	else if (optarg[0] == 'c')
	{
	  synctype = 'c';
	  strcat(test, "-c");
	  break;
	}	
      default:
	fprintf(stderr, "Error: invalid arguments. %s\n", correct_usage);
	exit(1);
      }
  }

  //cat correct name onto entry string
  if(!syncflag) 
    strcat(test, "-none");

  //get start time
  struct timespec start, finish; 
  clock_gettime(CLOCK_MONOTONIC, &start);

  //allocate space for threads
  pthread_t* threads = (pthread_t*) malloc(nthreads * sizeof(pthread_t));
  int i;

  //run the specified add function on each thread
  for(i = 0; i < nthreads; i++) 
    if (pthread_create(&threads[i], NULL, threadFun, NULL))
      error("Error creating threads");
  
  //wait for threads to finish with join()
  for (i = 0; i < nthreads; i++)
    if (pthread_join(threads[i], NULL))
      error("Error joining threads");    

  //get the finish clock time
  clock_gettime(CLOCK_MONOTONIC, &finish);
  
  free(threads);
  long long elapsed = 1000000000L * (finish.tv_sec - start.tv_sec) + finish.tv_nsec - start.tv_nsec;
  int nops = nthreads*niter*2;
  int avgop = elapsed / nops;
  
  printf("%s,%d,%d,%d,%lld,%d,%lld\n", test, nthreads, niter, nops, elapsed, avgop, count);
  
  exit(0);
}
