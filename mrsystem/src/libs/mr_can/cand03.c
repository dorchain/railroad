#include <string.h>
#include <bitmask.h>
#include <bytestream.h>
#include "mr_can.h"

void MrCs2DecMfxVerify6(MrCs2CanDataType *CanMsg, unsigned long *Uid,
                        unsigned int *Sid)
{
   *Uid = GetLongFromByteArray((char *)CanMsg->Data);
   *Sid = GetIntFromByteArray((char *)&(CanMsg->Data[4]));
}

void MrCs2DecMfxVerify7(MrCs2CanDataType *CanMsg, unsigned long *Uid,
                        unsigned int *Sid, int *Ask)
{
   *Uid = GetLongFromByteArray((char *)CanMsg->Data);
   *Sid = GetIntFromByteArray((char *)&(CanMsg->Data[4]));
   *Ask = CanMsg->Data[6];
}
