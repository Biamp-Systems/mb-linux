/*
 *  linux/include/net/labx_avb/video_depacketizer_defs.h
 *
 *  Lab X Technologies AVB Shaping DMA definitions
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
#ifndef _VIDEO_DEPACKETIZER_DEFS_H_
#define _VIDEO_DEPACKETIZER_DEFS_H_
 
#include <linux/types.h>
#include <linux/labx_microengine_defs.h>

/* I/O control commands specific to the depacketizer */

#define IOC_CLEAR_MATCHERS  _IO(ENGINE_IOC_CHAR, ENGINE_IOC_CLIENT_START)

typedef struct {
  uint32_t matchUnit;
  uint32_t configAction;
  uint32_t matchVector;
  uint64_t matchStreamId;
} VideoMatcherConfig;

/* Valid configuration actions to be performed on a match unit:
 * MATCHER_DISABLE - Disables the match unit
 * MATCHER_ENABLE  - Enables the match unit with a new ID
 */
#  define MATCHER_DISABLE  0x00000000
#  define MATCHER_ENABLE   0x00000001

#define IOC_CONFIG_MATCHER  _IOW(ENGINE_IOC_CHAR,               \
                                 (ENGINE_IOC_CLIENT_START + 1), \
                                 VideoMatcherConfig)

typedef struct {
  uint32_t matchUnit;
  uint32_t oldVector;
  uint32_t newVector;
  uint32_t ringStateOffset;
} MatcherRelocation;

#define IOC_RELOCATE_MATCHER _IOW(ENGINE_IOC_CHAR,                \
                                  (ENGINE_IOC_CLIENT_START + 2),  \
                                  MatcherRelocation)
typedef struct {
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t maxStreams;
  uint32_t maxInstructions;
  uint32_t maxParameters;
  uint32_t hasLoopback;
  uint32_t dataByteWidth;
} VideoDepacketizerCaps;

#define VIDEO_DEPACKETIZER_NO_LOOPBACK   (0)
#define VIDEO_DEPACKETIZER_HAS_LOOPBACK  (1)

#define IOC_GET_VIDEO_DEPACKETIZER_CAPS  _IOR(ENGINE_IOC_CHAR,               \
                                              (ENGINE_IOC_CLIENT_START + 3), \
                                              VideoDepacketizerCaps)

#define IOC_GET_VIDEO_STREAM_STATUS      _IOR(ENGINE_IOC_CHAR,               \
                                              (ENGINE_IOC_CLIENT_START + 4), \
                                              uint32_t*)

#define IOC_SET_VIDEO_DEPACKETIZER_LOOPBACK  _IOW(ENGINE_IOC_CHAR,               \
                                                  (ENGINE_IOC_CLIENT_START + 5), \
                                                  uint32_t)

#  define VIDEO_DISABLE_LOOPBACK  ((uint32_t) 0)
#  define VIDEO_ENABLE_LOOPBACK   ((uint32_t) 1)

/* Structure encapsulating settings for frame start discrimination */
typedef struct {
  uint32_t byteOffset;
  uint8_t  frameMask;
  uint8_t  frameValue;
} FrameDiscrimSettings;

#define IOC_SET_VIDEO_DEPACKETIZER_FRAME_DISCRIM  _IOW(ENGINE_IOC_CHAR,               \
                                                       (ENGINE_IOC_CLIENT_START + 6), \
                                                       FrameDiscrimSettings*)


/* Number of words encapsulated within a stream status response array */
#define VIDEO_STREAM_STATUS_WORDS  4

/* Type definitions and macros for packetizer microcode */

/* Opcode definitions */
#define VIDEO_DEPACKETIZER_OPCODE_NOP                0x00
#define VIDEO_DEPACKETIZER_OPCODE_VIDEO_SAMPLES      0x01
#define VIDEO_DEPACKETIZER_OPCODE_BRANCH_BAD_PACKET  0x02
#define VIDEO_DEPACKETIZER_OPCODE_STORE_RING_OFFSET  0x03
#define VIDEO_DEPACKETIZER_OPCODE_CHECK_SEQUENCE     0x04
#define VIDEO_DEPACKETIZER_OPCODE_BRANCH_CHECK       0x05
#define VIDEO_DEPACKETIZER_OPCODE_WRITE_PARAM        0x06
#define VIDEO_DEPACKETIZER_OPCODE_STORE_SEQUENCE     0x07
#define VIDEO_DEPACKETIZER_OPCODE_STOP               0x0F

/* Instruction field constant definitions */
#define VIDEO_DEPACKETIZER_OPCODE_SHIFT (24)

/* Mask applied to start-of-line video sample alignment fields */
#define VIDEO_LINE_ALIGN_MASK  (0x1F)

/* Number of bits in a sequence counter field */
#define AVTP_SEQUENCE_NUMBER_BITS  (8)

