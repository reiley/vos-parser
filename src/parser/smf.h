#ifndef __smf_h__
#define __smf_h__

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
  uint32_t tick; /* tick counter */
  union
  {
    uint8_t byte[4];
    uint32_t data;
  };
} event_t;

typedef struct
{
  size_t nevents; /* number of events */
  event_t* events; /* pointer to event buffer */
} track_t;

typedef struct
{
  uint32_t status; /* internal status */
  uint16_t format; /* midi file format number */
  uint16_t division; /* division */
  track_t ttempo; /* tempo info track */
  size_t ntracks; /* number of tracks */
  track_t tracks[1]; /* track info list */
} smf_t;

/*
release smf_t structure
this function can handle empty and broken value returned from smf_parser
*/
void smf_free(smf_t* smf);

/*
SMF parser
return value:
  NULL on severe error, otherwise a valid pointer to smf_t structure
fields description:
  status, 0 indicates the midi file is fully parsed, otherwise a partial result
  format, midi file format, always be 0, 1 or 2
  division, midi time division
  ttempo, track that holds sorted set tempo event info
    if no tempo info, ttempo.nevents=0 and ttempo.events=NULL
  ntracks, number of tracks in the midi file
  tracks, a list of midi event track, each track holds sorted midi events
    if the track is empty, track.nevents=0 and track.events=NULL
*/
smf_t* smf_parser(uint8_t* chunk, size_t size);

#endif /*  __smf_h__ */
