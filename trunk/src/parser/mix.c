#include "mix.h"

mix_t* smf_mix(smf_t* smf)
{
  mix_t* retval=NULL;
  smf_t* tmpsmf;
  size_t ntrack;
  uint32_t accumtick=0; /* accumulated tick */
  uint32_t accumtime=0; /* accumulated time */
  uint32_t numerator; /* inaccuracy adjustment */
  uint32_t denominator; /* inaccuracy adjustment */
  uint32_t spb=500; /* 60000000(millisec/min)/120(beat/min)/1000 */
  uint32_t rpb=0; /* 60000000(millisec/min)/120(beat/min)%1000 */
  uint32_t tpb; /* ticks per beat */
  if(!smf||smf->status)
    return retval; /* invalid smf */
  {
    size_t nevents=0;
    for(ntrack=0;ntrack<smf->ntracks;ntrack++)
      nevents+=smf->tracks[ntrack].nevents;
    if(!(retval=(mix_t*)malloc(sizeof(mix_t)-sizeof(event_t)+sizeof(event_t)*nevents)))
      return retval; /* not enough memory */
    retval->nevents=0;
    if(!(tmpsmf=(smf_t*)malloc(sizeof(smf_t)-sizeof(track_t)+sizeof(track_t)*smf->ntracks)))
      return retval; /* not enough memory */
  }
  tmpsmf->ttempo=smf->ttempo;
  tmpsmf->ntracks=0;
  for(ntrack=0;ntrack<smf->ntracks;ntrack++)
    if(smf->tracks[ntrack].nevents)
      tmpsmf->tracks[tmpsmf->ntracks++]=smf->tracks[ntrack];
  if(smf->division&0x8000) /* SMPTE fps*tpf */
  {
    tpb=0;
    numerator=1000;
    denominator=smf->division>>8&0x7f; /* fps, should be 24, 25, 29 (for 29.97) or 30 (almost obsolete) */
    if(denominator==29) /* 30*1000/1001 (29.97 fps) */
      numerator++,denominator++;
    denominator*=smf->division&0xff;
    tmpsmf->ttempo.nevents=0; /* ignore set tempo events */
  }
  else /* tpb */
  {
    tpb=smf->division;
    numerator=0;
    denominator=tpb*1000;
  }
  while(tmpsmf->ntracks)
  {
    size_t mintrack=0;
    uint32_t deltatick;
    for(ntrack=1;ntrack<tmpsmf->ntracks;ntrack++)
      if(tmpsmf->tracks[ntrack].events[0].tick<tmpsmf->tracks[mintrack].events[0].tick)
        mintrack=ntrack;
    while(tmpsmf->ttempo.nevents>0&&tmpsmf->tracks[mintrack].events[0].tick>tmpsmf->ttempo.events[0].tick)
    {
      deltatick=tmpsmf->ttempo.events[0].tick-accumtick;
      numerator+=deltatick*spb%tpb*1000+deltatick*rpb;
      accumtime+=deltatick*spb/tpb+numerator/denominator;
      accumtick=tmpsmf->ttempo.events[0].tick;
      numerator%=denominator;
      spb=tmpsmf->ttempo.events[0].data/1000;
      rpb=tmpsmf->ttempo.events[0].data%1000;
      tmpsmf->ttempo.events++;
      tmpsmf->ttempo.nevents--;
    }
    /* inaccuracy (-1, 0] millisecond */
    deltatick=tmpsmf->tracks[mintrack].events[0].tick-accumtick;
    retval->events[retval->nevents].tick=tpb?accumtime+deltatick*spb/tpb+
                                         (deltatick*rpb+deltatick*spb%tpb*1000+numerator)/denominator:
                                         tmpsmf->tracks[mintrack].events[0].tick*numerator/denominator;
    retval->events[retval->nevents++].data=tmpsmf->tracks[mintrack].events[0].data;
    tmpsmf->tracks[mintrack].events++;
    if(!--tmpsmf->tracks[mintrack].nevents) /* remove empty track */
    {
      tmpsmf->ntracks--;
      for(ntrack=mintrack;ntrack<tmpsmf->ntracks;ntrack++)
        tmpsmf->tracks[ntrack]=tmpsmf->tracks[ntrack+1];
    }
  }
  free(tmpsmf);
  return retval;
}
