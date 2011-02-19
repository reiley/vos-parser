#include "vos.h"

/*
<vos file> ::= <vos header> <magic: 1022 bytes> <vos channel>*17 <midi file>
<vos header> ::= { 0x03 0x00 0x00 0x00 0x40 0x00 0x00 0x00 0x69 0x6E 0x66 0x00 } \
                 <magic: 12 bytes> <midi offset: LSB32> <magic: 16 bytes> <file size: LSB32> \
                 ((<magic: 16 bytes> <vos info>) | (<magic: 86 bytes> <vos1 info>))
<vos info> ::= <meta: title> <meta: composer> <meta: ext genre> <meta: voser> <genre: LSB16> <total time: LSB32> <level: LSB16>
<vos1 info> ::= <meta: unknown> <vos info>
  # vos meta <meta length: 1 byte> <meta data bytes>
<vos channel> ::= <instrument: LSB32> <nnotes: LSB32> <magic: 14 bytes> (<note: 13 bytes>)*
<note: 13 bytes> ::= <tick: LSB32> <duration: LSB32> <midi event: 3 bytes> <vos key: 1 byte> <vos bar: 1 byte>
  # <vos key: 1 byte> 0x8?~0xE? for key {1,2,3,4,5,6,7}
  # <vos bar: 1 byte> bar for none-zero value

<canmusic file> ::= <nsubchunks: LSB32> (<subchunk>)+
<subchunk> ::= <subchunk name length: LSB32> <subchunk name> <subchunk size: LSB32> <subchunk content>
<subchunk content> ::= <midi file> | <canmusic chunk> | <other chunk>
<canmusic chunk> ::= "VOS0" <subtype: 2 bytes> \
                     <meta: title> <meta: composer> <meta: ext genre> <meta: voser> <genre: LSB16> \
                     <canmusic genre: 1 byte> <magic: 15 bytes> \
                     <total time: LSB32> <magic: 1022 bytes> \
                     <nchannels: LSB32> <magic: 4 bytes> \
                     (<magic: 1 byte> <instrument: LSB32>)+
                     <level: LSB16> <meta: unknown> \
                     (<nnotes: LSB32> (<note: 16 bytes>)*
                     <magic: 4 bytes>
                     <nnotes: LSB32> (<note: 6 bytes>)*
  # canmusic meta <meta length: LSB16> <meta data bytes>
<note: 16 bytes> ::= "0" <tick: LSB32> <distorted midi event: 3 bytes> <magic: 2 bytes> <canmusic bar: 1 byte> <duration: LSB32> <play: 1 byte>
<distorted midi event: 3 bytes> ::= <noteon first byte> {0 0 0 0 <midi control event channel: 4 bits>} <noteon second byte>
  # <canmusic bar: 1 byte> bar for none-zero value
  # <play: 1 byte> play for none-zero value
<note: 6 bytes> ::= <channel: 1 byte> <note index: LSB32> <canmusic key: 1 byte>
  # <canmusic key: 1 byte> 0x?0~0x?6 for key {1,2,3,4,5,6,7}
*/

void vos_free(vos_t* vos)
{
  if(vos)
  {
    size_t channel;
    for(channel=0;channel<vos->nchannels;channel++)
      if(vos->channels[channel].notes)
        free(vos->channels[channel].notes);
    if(vos->smf)
      smf_free(vos->smf);
    free(vos);
  }
}

