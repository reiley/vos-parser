#ifndef __conv_h__
#define __conv_h__

#include "mix.h"
#include "smf.h"
#include "vos.h"

struct struct_notex_t
{
  uint32_t begin; /* note hit time */
  uint32_t end; /* note release time */
  union
  {
    uint8_t byte[4];
    uint32_t data;
  };
  struct struct_notex_t* next;
};

typedef struct struct_notex_t notex_t;

typedef struct
{
  size_t nnotes;
  notex_t notes[1];
} conv_t;

conv_t* vos_conv(mix_t* mix);
mix_t* vos_bake(conv_t* conv);

#endif /*  __conv_h__ */
