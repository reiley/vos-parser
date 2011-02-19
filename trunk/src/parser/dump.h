#ifndef __dump_h__
#define __dump_h__

#include <stdio.h>

#include "conv.h"
#include "mix.h"
#include "rmi.h"
#include "smf.h"
#include "vos.h"

void conv_dump(conv_t* conv);
void mix_dump(mix_t* mix);
void smf_dump_byte(uint8_t* byte);
void smf_dump_event(event_t* event);
void smf_dump_track(track_t* track);
void smf_dump(smf_t* smf);
void vos_dump_note(note_t* note);
void vos_dump_channel(channel_t* channel);
void vos_dump(vos_t* vos);

#endif /* __dump_h__ */
