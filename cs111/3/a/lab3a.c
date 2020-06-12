// Written by Adam Young youngcadam@ucla.edu
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

#define SUPERSIZEME 1024

//global variable/function declarations that are used throughout the program
struct ext2_super_block superstruct;
struct ext2_group_desc groupstruct;
struct ext2_inode inodestruct;
unsigned int bsize; //block size 
unsigned int isize; //inode size 
int fsfd; //image file descriptor
void inode_summary(unsigned int ninode);
void directory_entries(int ninode);
void indirect_entry(int ndepth, int ninode, int baseoffset, __u32 bnum);

//time helper function that gets, formats, and prints requested times
void time_helper(void) {
  
  char buf1[20];
  char buf2[20];
  char buf3[20];

  //output creation time
  time_t ctime = inodestruct.i_ctime; 
  struct tm temp1 = *gmtime(&ctime);
  strftime(buf1, 20, "%m/%d/%y %H:%M:%S", &temp1);
  printf("%s,", buf1);

  //output last modified time
  time_t mtime = inodestruct.i_mtime;
  struct tm temp2 = *gmtime(&mtime);
  strftime(buf2, 20, "%m/%d/%y %H:%M:%S", &temp2);
  printf("%s,", buf2);

  //output last accessed time
  time_t atime = inodestruct.i_atime;
  struct tm temp3 = *gmtime(&atime);
  strftime(buf3, 20, "%m/%d/%y %H:%M:%S", &temp3);
  printf("%s,", buf3);
}

