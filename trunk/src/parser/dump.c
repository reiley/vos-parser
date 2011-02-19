#include "dump.h"
#include "dump.inl"

void conv_dump(conv_t* conv)
{
  size_t i;
  if(conv->notes)
  {
    printf("%d notes\n",conv->nnotes);
    for(i=0;i<conv->nnotes;i++)
      if(conv->notes)
      {
        printf("%05d %07d-%07d [%05d]: channel %d, note %d, velocity %d, bar %d, key %d\n",
          i,
          conv->notes[i].begin,
          conv->notes[i].end,
          conv->notes[i].next?conv->notes[i].next-conv->notes:0,
          conv->notes[i].byte[0]&0x0f,
          conv->notes[i].byte[1],
          conv->notes[i].byte[2],
          conv->notes[i].byte[3]>>7,
          conv->notes[i].byte[3]&0x7f);
      }
  }
  else
    printf("empty conv\n");
}

void mix_dump(mix_t* mix)
{
  track_t track;
  track.nevents=mix->nevents;
  track.events=mix->events;
  smf_dump_track(&track);
}

void smf_dump_byte(uint8_t* byte)
{
  if((byte[0]&0xf0)<0x80||(byte[0]&0xf0)>0xe0||byte[1]>127)
  {
    printf("Error: unknown event {0x%02x 0x%02x 0x%02x 0x%02x}\n",byte[0],byte[1],byte[2],byte[3]);
    return;
  }
  switch(byte[0]&0xf0)
  {
    case 0x80:
      printf("Note Off [%d]: note %d, velocity %d\n",byte[0]&0x0f,byte[1],byte[2]);
      break;
    case 0x90:
      printf("Note On [%d]: note %d, velocity %d\n",byte[0]&0x0f,byte[1],byte[2]);
      break;
    case 0xa0:
      printf("Polyphonic [%d]: note %d, pressure %d\n",byte[0]&0x0f,byte[1],byte[2]);
      break;
    case 0xb0:
      printf("Controller [%d]: %s #%d, value %d\n",byte[0]&0x0f,smf_control[byte[1]],byte[1],byte[2]);
      break;
    case 0xc0:
      printf("Instrument [%d]: %s #%d\n",byte[0]&0x0f,smf_instrument[byte[1]],byte[1]);
      break;
    case 0xd0:
      printf("Aftertouch [%d]: pressure %d\n",byte[0]&0x0f,byte[1]);
      break;
    case 0xe0:
      printf("Pitchwheel [%d]: LSB %d, MSB %d\n",byte[0]&0x0f,byte[1],byte[2]);
      break;
  }
}

void smf_dump_event(event_t* event)
{
  printf("%07d: ",event->tick);
  smf_dump_byte(event->byte);
}

void smf_dump_track(track_t* track)
{
  size_t i;
  printf("%d events\n",track->nevents);
  for(i=0;i<track->nevents;i++)
  {
    printf("%05d ",i);
    smf_dump_event(track->events+i);
  }
}

void smf_dump(smf_t* smf)
{
  size_t i;
  if(smf)
  {
    if(smf->division&0x8000)
      printf("MThd: format %d, SMPTE fps %d * tpf %d, %d tracks\n",smf->format,smf->division>>8&0x7f,smf->division&0xff,smf->ntracks);
    else
      printf("MThd: format %d, division %d, %d tracks\n",smf->format,smf->division,smf->ntracks);
  }
  else
  {
    printf("Error: empty midi file\n");
    return;
  }
  if(smf->ttempo.events)
  {
    printf("Tempo: %d meta events\n",smf->ttempo.nevents);
    for(i=0;i<smf->ttempo.nevents;i++)
    {
      printf("%05d ",i);
      printf("%07d: Set Tempo %d\n",smf->ttempo.events[i].tick,smf->ttempo.events[i].data);
    }
  }
  for(i=0;i<smf->ntracks;i++)
  {
    if(smf->tracks[i].events)
    {
      printf("MTrk %d: ",i);
      smf_dump_track(smf->tracks+i);
    }
    else
      printf("MTrk %d: empty track\n",i);
  }
}

void vos_dump_note(note_t* note)
{
  printf("%07d-%07d: channel %d, note %d, velocity %d, bar %d, key %d\n",
    note->begin,
    note->end,
    note->byte[0]&0x0f,
    note->byte[1],
    note->byte[2],
    note->byte[3]>>7,
    note->byte[3]&0x7f);
}

void vos_dump_channel(channel_t* channel)
{
  size_t i;
  if(channel->notes)
  {
    printf("%d notes\n",channel->nnotes);
    for(i=0;i<channel->nnotes;i++)
      if(channel->notes)
      {
        printf("%05d ",i);
        vos_dump_note(channel->notes+i);
      }
  }
  else
    printf("empty channel\n");
}

void vos_dump(vos_t* vos)
{
  size_t i,j;
  if(vos)
    printf("%s: %d channels\n",vos->format?"CAN":"VOS",vos->nchannels);
  else
  {
    printf("Error: empty vos file\n");
    return;
  }
  for(i=0;i<vos->nchannels;i++)
  {
    printf("Channel %d: ",i);
    vos_dump_channel(vos->channels+i);
  }
  if(vos->smf)
    smf_dump(vos->smf);
}
