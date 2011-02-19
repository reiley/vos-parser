#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#include "../parser/vos.h"
#include "../parser/dump.h"

int main(int argc, char* argv[])
{
  int retval=1;
  int fd;
  struct stat file_stat;
  uint8_t* buffer;
  vos_t* vos;
  if(argc<=1)
    return retval; /* too few args */
  if((fd=open(argv[1],O_RDONLY|O_BINARY))==-1)
    return retval; /* cannot open specified file */
  fstat(fd,&file_stat);
  if(file_stat.st_size>5*1024*1024)
    return retval; /* too big file size */
  if(!(buffer=(uint8_t*)malloc(file_stat.st_size)))
    return retval; /* allocation failed */
  read(fd,buffer,file_stat.st_size);
  close(fd);
  vos=vos_parser(buffer,file_stat.st_size);
  free(buffer);
  if(!vos||!vos->smf||vos->smf->status)
    return retval; /* smf parser error */
  vos_dump(vos);
  vos_free(vos);
  retval=0;
  return retval;
}
