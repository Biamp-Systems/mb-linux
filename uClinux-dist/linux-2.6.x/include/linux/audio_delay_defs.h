/*
 *  include/linux/audio_delay_defs.h
 *
 *  Dynamic Audio Delay (Lip Sync) driver
 *
 *  Written by Albert M. Hajjar (albert.hajjar@labxtechnologies.com)
 *
 *  Copyright (C) 2016 Biamp Systems, Inc., All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _AUDIO_DELAY_DEFS_H_
#define _AUDIO_DELAY_DEFS_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define MAX_NUM_SRCS  (4)
#define MAX_NUM_SINKS (4)

/* Max number of entries which can be configured in one ioctl() call */
#define MAX_SINK_OFFSETS  (256)

typedef struct {
  uint32_t srcOffset;
  uint32_t sinkOffset;
  uint32_t sampleDelay;
  uint32_t enableSink;
  uint32_t enableDelay;
} SinkDelayEntry;

typedef struct {
  uint32_t        numEntries;
  SinkDelayEntry *delayEntries;
} AudioDelayConfig;

typedef struct {
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t numSrcs;         
  uint32_t numSinks;         
  uint32_t maxRingSize;      
  uint32_t numRestSrcChans;  
  uint32_t numRestSinkChans;
  uint32_t dmaCacheAddrBits;
  uint32_t srcRingSize[MAX_NUM_SRCS];
  uint32_t numSrcChans[MAX_NUM_SRCS];
  uint32_t sinkRingSize[MAX_NUM_SINKS];
  uint32_t numSinkChans[MAX_NUM_SINKS];
} AudioDelayCaps;

/* I/O control commands and structures specific to the audio tdm hardware */
#define AUDIO_DELAY_IOC_CHAR        ('l')
#define IOC_GET_AUDIO_DELAY_CAPS    _IOR(AUDIO_DELAY_IOC_CHAR,  0x01, AudioDelayCaps)
#define IOC_SET_PARAM_OFFSET_BASE   _IOW(AUDIO_DELAY_IOC_CHAR,  0x02, uint8_t)
#define IOC_SET_SINK_DELAY_CHAN_MAP _IOW(AUDIO_DELAY_IOC_CHAR,  0x03, AudioDelayConfig)
#define IOC_GET_SINK_DELAY_CHAN_MAP _IOWR(AUDIO_DELAY_IOC_CHAR, 0x04, AudioDelayConfig)
  
#endif /* _AUDIO_DELAY_DEFS_H_ */
