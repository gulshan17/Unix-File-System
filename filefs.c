#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

#include <errno.h>

#include <sys/mman.h>

#include "fs.h"

int zerosize(int fd);
void exitusage(char* pname);


int main(int argc, char** argv){
  
  int opt;
  int create = 0;
  int list = 0;
  int add = 0;
  int remove = 0;
  int extract = 0;
  char* toadd = NULL;
  char* toremove = NULL;
  char* toextract = NULL;
  char* fsname = NULL;
  char* data = NULL;
  char *towrite = NULL;
  char *toread = NULL;
  int fd = -1;
  int newfs = 0;
  int filefsname = 0;
  int data_pass = 0;
  int write_flag = 0;
  int read_flag = 0;
 

  
  while ((opt = getopt(argc, argv, "la:r:e:w:f:o:d:")) != -1) {
    switch (opt) {
    case 'l':
      list = 1;
      break;
    case 'a':
      add = 1;
      toadd = strtok(strdup(optarg), "\r\t\n ");
      break;
    case 'r':
      remove = 1;
      toremove = strtok(strdup(optarg), "\r\t\n ");
      break;
    case 'f':
      filefsname = 1;
      fsname = strtok(strdup(optarg), "\r\t\n ");
      break;
    case 'w':
      write_flag = 1;
      towrite = strtok(strdup(optarg), "\r\t\n ");
      break;
    case 'd':
      data_pass = 1;
      data = strtok(strdup(optarg), "\r\t\n ");
      break;
    case 'o':
      read_flag = 1;
      toread = strtok(strdup(optarg), "\r\t\n ");
      break;
    default:
      exitusage(argv[0]);
    }
  }
  
  
  if (!filefsname){
    exitusage(argv[0]);
  }

  if ((fd = open(fsname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1){
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  else{
    if (zerosize(fd)){
      newfs = 1;
    }
    
    if (newfs)
      if (lseek(fd, FSSIZE-1, SEEK_SET) == -1){
	perror("seek failed");
	exit(EXIT_FAILURE);
      }
      else{
	if(write(fd, "\0", 1) == -1){
	  perror("write failed");
	  exit(EXIT_FAILURE);
	}
      }
  }
  

  mapfs(fd);
  
  if (newfs){
    formatfs(fsname);
  }

  if (add){
    addfilefs(toadd);
  }

  if (remove){
    removefilefs(toremove);
  }

  if (write_flag) {
    if(data_pass){
      writefilefs(towrite, data);
    } else {
      printf("\n PLEASE PASS DATA with -d flag\n");
    }
  }

  if (read_flag) {
    readfilefs(toread);
  }

  if(list){
    lsfs();
  }

  unmapfs();
  
  return 0;
}


int zerosize(int fd){
  struct stat stats;
  fstat(fd, &stats);
  if(stats.st_size == 0)
    return 1;
  return 0;
}

void exitusage(char* pname){
  fprintf(stderr, "Usage %s [-l] [-a path] [-e path] [-r path] -f name\n", pname);
  exit(EXIT_FAILURE);
}
