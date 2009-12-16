/****************************************************************************

  Filename:     crc16.h
  Description:  Cyclic Redundancy Check 16 functions
  Created:      24-Feb-1999

  Copyright (c) 2002-2003, Indigo Systems Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  Neither the name of the Indigo Systems Corporation nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.

****************************************************************************/

#define _CRC16_BYTES	1		/* ig */

#ifndef __CRC16_H__
#define __CRC16_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned short CRC16;

#ifdef _OPT_SIZE
    int ByteCRC16(int value, int crcin);
#else
    CRC16 CalcCRC16Words(unsigned int count, short *buffer);
#endif

#ifdef _CRC16_BYTES
    CRC16 CalcCRC16Bytes(unsigned int count, char *buffer);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CRC16_H__ */



