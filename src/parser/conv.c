#include "conv.h"

conv_t* vos_conv(mix_t* mix)
{
  typedef struct
  {
    notex_t* note;
    size_t count;
  } board_t;
  board_t *board;
  conv_t* retval=NULL;
  size_t nevent;
  if(!mix)
    return retval;
  if(!(retval=(conv_t*)malloc(sizeof(conv_t)-sizeof(notex_t)+sizeof(notex_t)*mix->nevents)))
    return retval; /* not enough memory */
  if(!(board=(board_t*)calloc(128*16,sizeof(board_t)))) /* allocate a tidy board */
    return retval; /* not enough memory */
  retval->nnotes=0;
  for(nevent=0;nevent<mix->nevents;nevent++)
    if((mix->events[nevent].byte[0]&0xe0)==0x80) /* key attack/release */
    {
      board_t* chalk=board+(mix->events[nevent].byte[0]&0x0f)*128+mix->events[nevent].byte[1];
      if((mix->events[nevent].byte[0]&0xf0)==0x90&&mix->events[nevent].byte[2]) /* key attack */
      {
        if(!chalk->count)
          chalk->note=retval->notes+retval->nnotes;
        retval->notes[retval->nnotes].begin=mix->events[nevent].tick;
        retval->notes[retval->nnotes].end=0;
        retval->notes[retval->nnotes].data=mix->events[nevent].data;
        chalk->count++;
        retval->nnotes++;
      }
      else /* key release */
      {
        if(!chalk->count)
          continue; /* cannot coerce */
        chalk->count--;
        if(chalk->count)
        {
          notex_t* tmp;
          for(tmp=retval->notes+retval->nnotes-1;tmp>=retval->notes;tmp--)
            if(tmp->byte[1]==mix->events[nevent].byte[1]&& /* same note */
               (tmp->byte[0]&0x0f)==(mix->events[nevent].byte[0]&0x0f)&& /* same channel */
               !tmp->end) /* not coerced */
            {
              tmp->end=mix->events[nevent].tick;
              break;
            }
        }
        else
          chalk->note->end=mix->events[nevent].tick;
      }
    }
  { /* filter out broken notes */
    size_t i,j;
    for(i=0;i<retval->nnotes;i++) /* check for broken note */
      if(retval->notes[i].begin>=retval->notes[i].end)
        break;
    for(j=i+1;j<retval->nnotes;j++) /* remove broken note */
      if(retval->notes[j].begin<retval->notes[j].end)
        retval->notes[i++]=retval->notes[j];
    retval->nnotes=i; /* adjust number of notes */
  }
  { /* fix overlapped notes */
    size_t nnote=retval->nnotes;
    memset(board,0,sizeof(board_t)*128);
    while(nnote)
    {
      size_t key=retval->notes[--nnote].byte[3]&0x7f;
      if(board[key].note)
      {
        retval->notes[nnote].next=board[key].note;
        if(retval->notes[nnote].byte[3]&0x7f&& /* key */
           retval->notes[nnote].byte[3]&0x80&& /* bar */
           retval->notes[nnote].end>retval->notes[nnote].next->begin) /* overlapped */
          retval->notes[nnote].byte[3]&=0x7f; /* clear bar flag */
      }
      else
        retval->notes[nnote].next=NULL;
      board[key].note=retval->notes+nnote;
      board[key].count++;
    }
  }
  { /* under construction... */
    size_t nkey=0;
    size_t nnote=0;
    size_t key;
    for(key=1;key<128;key++)
      if(board[key].note)
      {
        board[nkey].note=board[key].note;
        board[nkey].count=0;
        nnote+=board[key].count;
		nkey++;
      }
  }
  free(board);
  retval=(conv_t*)realloc(retval,sizeof(conv_t)-sizeof(notex_t)+sizeof(notex_t)*retval->nnotes); /* realloc and release unused memory */
  return retval;
}

mix_t* vos_bake(conv_t* conv)
{
  mix_t* retval=NULL;
  size_t* board;
  size_t* count;
  size_t* thread;
  if(!conv)
    return retval;
  if(!(retval=(mix_t*)malloc(sizeof(mix_t)-sizeof(event_t)+sizeof(event_t)*conv->nnotes*2)))
    return retval; /* not enough memory */
  retval->nevents=0;
  if(!(board=(size_t*)malloc(sizeof(size_t)*(256+conv->nnotes))))
    return retval; /* not enough memory */
  memset(board,0,sizeof(size_t)*256); /* clear board */
  count=board+128;
  thread=board+256;
  { /* create thread */
    size_t nnote=conv->nnotes;
    while(nnote)
    {
      size_t key=conv->notes[--nnote].byte[3]&0x7f;
      thread[nnote]=board[key]?board[key]:0;
      board[key]=nnote;
      count[key]++;
    }
  }
  {
    size_t key;
    size_t nkey=0; /* number of activate keys */
    for(key=1;key<128;key++)
      if(count[key])
      {
        uint32_t last;
        event_t* needle;
        size_t index=board[key];
        board[nkey]=nkey?count[nkey-1]:conv->nnotes;
        needle=retval->events+board[nkey];
        needle->tick=0;
        needle->data=conv->notes[index].data;
        needle->byte[0]&=0x0f;
        needle->byte[3]&=0x7f;
        last=conv->notes[index].byte[3]&0x80?conv->notes[index].end:conv->notes[index].begin;
        while(index=thread[index])
        {
          if(needle->byte[0]!=conv->notes[index].byte[0]|| /* channel */
             needle->byte[1]!=conv->notes[index].byte[1]|| /* note */
             needle->byte[2]!=conv->notes[index].byte[2]) /* velocity */
          {
            needle++;
            needle->tick=(last+conv->notes[index].begin)/2;
            needle->data=conv->notes[index].data;
            needle->byte[0]&=0x0f;
            needle->byte[3]&=0x7f;
            last=conv->notes[index].byte[3]&0x80?conv->notes[index].end:conv->notes[index].begin;
          }
        }
        count[nkey]=needle-retval->events+1;
        nkey++;
      }
    while(nkey) /* merge sort */
    {
      size_t minkey=0;
      for(key=1;key<nkey;key++)
        if(retval->events[board[key]].tick<retval->events[board[minkey]].tick)
          minkey=key;
      retval->events[retval->nevents++]=retval->events[board[minkey]];
      board[minkey]++;
      if(board[minkey]>=count[minkey])
      {
        do
        {
          board[minkey]=board[minkey+1];
          count[minkey]=count[minkey+1];
          minkey++;
        } while(minkey<nkey);
        nkey--;
      }
    }
  }
  free(board);
  retval=(mix_t*)realloc(retval,sizeof(mix_t)-sizeof(event_t)+sizeof(event_t)*retval->nevents);
  return retval;
}
