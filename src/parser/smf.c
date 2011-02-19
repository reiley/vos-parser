#include "smf.h"

/*
<midi file> ::= <mthd> <mtrk>+
<mthd> ::= "MThd" <length of header data: MSB32> <format: MSB16> <ntrks: MSB16> <division: MSB16>
  # <length of header data: MSB32> should be 6
  # <format: MSB16> must be 0, 1 or 2
  # <division: MSB16> can be { 0 <tpb: 15 bits> } or { 1 <SMPTE fps: 7 bits> <tpf: 8 bits> }
  # <SMPTE fps: 7 bits> can be 24, 25, 29 (for 29.97, 30*1000/1001) or 30 (almost obsolete) fps
<mtrk> ::= "MTrk" <length of track data: MSB32> <track data>
<track data> ::= (<delta time> <event>)+
<delta time> ::= <midi var length>
<event> ::= <midi event> | <sysex event> | <meta event>
<midi event> ::= <normal event> | <running status event>
<normal event> ::= <event control byte> <event data bytes>
<event control byte> ::= { <event type: 4 bits> <channel: 4 bits> }
  # midi event type
  # 1000: note off
  # 1001: note on
  # 1010: note aftertouch
  # 1011: controller
  # 1100: program change
  # 1101: channel aftertouch
  # 1110: pitch bend
<running status event> ::= <event data bytes>
  # <running status event> uses the last midi event control byte in the same track
<sysex event> ::= 0xf0 <midi var length> <bytes to be transmitted after 0xf0> | \
                  0xf7 <midi var length> <all bytes to be transmitted>
<meta event> ::= 0xff <type: 1 byte> <midi var length> <data bytes> | \
                 <quarter frame 0xf1> <data bytes: 1 byte> | \
                 <song position 0xf2> <data bytes: 2 bytes> | \
                 <song select 0xf3> <data bytes: 1 byte> | \
                 <tune request 0xf6> | \
                 <timing clock 0xf8> | \
                 <start 0xfa> | \
                 <continue 0xfb> | \
                 <stop 0xfc> | \
                 <active sense 0xfe>
<midi var length> ::= ({ 1 <7 bits> })* { 0 <7 bits> }
  # the value is calculated by concatenating all the <7 bits>
  # the max value is { 0xff, 0xff, 0xff, 0x7f }, which is 268435455
*/

void smf_free(smf_t* smf)
{
  if(smf)
  {
    size_t track;
    for(track=0;track<smf->ntracks;track++)
      if(smf->tracks[track].events)
        free(smf->tracks[track].events);
    if(smf->ttempo.events)
      free(smf->ttempo.events);
    free(smf);
  }
}

