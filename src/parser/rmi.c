#include "rmi.h"

/*
<rmi file> ::= <rmi header> <midi chunk> [<dls chunk>] (<info chunk>)*
  # each chunk begins at an even address, an additional blank byte might be used for alignment
<rmi header> ::= "RIFF" <rmi file size: LSB32>
<midi chunk> ::= "RMID" "data" <midi file size: LSB32> <midi file>
<dls chunk> ::= "RIFF" <dls size> "DLS " <dls data>
<info chunk> ::= (<chunk type: 4 bytes> <info chunk size: LSB32> <info data>) | \
                 ("LIST" <info chunk size: LSB32> "INFO" (<meta type: 4 bytes> <meta data> "\0")+)
*/

smf_t* rmi_parser(uint8_t* chunk, size_t size)
{
  size_t offset;
  uint32_t chunk_length;
  smf_t* retval=NULL; /* return value */
  if(size<20)
    return retval; /* no riff header */
  if(chunk[0x00]!=0x52||chunk[0x01]!=0x49||
     chunk[0x02]!=0x46||chunk[0x03]!=0x46)
    return retval;  /* "RIFF" */
  { /* inline LSB32 parser, rmi file size */
     chunk_length=chunk[0x07];
     chunk_length<<=8;
     chunk_length+=chunk[0x06];
     chunk_length<<=8;
     chunk_length+=chunk[0x05];
     chunk_length<<=8;
     chunk_length+=chunk[0x04];
  }
  offset=8;
  if(offset+chunk_length!=size)
    return retval; /* wrong rmi file size */
  if(chunk[0x08]!=0x52||chunk[0x09]!=0x4d||
     chunk[0x0a]!=0x49||chunk[0x0b]!=0x44||
     chunk[0x0c]!=0x64||chunk[0x0d]!=0x61||
     chunk[0x0e]!=0x74||chunk[0x0f]!=0x61)
    return retval;  /* "RMIDdata" */
  offset=16;
  { /* inline LSB32 parser : midi file size */
     chunk_length=chunk[offset+3];
     chunk_length<<=8;
     chunk_length+=chunk[offset+2];
     chunk_length<<=8;
     chunk_length+=chunk[offset+1];
     chunk_length<<=8;
     chunk_length+=chunk[offset];
     offset+=4;
  }
  if(chunk_length>size)
    return retval; /* wrong midi file size */
  if(!(retval=smf_parser(chunk+20,chunk_length)))
    return retval; /* smf parse failed */
  offset+=chunk_length;
  while(offset<size)
  {
    if(offset&0x1)
      offset++; /* adjusting alignment */
    { /* inline LSB32 parser : chunk size */
      if(offset+8>size)
        return retval; /* buffer overflow */
      chunk_length=chunk[offset+7];
      chunk_length<<=8;
      chunk_length+=chunk[offset+6];
      chunk_length<<=8;
      chunk_length+=chunk[offset+5];
      chunk_length<<=8;
      chunk_length+=chunk[offset+4];
    }
    if(offset+8+chunk_length>size)
      return retval; /* buffer overflow */
    if(chunk[offset]==0x52&&chunk[offset+1]==0x49&&
       chunk[offset+2]==0x46&&chunk[offset+3]==0x46) /* "RIFF" */
    {
      ; /* DLS (Downloadable Sounds) chunk */
    }
    else
    {
      ; /* info chunk */
    }
    offset+=8+chunk_length;
  }
  return retval;
}