/* Mask and shift value for packets-per-line fields */
#define VIDEO_LINE_PKTS_MASK   (0x0F)
#define VIDEO_LINE_PKTS_SHIFT  (AVTP_SEQUENCE_NUMBER_BITS)

/* Width of a parameter source instruction operand */
#define VIDEO_PARAM_SOURCE_WIDTH          (3)
#define VIDEO_PARAM_SOURCE_FALSE       (0x00)
#define VIDEO_PARAM_SOURCE_TRUE        (0x01)
#define VIDEO_PARAM_SOURCE_TIMESTAMP   (0x02)
#define VIDEO_PARAM_SOURCE_TS_VALID    (0x03)
#define VIDEO_PARAM_SOURCE_RING_BEGIN  (0x04)
#define VIDEO_PARAM_SOURCE_RING_END    (0x05)

/* Converts the passed value to an word containing a pointer to a video channel
 *
 * @param descriptorBase - Base address of the containing descriptor
 * @param mapOffset      - Relative offset of the audio channel map to be encoded
 */
#define VIDEO_CHANNEL_POINTER(descriptorBase, mapOffset) ((uint32_t) (descriptorBase + mapOffset))

/**
 * Returns a VIDEO_SAMPLES instruction
 *
 * @param lineAlignment - Number of depacketizer words to align the ring offset to on a packet
 *                        with a valid timestamp.  This must take the width of the depacketizer datapath
 *                        into consideration.
 * @param linePackets   - Number of packets per line of encoded video
 */
#define VIDEO_DEPACKETIZER_VIDEO_SAMPLES(lineAlignment, linePackets)                          \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_VIDEO_SAMPLES << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (((linePackets - 1) & VIDEO_LINE_PKTS_MASK) << VIDEO_LINE_PKTS_SHIFT)        | \
               ((lineAlignment - 1) & VIDEO_LINE_ALIGN_MASK)))

/* Returns a CHECK_SEQUENCE instruction
 * This checks the received AVTP sequence number against the expected value.  This instruction
 * sets the "check" status bit accordingly for use by a future BRANCH_CHECK; the status bit
 * is not valid until two instructions later, so a delay slot must be inserted between this
 * instruction and a BRANCH_CHECK.
 *
 * This not only checks the sequence count, but also fetches the inter-line packet
 * count to see if the ring offset requires an alignment cycle after the payload is done.
 *
 * @param descriptorBase - Base address of the containing descriptor
 * @param sequenceOffset - Relative offset at which to fetch the sequence number
 */
#define VIDEO_DEPACKETIZER_CHECK_SEQUENCE(descriptorBase, sequenceOffset)                      \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_CHECK_SEQUENCE << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (descriptorBase + sequenceOffset)))

/* Returns a BRANCH_BAD_PACKET instruction
 * This instruction will stall the processor until the packet is complete.  If the packet is good,
 * execution continues at the next address.  If it is bad, the branch path is taken.
 *
 * @param descriptorBase - Base address of the containing descriptor
 * @param branchTarget   - Branch target taken if the packet is bad
 */
#define VIDEO_DEPACKETIZER_BRANCH_BAD_PACKET(descriptorBase, branchTarget)                        \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_BRANCH_BAD_PACKET << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (descriptorBase + branchTarget)))

/* Returns a BRANCH_CHECK instruction
 * This will branch if a prior "check" instruction resulted in a true condition.
 *
 * @param descriptorBase - Base address of the containing descriptor
 * @param branchTarget   - Branch target taken if the prior check instruction evaluated to true
 */
#define VIDEO_DEPACKETIZER_BRANCH_CHECK(descriptorBase, branchTarget)                        \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_BRANCH_CHECK << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (descriptorBase + branchTarget)))

/* Returns a WRITE_PARAM instruction
 * This writes a parameter to an address over the parameter memory interface.  Parameters are
 * derived from stream status information (timestamps, buffer write pointers, etc.) and are used
 * by downstream audio processing logic to maintain proper presentation time, mute during stream
 * errors, etc.
 *
 * @param paramAddress      - Address at which to write the parameter
 * @param paramAddressWidth - Width, in bits, of the parameter address.  This is dependent upon the
 *                            DEPACKETIZER_MAX_PARAMS VHDL constant which the depacketizer was built
 *                            against.
 * @param paramSource       - Which of the enumerated sources to write
 * @param frameOffset       - Address offset to apply if the most-recent timestamp is for a frame
 *                            start.  The offset may range from 0-15, with 0 implying a constant
 *                            address regardless.
 */
#define VIDEO_DEPACKETIZER_WRITE_PARAM(paramAddress, paramAddressWidth, paramSource, frameOffset) \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_WRITE_PARAM << VIDEO_DEPACKETIZER_OPCODE_SHIFT)       | \
               (frameOffset << (VIDEO_PARAM_SOURCE_WIDTH + paramAddressWidth))                  | \
               (paramSource << paramAddressWidth)                                               | \
               paramAddress))

