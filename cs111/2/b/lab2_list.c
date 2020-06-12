// Adam Young youngcadam@ucla.edu
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
    fprintf(stderr, "Segmentation fault. Exiting.\n");
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

//for our newly requested time and synch params
long long threadTimes[] = { 0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0};

int* listindex;

//define global variables/flags used throughout the program
long long count = 0;

int nthreads = 1;
int threadflag = 0;

int niter = 1;
int iterflag= 0;

int syncflag = 0;
pthread_mutex_t* mutex;
int* spinlocks;

int listsflag = 0;
int nlists = 1;

char test[20] = "";
char synctype = 'n';

int opt_yield = 0;

SortedList_t* lists;
SortedListElement_t* elements;
int nelements = 0;

//thread function
void *threadFun(void *vargp) {  
  int tid = *((int*) vargp);
  int i;
  //insert elements
  struct timespec start, finish;
  for(i = tid; i < nelements; i += nthreads) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization
	SortedList_insert(&lists[listindex[i]], &elements[i]);
	//	printf("list->next->key = %s\n", list->next->key);
	//	printf("list->prev->key = %s\n", list->prev->key);
	break;
	
      case 'm':  //mutex
	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_mutex_lock(&mutex[listindex[i]]);
	clock_gettime(CLOCK_MONOTONIC, &finish);
	//	printf("hi2, i = %d, listindex[i] = %d, thread = %d\n", i, listindex[i], tid);
	SortedList_insert(&lists[listindex[i]], &elements[i]);
	threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	pthread_mutex_unlock(&mutex[listindex[i]]);
	break;
	
      case 's':   //spin-lock
	clock_gettime(CLOCK_MONOTONIC, &start);
	while (__sync_lock_test_and_set(&spinlocks[listindex[i]], 1))
	  while(spinlocks[listindex[i]]); //spin until we can get the lock
	clock_gettime(CLOCK_MONOTONIC, &finish);
	threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	SortedList_insert(&lists[listindex[i]], &elements[i]);
	__sync_lock_release(&spinlocks[listindex[i]]);	
	break;
      }
  }

  //printf("made it to length\n");

  //call length function
  int length;
  for(i = 0; i < nlists; i++) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization                                      
	if(SortedList_length(&lists[i]) == -1){
	  fprintf(stderr, "Error: SortedList_length returned -1.\n");
	  exit(2);
	}
        break;

      case 'm':  //mutex
	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_mutex_lock(&mutex[i]);
	clock_gettime(CLOCK_MONOTONIC, &finish);
	threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	length = SortedList_length(&lists[i]);
	if(length == -1) {
	  fprintf(stderr, "Error: SortedList_length returned -1.\n");
	  exit(2);
	}
	pthread_mutex_unlock(&mutex[i]);
        break;

      case 's':   //spin-lock
	clock_gettime(CLOCK_MONOTONIC, &start);
        while (__sync_lock_test_and_set(&spinlocks[i], 1))
          while(spinlocks[i]);
	clock_gettime(CLOCK_MONOTONIC, &finish);
	threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	SortedList_length(&lists[i]);
	if(length == -1) {
          fprintf(stderr, "Error: SortedList_length returned -1.\n");
          exit(2);
        }
        __sync_lock_release(&spinlocks[i]);
        break;
      }
  }

  //lookup and delete elements
  SortedListElement_t* temp;
  for(i = tid; i < nelements; i += nthreads) {
    switch(synctype)
      {
      case 'n':  //no syncrhonization                                                                                 
	temp = SortedList_lookup(&lists[listindex[i]], elements[i].key);
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
	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_mutex_lock(&mutex[listindex[i]]);
	clock_gettime(CLOCK_MONOTONIC, &finish);
        threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	temp = SortedList_lookup(&lists[listindex[i]], elements[i].key);
	if (temp == NULL) { //checl for failure on lookup
          fprintf(stderr, "Error: SortedList_lookup failed.\n");
          exit(2);
        }
        if (SortedList_delete(temp)) {  //check for failure on delete
          fprintf(stderr, "Error: SortedList_delete returned a nonzero number.\n");
          exit(2);
        }
	pthread_mutex_unlock(&mutex[listindex[i]]);
        break;

      case 's':   //spin-lock
	clock_gettime(CLOCK_MONOTONIC, &start);
        while (__sync_lock_test_and_set(&spinlocks[listindex[i]], 1))
          while(spinlocks[listindex[i]]);
	clock_gettime(CLOCK_MONOTONIC, &finish);
        threadTimes[tid] += 1000000000L * (start.tv_sec - finish.tv_sec) + finish.tv_nsec - start.tv_nsec;
	temp = SortedList_lookup(&lists[listindex[i]], elements[i].key);
	if (temp == NULL) {
          fprintf(stderr, "Error: SortedList_lookup failed.\n");
          exit(2);
        }
        if (SortedList_delete(temp)) { 
          fprintf(stderr, "Error: SortedList_delete returned a nonzero number.\n");
          exit(2);
        }
        __sync_lock_release(&spinlocks[listindex[i]]);
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
    {"lists", required_argument, NULL, 'l'},
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
      case 'l':
	listsflag = 1;
	nlists = atoi(optarg);
	break;
      case 's':
	syncflag = 1;
	//mutex
      	if (optarg[0] == 'm') 
	  {
	    //	    pthread_mutex_init(&mutex, NULL); //this is taken care of later
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


  nelements = nthreads * niter;
  lists = malloc(nlists * sizeof(SortedList_t));
  for (i = 0; i < nlists; i++) {
    lists[i].key = NULL;
    lists[i].next = &lists[i];
    lists[i].prev = &lists[i];
  }



  if(!opt_yield)
    strcat(test, "-none");

  //added additional synchronization parameters for 2b
  switch(synctype) 
    {
    case 'n':
      strcat(test, "-none");
      break;
    case 'm':
      strcat(test, "-m");
      mutex = malloc(nlists*sizeof(pthread_mutex_t));
      for (i = 0; i < nlists; i++)
	pthread_mutex_init(&mutex[i], NULL);
      break;
    case 's':
      spinlocks = malloc(nlists * sizeof(int));
      for (i = 0; i < nlists; i++)
	spinlocks[i] = 0;
      strcat(test, "-s");
      break;
    }
  
  //initialize empty list
  elements = malloc(nelements*sizeof(SortedListElement_t));
  for(i = 0; i < nelements; i++) {
    char* key = (char*) malloc(10*sizeof(char)); //allocate space for 10-byte random string
    randstr(key);
    elements[i].key = key;
  }
  
  //build index array based on each element's hash
  listindex = malloc(nelements*sizeof(int));
  for (i = 0; i < nelements; i++) {
    int ikey = (int) elements[i].key[0];
    listindex[i] = (ikey % nlists);
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
  long long new_time = 0L;
  for(i = 0; new_time < nthreads; i++)
    new_time += threadTimes[i];
  //free allocated memory and output result
  free(tids);
  free(threads);
  free(mutex);
  free(elements);
  printf("%s,%d,%d,%d,%d,%lld,%d,%lld\n", test, nthreads, niter, nlists, nops, elapsed, avgop, new_time);
  exit(0);
}
