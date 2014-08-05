/*
 *  linux/include/net/labx_avb/multi_packetizer_types.h
 *
 *  Lab X Technologies AVB Multimedia Packetizer engine type definitions
 *
 *  Written by Eldridge M. Mount IV (eldridge.mount@labxtechnologies.com)
 *
 *  Copyright (C) 2012 Lab X Technologies LLC, All Rights Reserved.
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

#ifndef _MULTI_PACKETIZER_TYPES_H_
#define _MULTI_PACKETIZER_TYPES_H_

#include <linux/types.h>

/**
 * Structure encapsulating hardware capabilities for a multimedia
 * packetizer instance
 */
typedef struct {
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t maxInstructions;
  uint32_t maxTemplateQuadlets;
  uint32_t numMediaPorts;
  uint32_t engineDataWidth;
  uint32_t ingressFifoDepth;
  uint32_t wordCountMask;
  uint32_t wordCountBits;
  uint32_t quadletCountMask;
  uint32_t quadletCountBits;
} MultiPacketizerCaps;

#endif
