#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>

#include "../parser/rmi.h"
#include "../parser/smf.h"
#include "../parser/vos.h"
#include "../parser/mix.h"

mix_t* synth(uint8_t* buffer, size_t size)
{
    mix_t* retval=NULL;
    vos_t* vos;
    smf_t* smf;
    if(vos=vos_parser(buffer,size))
    {
      smf=vos2smf(vos);
      if(!smf||smf->status)
        vos_free(vos);
      else
        ; /* vos destructed in vos2smf automatically */
    }
    else if(!(smf=smf_parser(buffer,size)))
      smf=rmi_parser(buffer,size);
    if(!smf)
      return retval; /* mix failed */
    retval=smf_mix(smf);
    smf_free(smf);
    return retval;
}

void play(UINT_PTR port, mix_t* mix)
{
  HMIDIOUT midi_device;
  long time_begin;
  long time_current;
  midiOutOpen(&midi_device, port, 0, 0, CALLBACK_NULL);
  time_begin=timeGetTime();
  size_t i=0;
  while(i<mix->nevents)
  {
    do
    {
      time_current=timeGetTime();
      Sleep(1);
    } while((time_current-time_begin)<mix->events[i].tick);
    do
    {
      midiOutShortMsg(midi_device,mix->events[i++].data);
    } while(i<mix->nevents&&(time_current-time_begin)>=mix->events[i].tick);
  }
  midiOutReset(midi_device);
  midiOutClose(midi_device);
}

int main(int argc, char* argv[])
{
  int retval=1;
  if(argc>2)
  {
    int fd;
    struct stat file_stat;
    uint8_t* buffer;
    mix_t* mix;
    if((fd=open(argv[2],O_RDONLY|O_BINARY))==-1)
      return retval; /* cannot open file */
    fstat(fd,&file_stat);
    if(!(buffer=(uint8_t*)malloc(file_stat.st_size)))
    {
      close(fd);
      return retval;
    }
    read(fd,buffer,file_stat.st_size);
    close(fd);
    mix=synth(buffer,file_stat.st_size);
    free(buffer);
    if(mix)
    {
      retval=0;
      play(atoi(argv[1]), mix);
      free(mix);
    }
  }
  return retval;
}
