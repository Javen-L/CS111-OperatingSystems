#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "SortedList.h"

//error function for convenient error closing                                                                                             
void error(const char* msg) {
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

//signal handler to catch segfaults
void signal_handler(int sig) {
  if(sig == SIGSEGV) {
    fprintf(stderr, "Segmentation fault. Exiting.");
    exit(2);
  }
}

//create random 10-byte string 
void randstr(char* buf) {
  static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  int i;
  for (i = 0; i < 9; ++i) 
    buf[i] = letters[rand() % 52];    
  buf[9] = '\0';
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

int opt_yield = 0;

SortedList_t* list;
SortedListElement_t* elements;
int nelements = 0;

//thread function
void *threadFun(void *vargp) {  
  int tid = *((int*) vargp);
  int i;
  //insert elements
  for(i = tid; i < nelements; i += nthreads) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization
	SortedList_insert(list, &elements[i]);
	//	printf("list->next->key = %s\n", list->next->key);
	//	printf("list->prev->key = %s\n", list->prev->key);
	break;
	
      case 'm':  //mutex
	pthread_mutex_lock(&mutex);
	SortedList_insert(list, &elements[i]);
	pthread_mutex_unlock(&mutex);
	break;
	
      case 's':   //spin-lock
	while (__sync_lock_test_and_set(&spinlock, 1))
	  while(spinlock); //spin until we can get the lock
	SortedList_insert(list, &elements[i]);
	__sync_lock_release(&spinlock);	
	break;
      }
  }

  //call length function
  int length;
  for(i = tid; i < nelements; i += nthreads) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization                                      
	if(SortedList_length(list) == -1){
	  fprintf(stderr, "Error: SortedList_length returned -1.\n");
	  exit(2);
	}
        break;

      case 'm':  //mutex                                                                                              
	pthread_mutex_lock(&mutex);
	length = SortedList_length(list);
	if(length == -1) {
	  fprintf(stderr, "Error: SortedList_length returned -1.\n");
	  exit(2);
	}
	pthread_mutex_unlock(&mutex);
        break;

      case 's':   //spin-lock                                                                                         
        while (__sync_lock_test_and_set(&spinlock, 1))
          while(spinlock); 	
	SortedList_length(list);
	if(length == -1) {
          fprintf(stderr, "Error: SortedList_length returned -1.\n");
          exit(2);
        }
        __sync_lock_release(&spinlock);
        break;
      }
  }

  //lookup and delete elements
  SortedListElement_t* temp;
  for(i = tid; i < nelements; i += nthreads) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization                                                                                 
	temp = SortedList_lookup(list, elements[i].key);
	if (temp == NULL) { //check for failure on lookup
	  fprintf(stderr, "Error: SortedList_lookup failed. Element %d not found\n",i);
	  exit(2);
	}
	if (SortedList_delete(temp)) {  //check for failure on delete
	  fprintf(stderr, "Error: SortedList_delete returned a nonzero number.\n");
	  exit(2);
	}
        break;

      case 'm':  //mutex                                                                                              
	pthread_mutex_lock(&mutex);
	temp = SortedList_lookup(list, elements[i].key);
	if (temp == NULL) { //checl for failure on lookup
          fprintf(stderr, "Error: SortedList_lookup failed.\n");
          exit(2);
        }
        if (SortedList_delete(temp)) {  //check for failure on delete
          fprintf(stderr, "Error: SortedList_delete returned a nonzero number.\n");
          exit(2);
        }
	pthread_mutex_unlock(&mutex);
        break;

      case 's':   //spin-lock                                                                                         
        while (__sync_lock_test_and_set(&spinlock, 1))
          while(spinlock);
	temp = SortedList_lookup(list, elements[i].key);
	if (temp == NULL) { //checl for failure on lookup
          fprintf(stderr, "Error: SortedList_lookup failed.\n");
          exit(2);
        }
        if (SortedList_delete(temp)) {  //check for failure on delete 
          fprintf(stderr, "Error: SortedList_delete returned a nonzero number.\n");
          exit(2);
        }
        __sync_lock_release(&spinlock);
        break;
      }
  }
  return NULL;
}



