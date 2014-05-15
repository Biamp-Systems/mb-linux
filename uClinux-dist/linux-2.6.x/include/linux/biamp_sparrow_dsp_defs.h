/*
 *  include/linux/biamp_sparrow_dsp_defs.h
 *
 *  Biamp Sparrow DSP driver
 *
 *  Written by Eldridge M. Mount IV (eldridge.mount@labxtechnologies.com)
 *  Written by Albert M. Hajjar (albert.hajjar@labxtechnologies.com)
 *
 *  Copyright (C) 2013 Biamp Systems, All Rights Reserved.
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

#ifndef _BIAMP_SPARROW_DSP_DEFS_H_
#define _BIAMP_SPARROW_DSP_DEFS_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/* Structure mapping a single TDM channel to its AVB stream */
typedef struct {
  uint32_t addr;
  uint32_t data;
  uint32_t mask;
} SparrowDspInfo;

/* I/O control commands and structures specific to the audio tdm hardware */
#define SPARROW_DSP_IOC_CHAR            ('t')
#define IOC_GET_SPARROW_DSP_REG       _IOR(SPARROW_DSP_IOC_CHAR, 0x00, SparrowDspInfo)
#define IOC_SET_SPARROW_DSP_REG       _IOW(SPARROW_DSP_IOC_CHAR, 0x01, SparrowDspInfo)
#define IOC_SET_SPARROW_DSP_REG_BIT   _IOW(SPARROW_DSP_IOC_CHAR, 0x02, SparrowDspInfo)
#define IOC_CLEAR_SPARROW_DSP_REG_BIT _IOW(SPARROW_DSP_IOC_CHAR, 0x03, SparrowDspInfo)
#define IOC_MODIFY_SPARROW_DSP_REG    _IOW(SPARROW_DSP_IOC_CHAR, 0x04, SparrowDspInfo)

#endif
