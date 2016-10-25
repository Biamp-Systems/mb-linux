/*
 *  include/cache_interface_defs.h
 *
 *  Microblaze mailbox defs
 *
 *  Written by Chris Pane (chris.pane@biamp.com)
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

#ifndef CACHE_INTERFACE_DEFS_H
#define CACHE_INTERFACE_DEFS_H

#include <linux/types.h>
#include <linux/ioctl.h>
#ifndef __KERNEL__
#include <stdint.h>
#endif

#define CACHE_INTERFACE_IOC_CHAR  'c'
#define CACHE_INTERFACE_MAX_MAP_ENTRIES     64          

#define CACHE_INTERFACE_AUTO_MUTE_ALWAYS    0x00
#define CACHE_INTERFACE_AUTO_MUTE_STMSTATUS 0x01
#define CACHE_INTERFACE_AUTO_MUTE_NEVER     0x02

#define CACHE_INTERFACE_AVB_STREAM_NONE  0xFFFFFFFF

/* Structure mapping a single Cache interface channel to its AVB stream */
typedef struct {
  uint32_t channel;
  uint32_t avbStream;
  uint32_t enable;
} CacheInterfaceStreamMapEntry;

typedef struct {
  uint32_t        enable;
  uint32_t        numMapEntries;
  CacheInterfaceStreamMapEntry *mapEntries;
} CacheInterfaceAutoMuteConfig;


#define IOC_CACHE_INTERFACE_CONFIG_AUTO_MUTE            _IOR(CACHE_INTERFACE_IOC_CHAR, 0x04, CacheInterfaceAutoMuteConfig)

#endif // CACHE_INTERFACE_DEFS_H