smf_t* vos2smf(vos_t* vos)
{
  smf_t* retval=NULL;
  size_t ntrack;
  size_t nevent;
  size_t nchannel;
  if(!vos||!vos->smf||vos->smf->status)
    return retval; /* corrupted vos file */
  nchannel=vos->format?vos->nchannels:vos->nchannels-1; /* remove redundant channel for vos format */
  if(!(retval=(smf_t*)malloc(sizeof(smf_t)-sizeof(track_t)+sizeof(track_t)*(nchannel*2+vos->smf->ntracks))))
    return retval; /* not enough memory */
  retval->status=1;
  retval->ttempo.nevents=0;
  retval->ttempo.events=NULL;
  for(retval->ntracks=0;retval->ntracks<vos->smf->ntracks;retval->ntracks++)
  {
    retval->tracks[retval->ntracks].nevents=0;
    retval->tracks[retval->ntracks].events=NULL;
  }
  for(retval->ntracks=vos->smf->ntracks;retval->ntracks<nchannel*2+vos->smf->ntracks;retval->ntracks++)
  {
    retval->tracks[retval->ntracks].nevents=0;
    nevent=vos->channels[(retval->ntracks-vos->smf->ntracks)/2].nnotes;
    if(!nevent)
      retval->tracks[retval->ntracks].events=NULL;
    else if(!(retval->tracks[retval->ntracks].events=(event_t*)malloc(sizeof(event_t)*nevent)))
      return retval;
  }
  retval->division=vos->smf->division;
  retval->ttempo=vos->smf->ttempo;
  for(ntrack=0;ntrack<vos->smf->ntracks;ntrack++)
    retval->tracks[ntrack]=vos->smf->tracks[ntrack];
  for(ntrack=0;ntrack<nchannel;ntrack++)
  {
    track_t* ontrk=retval->tracks+vos->smf->ntracks+ntrack*2;
    track_t* offtrk=retval->tracks+vos->smf->ntracks+ntrack*2+1;
    ontrk->nevents=offtrk->nevents=vos->channels[ntrack].nnotes;
    for(nevent=0;nevent<vos->channels[ntrack].nnotes;nevent++)
    {
      note_t* note=vos->channels[ntrack].notes+nevent;
      ontrk->events[nevent].tick=note->begin*retval->division/768;
      ontrk->events[nevent].data=note->data;
      /* ontrk->events[nevent].byte[3]=0; */
      offtrk->events[nevent].tick=note->end*retval->division/768;
      offtrk->events[nevent].data=note->data;
      offtrk->events[nevent].byte[2]=0;
      /* offtrk->events[nevent].byte[3]=0; */
    }
    { /* insertion sort */
      size_t i;
      event_t event;
      for(nevent=1;nevent<vos->channels[ntrack].nnotes;nevent++)
      {
        event=offtrk->events[nevent];
        for(i=nevent;i&&event.tick<offtrk->events[i-1].tick;i--)
           offtrk->events[i]=offtrk->events[i-1];
        offtrk->events[i]=event;
      }
    }
  }
  vos->smf->ttempo.events=NULL;
  vos->smf->ntracks=0;
  vos_free(vos);
  retval->status=0;
  return retval;
}

