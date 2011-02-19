#ifndef __vos_h__
#define __vos_h__

#include "smf.h"

typedef struct
{
  uint32_t begin; /* note hit time */
  uint32_t end; /* note release time */
  union
  {
    uint8_t byte[4];
    uint32_t data;
  };
} note_t;

typedef struct
{
  size_t nnotes; /* number of notes */
  note_t* notes; /* pointer to note buffer */
} channel_t;

typedef struct
{
  uint16_t format; /* vos file format, 0 for vos, 1 for canmusic */
  size_t nchannels; /* number of channels */
  smf_t* smf; /* embedded smf chunk */
  channel_t channels[1]; /* channel info list */
} vos_t;

/*
release vos_t structure
this function can handle empty and broken value returned from vos_parser
*/
void vos_free(vos_t* vos);

/*
destructive converting vos_t to smf_t
return value:
  NULL indicates a severe error
  a pointer to smf_t object will be returned with the input vos_t object destructed on success
fields description:
  smf->status, 0 indicates a success, otherwise a failure while the input vos_t object untouched
  smf->format, unused
  smf->division, midi time division
  smf->ttempo, track that holds sorted Set Tempo event info
    if no tempo info, ttempo.nevents=0 and ttempo.events=NULL
  smf->ntracks, number of tracks
  smf->tracks, a list of midi event track, each track holds sorted midi events
    if the track is empty, track.nevents=0 and track.events=NULL
*/
smf_t* vos2smf(vos_t* vos);

/*
VOS file parser
return value:
  NULL on severe error, otherwise a valid pointer to vos_t structure
fields description:
  format, vos file format, 0 for vos, 1 for canmusic
  nchannels, number of vos channels
  smf, embedded smf chunk, can be NULL on parsing failure
  channels, a list of vos channel, each channel holds vos notes
    if the channel is empty, channel.nnotes=0 and channel.notes=NULL
  smf->status, 0 indicates a success
  smf->format, the original midi format
  smf->division, midi time division
  smf->ttempo, track that holds sorted Set Tempo event info
    if no tempo info, ttempo.nevents=0 and ttempo.events=NULL
  smf->ntracks, number of tracks
  smf->tracks, a list of midi event track, each track holds sorted midi events
    if the track is empty, track.nevents=0 and track.events=NULL
*/
vos_t* vos_parser(uint8_t* chunk, size_t size);

#endif /*  __vos_h__ */