int main(int argc, char* argv[]) {
  
  signal(SIGSEGV, signal_handler);
  
  struct option options[] = {
    {"threads", required_argument, NULL, 't'},
    {"iterations", required_argument, NULL, 'i'},
    {"yield", required_argument, NULL, 'y'},
    {"sync", required_argument, NULL, 's'},
    {0, 0, 0, 0}
  };
  
  int c, i;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) 
      {
      case 't':
	strcat(test, "list");
	threadflag = 1;
	nthreads = atoi(optarg);
	break;
      case 'i':
	iterflag = 1;
	niter = atoi(optarg);
	break;
      case 'y':
	opt_yield = 1;
	strcat(test, "-");
	size_t j = 0;
	for (j = 0; j < strlen(optarg); j++) {
	  if(optarg[j] == 'i') {
	    strcat(test, "i");
	    opt_yield |= INSERT_YIELD;					 
	  }
	  else if (optarg[j] == 'd'){
	    strcat(test, "d");
	    opt_yield |= DELETE_YIELD;
	  }
	  else if (optarg[j] == 'l') {
	    strcat(test, "l");
	    opt_yield |= LOOKUP_YIELD;					
	  }
	}
	break;
      case 's':
	syncflag = 1;
	//mutex
      	if (optarg[0] == 'm') 
	  {
	    pthread_mutex_init(&mutex, NULL);
	    synctype = 'm';
	    break;
	  }
      	//spin
      	else if (optarg[0] == 's')
	  {
	    synctype = 's';
	    break;
	  }      					
      default:
	fprintf(stderr, "Error: invalid argiments.");
	fprintf(stderr, "Correct usage: ./lab2_list [--threads][--iterations][--yield][--sync]\n");
	exit(1);
      }
  }

  if(!opt_yield)
    strcat(test, "-none");

  switch(synctype) 
    {
    case 'n':
      strcat(test, "-none");
      break;
    case 'm':
      strcat(test, "-m");
      break;
    case 's':
      strcat(test, "-s");
      break;
    }

  //setup empty list
  nelements = nthreads * niter;
  list = (SortedList_t *) malloc(sizeof(SortedList_t));
  list->next = list;
  list->prev = list;
  list->key = NULL;
  
  
  //initialize empty list
  elements = malloc(nelements*sizeof(SortedListElement_t));
  for(i = 0; i < nelements; i++) {
    char* key = (char*) malloc(10*sizeof(char)); //allocate space for 10-byte random string
    randstr(key);
    elements[i].key = key;
  }
  
  //get start time
  struct timespec start, finish; 
  clock_gettime(CLOCK_MONOTONIC, &start);
  
  //allocate space for threads
  pthread_t* threads = (pthread_t*) malloc(nthreads*sizeof(pthread_t));
  int* tids = malloc(nthreads*sizeof(int));
  
  
  //run threads
  for (i = 0; i < nthreads; i++){
    tids[i] = i;
    if (pthread_create(threads + i, NULL, threadFun, &tids[i]))
      error("Error creating threads.");
  }
  
  //wait for threads
  for (i = 0; i < nthreads; i++) 
    if(pthread_join(*(threads + i), NULL))
      error("Error joining threads.");		
  
  
  //get the finish clock time
  clock_gettime(CLOCK_MONOTONIC, &finish);
  long long elapsed = 1000000000L * (finish.tv_sec - start.tv_sec) + finish.tv_nsec - start.tv_nsec;
  int nops = nthreads*niter*3;
  int avgop = elapsed / nops;
  
  //free allocated memory and output result
  free(tids);
  free(threads);
  free(elements);
  printf("%s,%d,%d,1,%d,%lld,%d\n", test, nthreads, niter, nops, elapsed, avgop);
  exit(0);
}