smf_t* smf_parser(uint8_t* chunk, size_t size)
{
  event_t event;
  size_t offset=0;
  uint8_t rstatus; /* running status */
  uint16_t track; /* current track number, start from 0 */
  uint32_t trklen; /* track length in bytes */
  smf_t* retval=NULL; /* return value */
  event.byte[3]=0; /* reserved by midi standard*/
  do
  {
    if(offset+8>size)
      return retval; /* buffer overflow */
    if(chunk[offset]!=0x4d||chunk[offset+1]!=0x54||
       chunk[offset+2]!=((offset)?0x72:0x68)||
       chunk[offset+3]!=((offset)?0x6b:0x64)) /* "MTrk", "MThd" */
      return retval; /* invalid header */
    offset+=4;
    { /* inline MSB32 parser */
      trklen=chunk[offset++];
      trklen<<=8;
      trklen+=chunk[offset++];
      trklen<<=8;
      trklen+=chunk[offset++];
      trklen<<=8;
      trklen+=chunk[offset++];
    }
    if(offset!=8) /* parse midi track */
    {
      size_t sentry;
      if(trklen)
        if(!(retval->tracks[track].events=(event_t*)malloc(sizeof(event_t)*trklen/2)))
          return retval; /* not enough memory */
      event.tick=0; /* start tick from 0 */
      rstatus=0; /* sentry for running status */
      sentry=offset+trklen;
      while(offset<sentry)
      {
        uint32_t dt=0;
        do /* parse midi variable length number */
        {
          if(offset>=size)
            return retval; /* buffer overflow */
          dt=(dt<<7)+(chunk[offset]&0x7f);
        } while(chunk[offset++]&0x80);
        event.tick+=dt; /* increase the tick number by delta time */
        if(offset>=size)
          return retval;  /* buffer overflow */
        event.byte[0]=chunk[offset]&0x80?chunk[offset++]:rstatus; /* check running status */
        switch(event.byte[0]&0xf0) /* high nibble: function, low nibble: channel */
        {
        case 0x80: /* note off */
        case 0x90: /* note on */
        case 0xa0: /* note aftertouch */
        case 0xb0: /* controller */
        case 0xe0: /* pitch bend */
          if(offset+2>size)
            return retval; /* buffer overflow */
          event.byte[1]=chunk[offset++];
          event.byte[2]=chunk[offset++];
          retval->tracks[track].events[retval->tracks[track].nevents++]=event;
          rstatus=event.byte[0]; /* update running status */
          break;
        case 0xc0: /* program change */
        case 0xd0: /* channel aftertouch */
          if(offset>=size)
            return retval;  /* buffer overflow */
          event.byte[1]=chunk[offset++];
          event.byte[2]=0;
          retval->tracks[track].events[retval->tracks[track].nevents++]=event;
          rstatus=event.byte[0]; /* update running status */
          break;
        case 0xf0:
          switch(event.byte[0]&0x0f)
          {
          case 0x02: /* song position */
            offset++;
          case 0x01: /* quarter frame */
          case 0x03: /* song select */
            offset++;
          case 0x06: /* tune request */
          case 0x08: /* timing clock */
          case 0x0a: /* start */
          case 0x0b: /* continue */
          case 0x0c: /* stop */
          case 0x0e: /* active sense */
            offset++;
            break;
          case 0x0f: /* meta events */
            if(offset>=size)
              return retval; /* buffer overflow */
            event.byte[1]=chunk[offset++];
          case 0x00: /* normal sysex event */
          case 0x07: /* divided sysex event, EOX */
            {
              uint32_t len=0;
              do
              {
                if(offset>=size)
                  return retval;  /* buffer overflow */
                len=(len<<7)+(chunk[offset]&0x7f);
              } while(chunk[offset++]&0x80);
              if(offset+len>size)
                return retval;  /* buffer overflow */
              /* proceed set tempo meta event */
              if((event.byte[0]==0xff)&&(event.byte[1]==0x51))
              { /* inline MSB24 parser */
                event.data=chunk[offset];
                event.data<<=8;
                event.data+=chunk[offset+1];
                event.data<<=8;
                event.data+=chunk[offset+2];
                if(retval->ttempo.nevents>=retval->status)
                {
                  event_t* tmp;
                  retval->status<<=1;
                  tmp=(event_t*)realloc(retval->ttempo.events,sizeof(event_t)*retval->status);
                  if(!tmp)
                    return retval; /* not enough memory */
                  retval->ttempo.events=tmp;
                }
                { /* insertion sort, inplace, and O(n) if list is almost sorted */
                  size_t i;
                  for(i=retval->ttempo.nevents;i&&retval->ttempo.events[i-1].tick>event.tick;i--)
                    retval->ttempo.events[i]=retval->ttempo.events[i-1];
                  retval->ttempo.events[i]=event;
                }
                retval->ttempo.nevents++;
                event.data=0; /* clear the reserved byte */
              }
              offset+=len;
            }
            break;
          default:
            return retval; /* unsupported event */
          }
          break;
        }
      }
      offset=sentry;
      if(retval->tracks[track].nevents)
      { /* realloc and release unused memory */
        event_t* tmp=(event_t*)realloc(retval->tracks[track].events,sizeof(event_t)*retval->tracks[track].nevents);
        if(tmp)
          retval->tracks[track].events=tmp;
      }
      else
      { /* free the unused memory */
        free(retval->tracks[track].events);
        retval->tracks[track].events=NULL;
      }
      track++;
    }
    else /* parse midi header */
    {
      if(offset+6>size)
        return retval;  /* buffer overflow */
      if(chunk[offset++])
        return retval; /* midi type error */
      if((event.byte[1]=chunk[offset++])>2)
        return retval; /* midi type must be 0, 1, or 2 */
      { /* inline MSB16 parser */
        track=chunk[offset++];
        track<<=8;
        track+=chunk[offset++];
      }
      if(!(retval=(smf_t*)malloc(sizeof(smf_t)-sizeof(track_t)+sizeof(track_t)*track)))
        return retval; /* not enough memory */
      retval->format=event.byte[1];
      retval->division=chunk[offset++];
      retval->division<<=8;
      retval->division+=chunk[offset++];
      for(retval->ntracks=0;retval->ntracks<track;retval->ntracks++)
      {
        retval->tracks[retval->ntracks].nevents=0;
        retval->tracks[retval->ntracks].events=NULL;
      }
      offset+=trklen-6; /* adjust offset for future support */
      track=0;
      retval->status=128; /* initial buffer capacity for tempo events */
      if(!(retval->ttempo.events=(event_t*)malloc(sizeof(event_t)*retval->status)))
        return retval; /* not enough memory */
      retval->ttempo.nevents=0;
    }
  } while(track<retval->ntracks);
  if(retval->ttempo.nevents)
  { /* realloc and release unused memory */
    event_t* tmp=(event_t*)realloc(retval->ttempo.events,sizeof(event_t)*retval->ttempo.nevents);
    if(tmp)
      retval->ttempo.events=tmp;
  }
  else
  { /* free the unused memory */
    free(retval->ttempo.events);
    retval->ttempo.events=NULL;
  }
  if(retval->division&0x8000?
     retval->division&0x7f00&&retval->division&0x00ff: /* SMPTE fps tpf */
     retval->division&0x7fff) /* tpb */
    retval->status=0;
  return retval; /* succeeded */
}
