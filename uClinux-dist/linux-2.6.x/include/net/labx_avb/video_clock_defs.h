/*
 *  linux/include/net/labx_avb/video_clock_defs.h
 *
 *  Lab X Technologies AVB video clock recovery definitions
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

#ifndef _VIDEO_CLOCK_DEFS_H_
#define _VIDEO_CLOCK_DEFS_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* Enumeration for video clock modes */
typedef enum {
  VIDEO_CLOCK_DISABLED,
  VIDEO_CLOCK_MASTER,
  VIDEO_CLOCK_STREAM,
  VIDEO_CLOCK_EXTERNAL,
} VideoClockMode;

/* Type definition for conveying the video format */
typedef uint8_t  VideoClockFormat;

/* Enumeration for selecting a clock source */
typedef enum {
  VIDEO_CLOCK_SRC_A,
  VIDEO_CLOCK_SRC_B,
} VideoClockSource;

/* Stream index constant for signifying "coast" */
#define VIDEO_COAST_INDEX  (0xFFFFFFFF)

/* Structure for configuring a clock */
typedef struct {
  VideoClockMode   mode;
  VideoClockFormat format;
  VideoClockSource source;
  uint32_t         stream_index;
} VideoClockConfig;

/* I/O control commands */

#define VIDEO_CLOCK_IOC_CHAR  'v'

#define IOC_VIDEO_CLOCK_CONFIG  _IOW(VIDEO_CLOCK_IOC_CHAR,         \
                                     0,                            \
                                     VideoClockConfig*)

#endif
