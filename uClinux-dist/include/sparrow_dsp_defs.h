/*
 *  include/linux/sparrow_dsp_defs.h
 *
 *  Dsp processor driver
 *
 *  Written by Albert M. Hajjar (albert.hajjar@biamp.com)
 *
 *  Copyright (C) 2015 Biamp Systems, Inc., All Rights Reserved.
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

#ifndef _SPARROW_DSP_DEFS_H_
#define _SPARROW_DSP_DEFS_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define MAX_PORT_GROUPS  (8)
#define MAX_TDM_PORTS   (12)
#define MAX_TDM_SLOTS   (15)

typedef struct {
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t numInputGroups;
  uint32_t numOutputGroups;
  uint32_t numPortsGroup[MAX_PORT_GROUPS];
  uint32_t maxTdmChannels;
  uint32_t maxNetChannels;
  uint32_t totalOutputPorts;
} DspCaps;

#if 0
typedef enum {
  ASM_DIR_INPUT = 0,
  ASM_DIR_OUTPUT
} AsmDirection;

typedef struct {
  AsmDirection direction;
  uint32_t tdmPort;
  uint32_t tdmSlot;
} AsmLocationInfo;

typedef struct {
  AsmLocationInfo locationInfo;
  uint32_t scaleFactor;
} ScalingConfig;

typedef struct {
  AsmLocationInfo locationInfo;
  uint32_t pcmData;
} PeakConfig;

typedef struct {
  AsmLocationInfo locationInfo;
  uint64_t meterData;
} RmsConfig;

#endif

/* I/O control commands and structures specific to the sparrow dsp hardware */
#define SPARROW_DSP_IOC_CHAR      ('s')
#define IOC_GET_DSP_CAPS          _IOR(SPARROW_DSP_IOC_CHAR,  0x01, DspCaps)

#if 0
#define IOC_SET_RMS_UPDATE_PERIOD _IOW(SPARROW_DSP_IOC_CHAR,  0x01, uint32_t)
#define IOC_SET_SCALING           _IOW(SPARROW_DSP_IOC_CHAR,  0x01, ScalingConfig)
#define IOC_GET_PEAK              _IOWR(SPARROW_DSP_IOC_CHAR, 0x01, PeakConfig)
#define IOC_GET_RMS               _IOWR(SPARROW_DSP_IOC_CHAR, 0x01, RmsConfig)
#endif

#endif