//error function for convenient error closing
void error(char msg[]) {
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

//function for superblock summary section
void superblock_summary(void) {
  pread(fsfd, &superstruct, sizeof(superstruct), SUPERSIZEME);
  bsize = EXT2_MIN_BLOCK_SIZE << superstruct.s_log_block_size;
  isize = superstruct.s_inodes_per_group / 8;
                   // 1  2  3  4  5  6  7
  printf("SUPERBLOCK,%d,%d,%u,%d,%d,%d,%d\n",
	 superstruct.s_blocks_count,
	 superstruct.s_inodes_count,
	 bsize,
	 superstruct.s_inode_size,
	 superstruct.s_blocks_per_group,
	 superstruct.s_inodes_per_group,
	 superstruct.s_first_ino);
}

//function for group summary section
void group_summary(void) {
  //note that SUPERSIZEME is the size of both block size and super offset
  int goffset = SUPERSIZEME*2;
  pread(fsfd, &groupstruct, sizeof(groupstruct), goffset);
                // 1  2  3  4  5  6  7
  printf("GROUP,0,%d,%d,%d,%d,%d,%d,%d\n",
	 superstruct.s_blocks_count,
	 superstruct.s_inodes_per_group,
	 groupstruct.bg_free_blocks_count,
	 groupstruct.bg_free_inodes_count,
	 groupstruct.bg_block_bitmap,
	 groupstruct.bg_inode_bitmap,
	 groupstruct.bg_inode_table);
}


//function for free block entries. This will be called from group_summary()
void free_block_entries(void) {
  unsigned int index, i, j;
  int occupied;
  index = superstruct.s_first_data_block;
  char* buf = malloc(bsize);
  char byte;
  size_t offset = SUPERSIZEME + (groupstruct.bg_block_bitmap - 1) * bsize;
  pread(fsfd, buf, bsize, offset);
  for(i = 0; i < bsize; i++) {
    byte = buf[i];
    for(j = 0; j < 8; j++) {
      occupied = byte & 1;
      byte = byte >> 1;
      if(!occupied)
        printf("BFREE,%d\n", index);
      index++;
    }
  }
  free(buf);
}

void inode_entries(void) {  
  unsigned int index, i, j;
  int occupied;
  index = 1;
  char* buf = malloc(isize);
  char byte;
  size_t offset = SUPERSIZEME + bsize * (groupstruct.bg_inode_bitmap - 1);
  pread(fsfd, buf, isize, offset);
  for(i = 0; i < isize; i++) {
    byte = buf[i];
    for(j = 0; j < 8; j++) {
      occupied = byte & 1;
      byte = byte >> 1;
      if(!occupied)
        printf("IFREE,%d\n", index);
      else
	inode_summary(index);
      index++;
    }
  }
  free(buf);
}

void inode_summary(unsigned int ninode) {
  //declarations and read from image fd 
  char filetype;
  unsigned int itable = groupstruct.bg_inode_table;
  size_t offset = ninode * sizeof(inodestruct) + SUPERSIZEME + (itable - 1) * bsize;
  pread(fsfd, &inodestruct, sizeof(inodestruct), offset);
  //check to see if there are file contents
  if(inodestruct.i_links_count == 0 || inodestruct.i_mode == 0)
    return;

  //select file type and output for INODE section
  __u16 typemask = 0xF000;
  __u16 temp = inodestruct.i_mode & typemask;
  switch(temp)
    {
    case 0xa000:
      filetype = 's';
      break;
    case 0x8000:
      filetype = 'f';
      break;
    case 0x4000:
      filetype = 'd';
      break;
    }

  printf("INODE,%d,%c,%o,%d,%d,%d,",
	 ninode + 1,
	 filetype,
	 inodestruct.i_mode & 0xFFF,
	 inodestruct.i_uid,
	 inodestruct.i_gid,
	 inodestruct.i_links_count);


  time_helper(); //helper function that returns create, modify, and access 
  printf("%d,%d", inodestruct.i_size, inodestruct.i_blocks);
  unsigned int i;
  for (i = 0; i < 15; i++)
    printf(",%d", inodestruct.i_block[i]);
  printf("\n");

  if (filetype == 'd')
    directory_entries(ninode);

  //check for indirect blocks, call helper if indirect block set
  if(inodestruct.i_block[12] != 0)
    indirect_entry(12, ninode, 12, inodestruct.i_block[12]);

  if(inodestruct.i_block[13] != 0)
    indirect_entry(13, ninode, 268, inodestruct.i_block[13]);

  if(inodestruct.i_block[14] != 0)
    indirect_entry(14, ninode, 65804, inodestruct.i_block[14]);
}

void directory_entries(int ninode) {
  struct ext2_dir_entry dir;
  unsigned int i, j;
  for(i = 0; i < 12; i++) {
    if(inodestruct.i_block[i] != 0) {
      for(j = 0; j < bsize; j+= dir.rec_len) {
	       pread(fsfd, &dir, sizeof(dir), inodestruct.i_block[i]*bsize+j);
	      if (dir.inode != 0) {
	        printf("DIRENT,%d,%d,%u,%u,%u,'%s'\n",
        		ninode + 1,
      		  j,
        		dir.inode,
        		dir.rec_len,
        		dir.name_len,
        		dir.name);
        }
      }
    }
  }
}

//function for printing indirect inode entries
void indirect_entry(int ndepth, int ninode, int baseoffset, __u32 bnum) {
  __u32 blocks[bsize/4];
  __u32 nblocks = bsize / sizeof(__u32);
  size_t offset = SUPERSIZEME + bsize * (bnum - 1);
  pread(fsfd, blocks, bsize, offset);
  unsigned int i;
  for(i = 0; i < nblocks; i++) {
    if(blocks[i] != 0) {
      printf("INDIRECT,%d,%d,%d,%u,%u\n",
	     ninode + 1,
	     ndepth - 11,
	     baseoffset + i,
	     bnum,
	     blocks[i]);
      
      //recursively call the function if we are at a double or triple indirect inode
      if(ndepth == 14)
        indirect_entry(ndepth - 1, ninode, baseoffset, blocks[i]);
      if(ndepth == 13)
        indirect_entry(ndepth - 1, ninode, baseoffset, blocks[i]);
    }
  }
}



int main(int argc, char* argv[]) {
  //check for "bogus" arguments in the sanity check
  struct option options[] = {
    {0, 0, 0, 0}
  };
  int c = getopt_long(argc, argv, "", options, NULL);
  if (c != -1) {
    fprintf(stderr, "Invalid arguments. Correct usage: ./lab3a [image]\n");
    exit(1);
  }
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Correct usage: ./lab3a [image]\n");
    exit(1);
  }

  //open file system image
  fsfd = open(*(argv + 1), O_RDONLY);
  if (fsfd < 0)
    error("Error calling open.");

  //call functions for specified output mentioned in spec
  superblock_summary();
  group_summary();
  free_block_entries();
  inode_entries(); //returns free, used, directory, and indirect inode information
  exit(0);

}
