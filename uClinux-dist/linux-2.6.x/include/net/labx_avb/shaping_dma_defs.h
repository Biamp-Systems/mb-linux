/*
 *  linux/include/net/labx_avb/shaping_dma_defs.h
 *
 *  Lab X Technologies AVB Shaping DMA definitions
 *
 *  Written by Yi Cao (yi.cao@labxtechnologies.com)
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
#ifndef _SHAPING_DMA_DEFS_H_
#define _SHAPING_DMA_DEFS_H_
 
#include <linux/types.h>
#include <linux/labx_microengine_defs.h>

/* I/O control commands and structures specific to the shaping DMA */

#define IOC_LOAD_PACKET_TEMPLATE     _IOW(ENGINE_IOC_CHAR,         \
                                          ENGINE_IOC_CLIENT_START, \
                                          ConfigWords)
#define IOC_COPY_PACKET_TEMPLATE     _IOWR(ENGINE_IOC_CHAR,               \
                                           (ENGINE_IOC_CLIENT_START + 1), \
                                           ConfigWords)

typedef struct {
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t maxStreams;
  uint32_t shaperFractionBits;
  uint32_t maxInstructions;
  uint32_t maxTemplateQuadlets;
  uint32_t numOutputs;
  uint32_t ingressAlignment;
  uint32_t maxPacketBerths;
  uint32_t egressDataWidth;
} ShapingDmaCaps;

#define IOC_GET_SHAPING_DMA_CAPS      _IOR(ENGINE_IOC_CHAR,               \
                                          (ENGINE_IOC_CLIENT_START + 4), \
                                          ShapingDmaCaps)


#define SHAPER_STREAM_DISABLED (0x00)
#define SHAPER_STREAM_ENABLED  (0x01)

typedef struct {
  uint32_t streamIndex;
  uint32_t enabled;
  int32_t  idleSlope;
  int32_t  sendSlope;
} CreditShaperSettings;


#define IOC_CONFIG_STREAM_CREDIT     _IOW(ENGINE_IOC_CHAR,               \
                                          (ENGINE_IOC_CLIENT_START + 5), \
                                          CreditShaperSettings)

#define IOC_CONFIG_EGRESS_CREDIT     _IOW(ENGINE_IOC_CHAR,               \
                                          (ENGINE_IOC_CLIENT_START + 6), \
                                          CreditShaperSettings)                                                                                    
                                         
typedef enum {
  CTRL_LINK_100_MBIT = 0x00000000,
  CTRL_LINK_1_GBIT   = 0x10000000,
  CTRL_LINK_10_GBIT  = 0x20000000
} EgressLinkSpeed;

#define SHAPER_TX_DISABLED (0)
#define SHAPER_TX_ENABLED  (1)

/* Maximum transfer length supported */
#define DMA_MAX_TRANSFER_LENGTH  32

/* Maximum FIFO high-water level supported */
#define MAX_BURST_FIFO_LEVEL  255
                                          
typedef struct {
  uint32_t        txEnabled;
  uint32_t        burstLength;
  uint32_t        readFifoLevel;
  EgressLinkSpeed linkSpeed;
  int32_t         idleSlope;
  int32_t         sendSlope;
} TxEnableSettings;

#define IOC_SET_TX_ENABLED       _IOW(ENGINE_IOC_CHAR,               \
                                      (ENGINE_IOC_CLIENT_START + 7), \
                                      TxEnableSettings)


/* Type definitions and macros for the egress-stage microengine */

/* Opcode definitions */
#define SHAPING_DMA_OPCODE_TEMPLATE  0x00
#define SHAPING_DMA_OPCODE_PAYLOAD   0x07
#define SHAPING_DMA_OPCODE_STOP      0x0F

/* Instruction field constant definitions
 *
 * NOTE - The "relative addressing" bit is a carryover from a different
 *        packet engine design.  It is not supported yet; but is retained
 *        in the TEMPLATE instruction word definitions for possible future
 *        implementation.
 */
#define SHAPING_DMA_OPCODE_SHIFT 24

#define SHAPING_DMA_TEMPLATE_COUNT_MASK  0x01F
#define SHAPING_DMA_TEMPLATE_COUNT_SHIFT 18

#define SHAPING_DMA_OFFSET_COUNT_MASK  0x01F
#define SHAPING_DMA_OFFSET_COUNT_BITS      5
#define SHAPING_DMA_BYTE_SHIFT_MASK     0x07
#define SHAPING_DMA_BYTE_SHIFT_BITS        3

/**
 * Returns a TEMPLATE instruction
 *
 * @param templateAddress - Address of the first word in the template RAM to emit
 * @param byteCount       - Number of template bytes to emit
 */
#define SHAPING_DMA_TEMPLATE(templateAddress, byteCount)                   \
  ((uint32_t) ((SHAPING_DMA_OPCODE_TEMPLATE << SHAPING_DMA_OPCODE_SHIFT) | \
               ((((byteCount) - 1) & SHAPING_DMA_TEMPLATE_COUNT_MASK)   << \
                SHAPING_DMA_TEMPLATE_COUNT_SHIFT)                        | \
               (templateAddress)))

/**
 * Returns a PAYLOAD instruction
 * @param offsetCount - Number of cycles to delay before beginning the insertion
 * @param byteShift   - Number of bytes by which the payload data must be shifted 
 *                      right in order to align with previous bytes within the
 *                      packet engine's data word
 */
#define SHAPING_DMA_PAYLOAD(offsetCount, byteShift)                       \
  ((uint32_t) ((SHAPING_DMA_OPCODE_PAYLOAD << SHAPING_DMA_OPCODE_SHIFT) | \
               ((byteShift & SHAPING_DMA_BYTE_SHIFT_MASK)              << \
                SHAPING_DMA_OFFSET_COUNT_BITS)                          | \
               (offsetCount & SHAPING_DMA_OFFSET_COUNT_MASK)))

/**
 * Returns a STOP instruction, stopping the microengine.
 */
#define SHAPING_DMA_STOP  ((uint32_t) (SHAPING_DMA_OPCODE_STOP << SHAPING_DMA_OPCODE_SHIFT))

#endif