/* Returns a STORE_RING_OFFSET instruction
 * The ring offset is the buffer pointer into cache memory for the stream, and forms the lower
 * portion of the cache memory write address.  It advances automatically with each byte of video
 * data written during execution of an VIDEO_SAMPLES opcode, and must be stored back if the packet
 * is determined to be valid and error-free.
 *
 * @param descriptorBase - Base address of the containing descriptor
 * @param storeOffset    - Relative offset at which to store the ring offset
 */
#define VIDEO_DEPACKETIZER_STORE_RING_OFFSET(descriptorBase, storeAddress)                        \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_STORE_RING_OFFSET << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (descriptorBase + storeAddress)))

/**  
  * Returns a STORE_SEQUENCE instruction
  * This stores both the sequence number from the packet as well as the inter-line packet
  * count, which has been either incremented or reloaded at the beginning of a line.
  *
  * @param descriptorBase - Base address of the containing descriptor
  * @param sequenceOffset - Address at which to store the sequence number
  */
#define VIDEO_DEPACKETIZER_STORE_SEQUENCE(descriptorBase, sequenceOffset)                      \
  ((uint32_t) ((VIDEO_DEPACKETIZER_OPCODE_STORE_SEQUENCE << VIDEO_DEPACKETIZER_OPCODE_SHIFT) | \
               (descriptorBase + sequenceOffset)))

/**
 * Returns a STOP instruction, stopping the microengine.
 */
#define VIDEO_DEPACKETIZER_STOP  ((uint32_t) (VIDEO_DEPACKETIZER_OPCODE_STOP << VIDEO_DEPACKETIZER_OPCODE_SHIFT))

/* Returns an instruction word containing an initial sequence number for a stream.
 * This special encoded value suppresses a sequence error from being generated when the
 * first packet of a newly-initialized stream is received, since there is no way for
 * the engine to know what sequence number will come first.
 */
#define VIDEO_INITIALIZE_SEQUENCE   ((uint32_t) 0x80000000)

/* Returns an instruction word containing a ring buffer offset, useful for allocating a location
 * for storing the present offset into a stream's ring buffers.
 *
 * @param ringOffset  - Starting offset for the stream
 */
#define VIDEO_RING_OFFSET_WORD(ringOffset)  ((uint32_t) ringOffset)

/* Returns an instruction word containing a video channel assignment
 *
 * @param videoChannel - Channel assignment for the stream
 */
#define VIDEO_CHANNEL_WORD(videoChannel)  ((uint32_t) videoChannel)

/* Constant and type declarations for use with Netlink */

/* Structure definition and command to return the active links of all streams.
 * Client code must be cognizant of which of the 128 stream bits are actually
 * relevant (i.e. the stream is within the max supported by hardware, and is
 * actively being received on at least one of the links).
 *
 * The STREAM_STATUS_WORDS constant is used to bound the number of words
 * returned for each call.
 */

/* Video Depacketizer events Generic Netlink family name, version, and multicast groups */
#define VIDEO_DEPACKETIZER_EVENTS_FAMILY_NAME     "VID_DEPKT_EVTS"
#define VIDEO_DEPACKETIZER_EVENTS_FAMILY_VERSION  1
#define VIDEO_DEPACKETIZER_EVENTS_STREAM_GROUP    "StreamGroup"

/* Constant enumeration for Netlink event commands from the video depacketizer driver */
enum {
  VIDEO_DEPACKETIZER_EVENTS_C_UNSPEC,
  VIDEO_DEPACKETIZER_EVENTS_C_STREAM_STATUS,
  __VIDEO_DEPACKETIZER_EVENTS_C_MAX,
};
#define VIDEO_DEPACKETIZER_EVENTS_C_MAX (__VIDEO_DEPACKETIZER_EVENTS_C_MAX - 1)

/* Netlink family attributes */
enum {
  VIDEO_DEPACKETIZER_EVENTS_A_UNSPEC,
  VIDEO_DEPACKETIZER_EVENTS_A_MINOR,
  VIDEO_DEPACKETIZER_EVENTS_A_STREAM_STATUS0,
  VIDEO_DEPACKETIZER_EVENTS_A_STREAM_STATUS1,
  VIDEO_DEPACKETIZER_EVENTS_A_STREAM_STATUS2,
  VIDEO_DEPACKETIZER_EVENTS_A_STREAM_STATUS3,
  VIDEO_DEPACKETIZER_EVENTS_A_STREAM_SEQ_ERROR,
  VIDEO_DEPACKETIZER_EVENTS_A_MATCH_ERROR,
  __VIDEO_DEPACKETIZER_EVENTS_A_MAX,
};
#define VIDEO_DEPACKETIZER_EVENTS_A_MAX (__VIDEO_DEPACKETIZER_EVENTS_A_MAX - 1)

#endif