vos_t* vos_parser(uint8_t* chunk, size_t size)
{
  uint8_t vos_header[]={0x3,0x0,0x0,0x0,0x40,0x0,0x0,0x0,0x69,0x6e,0x66,0x0};
  size_t offset=0;
  uint32_t sub_chuck; /* subchunk info */
  uint32_t chunk_length; /* subchunk size */
  uint16_t channel; /* current channel number, start from 0 */
  vos_t* retval=NULL; /* return value */
  if(sizeof(vos_header)>size)
    return retval; /* buffer overflow */
  if(!memcmp(chunk,vos_header,sizeof(vos_header))) /* vos file */
  {
    offset=0x18;
    { /* LSB32 parser, midi file offset */
      if(offset+4>size)
        return retval; /* buffer overflow */
      sub_chuck=chunk[offset+3];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset+2];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset+1];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset];
      offset+=4;
    }
    offset=0x2c;
    { /* LSB32 parser, vos file size */
      if(offset+4>size)
        return retval; /* buffer overflow */
      chunk_length=chunk[offset+3];
      chunk_length<<=8;
      chunk_length+=chunk[offset+2];
      chunk_length<<=8;
      chunk_length+=chunk[offset+1];
      chunk_length<<=8;
      chunk_length+=chunk[offset];
      offset+=4;
      if(chunk_length!=size)
        return retval; /* incorrect file size */
    }
    channel=17; /* vos has constant 17 channels (the last channel is redundant) */
    offset=0x40; /* begin reading vos meta info from 0x40 */
    if(!(retval=(vos_t*)malloc(sizeof(vos_t)-sizeof(channel_t)+sizeof(channel_t)*channel)))
      return retval; /* not enough memory */
    retval->format=0; /* vos file */
    retval->smf=NULL;
    for(retval->nchannels=0;retval->nchannels<channel;retval->nchannels++)
    {
      retval->channels[retval->nchannels].nnotes=0;
      retval->channels[retval->nchannels].notes=NULL;
    }
    if(offset+4>size)
      return retval; /* buffer overflow */
    if(chunk[offset]==0x56&&chunk[offset+1]==0x4f&&
       chunk[offset+2]==0x53&&chunk[offset+3]==0x31) /* "VOS1" */
    {
      offset=0x86; /* adjust offset for vos rank (VOS1) file */
      if(offset>=size)
        return retval; /* buffer overflow */
      offset+=chunk[offset]+1; /* skip the unknown meta data */
    }
    if(offset>=size)
      return retval; /* buffer overflow */
    offset+=chunk[offset]+1; /* vos meta, title */
    if(offset>=size)
      return retval; /* buffer overflow */
    offset+=chunk[offset]+1; /* vos meta, composer */
    if(offset>=size)
      return retval; /* buffer overflow */
    offset+=chunk[offset]+1; /* vos meta, ext genre */
    if(offset>=size)
      return retval; /* buffer overflow */
    offset+=chunk[offset]+1; /* vos meta, voser */
    { /* vos meta, genre */
      uint16_t genre;
      if(offset+2>size)
        return retval; /* buffer overflow */
      genre=chunk[offset+1];
      genre<<=8;
      genre+=chunk[offset];
      offset+=2;
    }
    { /* parse vos time length (ms) */
      uint32_t vos_length;
      if(offset+4>size)
        return retval; /* buffer overflow */
      vos_length=chunk[offset+3];
      vos_length<<=8;
      vos_length+=chunk[offset+2];
      vos_length<<=8;
      vos_length+=chunk[offset+1];
      vos_length<<=8;
      vos_length+=chunk[offset];
      offset+=4;
    }
    { /* vos meta, genre */
      uint16_t level;
      if(offset+2>size)
        return retval; /* buffer overflow */
      level=chunk[offset+1];
      level<<=8;
      level+=chunk[offset];
      offset+=2;
    }
    offset+=1022; /* skip the 1022 magic bytes */
    for(channel=0;channel<retval->nchannels;channel++)
    {
      size_t nnotes;
      { /* vos instrument */
        uint32_t vos_instrument;
        if(offset+4>size)
          return retval; /* buffer overflow */
        vos_instrument=chunk[offset+3];
        vos_instrument<<=8;
        vos_instrument+=chunk[offset+2];
        vos_instrument<<=8;
        vos_instrument+=chunk[offset+1];
        vos_instrument<<=8;
        vos_instrument+=chunk[offset];
        offset+=4;
      }
      { /* number of notes */
        if(offset+4>size)
          return retval; /* buffer overflow */
        nnotes=chunk[offset+3];
        nnotes<<=8;
        nnotes+=chunk[offset+2];
        nnotes<<=8;
        nnotes+=chunk[offset+1];
        nnotes<<=8;
        nnotes+=chunk[offset];
        offset+=4;
      }
      offset+=14; /* skip 14 magic bytes */
      if(nnotes)
      {
        if(offset+nnotes*13>size)
          return retval; /* buffer overflow */
        if(!(retval->channels[channel].notes=(note_t*)malloc(sizeof(note_t)*nnotes)))
          return retval;
        retval->channels[channel].nnotes=nnotes;
        for(nnotes=0;nnotes<retval->channels[channel].nnotes;nnotes++)
        {
          /* note begin */
          retval->channels[channel].notes[nnotes].begin=chunk[offset+3];
          retval->channels[channel].notes[nnotes].begin<<=8;
          retval->channels[channel].notes[nnotes].begin+=chunk[offset+2];
          retval->channels[channel].notes[nnotes].begin<<=8;
          retval->channels[channel].notes[nnotes].begin+=chunk[offset+1];
          retval->channels[channel].notes[nnotes].begin<<=8;
          retval->channels[channel].notes[nnotes].begin+=chunk[offset];
          offset+=4;
          /* note end */
          retval->channels[channel].notes[nnotes].end=chunk[offset+3];
          retval->channels[channel].notes[nnotes].end<<=8;
          retval->channels[channel].notes[nnotes].end+=chunk[offset+2];
          retval->channels[channel].notes[nnotes].end<<=8;
          retval->channels[channel].notes[nnotes].end+=chunk[offset+1];
          retval->channels[channel].notes[nnotes].end<<=8;
          retval->channels[channel].notes[nnotes].end+=chunk[offset];
          retval->channels[channel].notes[nnotes].end+=retval->channels[channel].notes[nnotes].begin;
          offset+=4;
          /* midi event */
          retval->channels[channel].notes[nnotes].byte[0]=chunk[offset++];
          retval->channels[channel].notes[nnotes].byte[1]=chunk[offset++];
          retval->channels[channel].notes[nnotes].byte[2]=chunk[offset++];
          /* reserved byte */
          retval->channels[channel].notes[nnotes].byte[3]=chunk[offset+1]?0x80:0x00;
          if(chunk[offset]&0x80)
            retval->channels[channel].notes[nnotes].byte[3]|=((chunk[offset]&0x70)>>4)+1;
          offset+=2;
        }
      }
    }
    if(offset==sub_chuck)
      retval->smf=smf_parser(chunk+sub_chuck,size-sub_chuck);
  }
  else /* canmusic file */
  {
    { /* number of subchunk */
      if(offset+4>size)
        return retval; /* buffer overflow */
      sub_chuck=chunk[offset+3];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset+2];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset+1];
      sub_chuck<<=8;
      sub_chuck+=chunk[offset];
      offset+=4;
    }
    while(sub_chuck)
    {
      size_t sentry;
      { /* subchunk name */
        if(offset+4>size)
          return retval; /* buffer overflow */
        chunk_length=chunk[offset+3];
        chunk_length<<=8;
        chunk_length+=chunk[offset+2];
        chunk_length<<=8;
        chunk_length+=chunk[offset+1];
        chunk_length<<=8;
        chunk_length+=chunk[offset];
        offset+=4;
        offset+=chunk_length;
      }
      { /* subchunk size */
        if(offset+4>size)
          return retval; /* buffer overflow */
        chunk_length=chunk[offset+3];
        chunk_length<<=8;
        chunk_length+=chunk[offset+2];
        chunk_length<<=8;
        chunk_length+=chunk[offset+1];
        chunk_length<<=8;
        chunk_length+=chunk[offset];
        offset+=4;
      }
      sentry=offset+chunk_length;
      if(sentry>size||offset+4>size)
        return retval; /* buffer overflow */
      if(chunk[offset]==0x56&&chunk[offset+1]==0x4f&&
         chunk[offset+2]==0x53&&chunk[offset+3]==0x30) /* "VOS0" */
      {
        if(retval)
          return retval; /* VOS0 chunk already defined */
        offset+=6; /* "VOS0" XX, XX can be "06", "08", "09", "22", etc. */
        {
          uint16_t meta_len;
          { /* canmusic meta, title */
            if(offset+2>size)
              return retval; /* buffer overflow */
            meta_len=chunk[offset+1];
            meta_len<<=8;
            meta_len+=chunk[offset];
            offset+=2;
            offset+=meta_len;
          }
          { /* canmusic meta, composer */
            if(offset+2>size)
              return retval; /* buffer overflow */
            meta_len=chunk[offset+1];
            meta_len<<=8;
            meta_len+=chunk[offset];
            offset+=2;
            offset+=meta_len;
          }
          { /* canmusic meta, ext genre */
            if(offset+2>size)
              return retval; /* buffer overflow */
            meta_len=chunk[offset+1];
            meta_len<<=8;
            meta_len+=chunk[offset];
            offset+=2;
            offset+=meta_len;
          }
          { /* canmusic meta, voser */
            if(offset+2>size)
              return retval; /* buffer overflow */
            meta_len=chunk[offset+1];
            meta_len<<=8;
            meta_len+=chunk[offset];
            offset+=2;
            offset+=meta_len;
          }
          { /* canmusic meta, genre */
            if(offset+2>size)
              return retval; /* buffer overflow */
            meta_len=chunk[offset+1];
            meta_len<<=8;
            meta_len+=chunk[offset];
            offset+=2;
            offset+=meta_len;
          }
        }
        { /* canmusic genre */
          if(offset>=size)
            return retval; /* buffer overflow */
          chunk[offset];
        }
        offset+=15; /* skip magic bytes */
        { /* parse canmusic time length (ms) */
          uint32_t vos_length;
          if(offset+4>size)
            return retval; /* buffer overflow */
          vos_length=chunk[offset+3];
          vos_length<<=8;
          vos_length+=chunk[offset+2];
          vos_length<<=8;
          vos_length+=chunk[offset+1];
          vos_length<<=8;
          vos_length+=chunk[offset];
          offset+=4;
        }
        offset+=1024; /* skip 1024 magic bytes */
        { /* parse channel number */
          if(offset+4>size)
            return retval; /* buffer overflow */
          if(chunk[offset+3]||chunk[offset+2])
            return retval; /* channel number exceeds 65535 */
          channel=chunk[offset+1];
          channel<<=8;
          channel+=chunk[offset];
          offset+=4;
        }
        offset+=4; /* skip 4 magic bytes */
        if(!(retval=(vos_t*)malloc(sizeof(vos_t)-sizeof(channel_t)+sizeof(channel_t)*channel)))
          return retval; /* not enough memory */
        retval->format=1; /* canmusic file */
        retval->smf=NULL;
        for(retval->nchannels=0;retval->nchannels<channel;retval->nchannels++)
        {
          retval->channels[retval->nchannels].nnotes=0;
          retval->channels[retval->nchannels].notes=NULL;
        }
        for(channel=0;channel<retval->nchannels;channel++)
        { /* canmusic instrument */
          uint32_t vos_instrument;
          offset++; /* skip 1 magic byte */
          if(offset+4>size)
            return retval; /* buffer overflow */
          vos_instrument=chunk[offset+3];
          vos_instrument<<=8;
          vos_instrument+=chunk[offset+2];
          vos_instrument<<=8;
          vos_instrument+=chunk[offset+1];
          vos_instrument<<=8;
          vos_instrument+=chunk[offset];
          offset+=4;
        }
        { /* canmusic level */
          uint16_t level;
          level=chunk[offset+1];
          level<<=8;
          level+=chunk[offset];
          offset+=2;
        }
        offset+=chunk[offset]+6;
        for(channel=0;channel<retval->nchannels;channel++)
        {
          size_t nnotes;
          { /* number of notes */
            if(offset+4>size)
              return retval; /* buffer overflow */
            nnotes=chunk[offset+3];
            nnotes<<=8;
            nnotes+=chunk[offset+2];
            nnotes<<=8;
            nnotes+=chunk[offset+1];
            nnotes<<=8;
            nnotes+=chunk[offset];
            offset+=4;
          }
          if(nnotes)
          {
            if(offset+nnotes*16>size)
              return retval; /* buffer overflow */
            if(!(retval->channels[channel].notes=(note_t*)malloc(sizeof(note_t)*nnotes)))
              return retval;
            retval->channels[channel].nnotes=nnotes;
            for(nnotes=0;nnotes<retval->channels[channel].nnotes;nnotes++)
            {
              offset++;
              retval->channels[channel].notes[nnotes].begin=chunk[offset+3];
              retval->channels[channel].notes[nnotes].begin<<=8;
              retval->channels[channel].notes[nnotes].begin+=chunk[offset+2];
              retval->channels[channel].notes[nnotes].begin<<=8;
              retval->channels[channel].notes[nnotes].begin+=chunk[offset+1];
              retval->channels[channel].notes[nnotes].begin<<=8;
              retval->channels[channel].notes[nnotes].begin+=chunk[offset];
              offset+=4;
              retval->channels[channel].notes[nnotes].byte[1]=chunk[offset++];
              retval->channels[channel].notes[nnotes].byte[0]=chunk[offset++]|0x90;
              retval->channels[channel].notes[nnotes].byte[2]=chunk[offset++];
              offset++;
              offset++;
              retval->channels[channel].notes[nnotes].byte[3]=chunk[offset++]?0x80:0x00;
              retval->channels[channel].notes[nnotes].end=chunk[offset+3];
              retval->channels[channel].notes[nnotes].end<<=8;
              retval->channels[channel].notes[nnotes].end+=chunk[offset+2];
              retval->channels[channel].notes[nnotes].end<<=8;
              retval->channels[channel].notes[nnotes].end+=chunk[offset+1];
              retval->channels[channel].notes[nnotes].end<<=8;
              retval->channels[channel].notes[nnotes].end+=chunk[offset];
              retval->channels[channel].notes[nnotes].end+=retval->channels[channel].notes[nnotes].begin;
              offset+=4;
              offset++;
            }
          }
        }
        offset+=4; /* skip 4 magic bytes */
        {
          size_t nnotes;
          uint32_t index;
          if(offset+4>size)
            return retval; /* buffer overflow */
          nnotes=chunk[offset+3];
          nnotes<<=8;
          nnotes+=chunk[offset+2];
          nnotes<<=8;
          nnotes+=chunk[offset+1];
          nnotes<<=8;
          nnotes+=chunk[offset];
          offset+=4;
          if(offset+nnotes*6>size)
            return retval; /* buffer overflow */
          while(nnotes)
          {
            index=chunk[offset+4];
            index<<=8;
            index+=chunk[offset+3];
            index<<=8;
            index+=chunk[offset+2];
            index<<=8;
            index+=chunk[offset+1];
            if(chunk[offset]<retval->nchannels&&index<retval->channels[chunk[offset]].nnotes)
              retval->channels[chunk[offset]].notes[index].byte[3]|=(chunk[offset+5]&0x0f)+1;
            else
              ; /* ignore illegal key */
            nnotes--;
            offset+=6;
          }
        }
      }
      else if(chunk[offset]==0x4d&&chunk[offset+1]==0x54&&
              chunk[offset+2]==0x68&&chunk[offset+3]==0x64) /* "MThd" */
      {
        if(retval)
          retval->smf=smf_parser(chunk+offset,chunk_length);
      }
      else
      {
        ; /* unknown subchunk */
      }
      offset=sentry;
      sub_chuck--;
    }
  }
  return retval;
}
