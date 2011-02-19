#ifndef __mix_h__
#define __mix_h__

#include "smf.h"

typedef struct
{
  size_t nevents;
  event_t events[1];
} mix_t;

/*
track mixer
return value:
  NULL on failure, otherwise a valid mix_t pointer value
notes:
  the division value of smf_t should be checked before invocation
  the caller is responsible for releasing the return value (using free)
*/
mix_t* smf_mix(smf_t* smf);

#endif /*  __mix_h__ */
