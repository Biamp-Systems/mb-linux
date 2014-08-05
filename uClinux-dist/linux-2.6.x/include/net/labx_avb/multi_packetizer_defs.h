/*
 *  linux/include/net/labx_avb/multi_packetizer_defs.h
 *
 *  Lab X Technologies AVB Multimedia Packetizer engine definitions
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

#ifndef _MULTI_PACKETIZER_DEFS_H_
#define _MULTI_PACKETIZER_DEFS_H_

#include <linux/types.h>
#include <linux/labx_microengine_defs.h>
#include <net/labx_avb/multi_packetizer_types.h>

/*
 * Packetizer definitions
 */

/* I/O control commands and structures specific to the packetizer */
#define IOC_LOAD_PACKET_TEMPLATE     _IOW(ENGINE_IOC_CHAR,         \
                                          ENGINE_IOC_CLIENT_START, \
                                          ConfigWords)
#define IOC_COPY_PACKET_TEMPLATE     _IOWR(ENGINE_IOC_CHAR,               \
                                           (ENGINE_IOC_CLIENT_START + 1), \
                                           ConfigWords)

/* Fetches the capabilities of the hardware instance */
#define IOC_GET_MULTI_PACKETIZER_CAPS _IOR(ENGINE_IOC_CHAR,               \
                                           (ENGINE_IOC_CLIENT_START + 2), \
                                           MultiPacketizerCaps)

/* Activates a media port, loading its packet-creation microcode,
 * executing its initialization routine, and activating the input upon
 * success.
 */

typedef struct {
  uint32_t ingressIndex;
  uint32_t ingressHighWater;
  uint32_t initCodeOffset;
  uint32_t packetCodeOffset;
} IngressActivation;

#define IOC_ACTIVATE_MEDIA_INGRESS _IOW(ENGINE_IOC_CHAR,               \
                                        (ENGINE_IOC_CLIENT_START + 3), \
                                        IngressActivation)

/* Deactivates a media port by index */
#define IOC_DEACTIVATE_MEDIA_INGRESS _IOW(ENGINE_IOC_CHAR,               \
                                          (ENGINE_IOC_CLIENT_START + 4), \
                                          uint32_t)

/* Type definitions and macros for packetizer microcode */

/* Opcode definitions */
#define MULTI_OPCODE_TEMPLATE          0x00
#define MULTI_OPCODE_LOAD_ACCUM        0x01
#define MULTI_OPCODE_STORE_ACCUM       0x02
#define MULTI_OPCODE_ADD_ACCUM         0x03
#define MULTI_OPCODE_INSERT_ACCUM      0x04
#define MULTI_OPCODE_JUMP              0x05
#define MULTI_OPCODE_PAYLOAD           0x06
#define MULTI_OPCODE_INGRESS_CONFIG    0x07
#define MULTI_OPCODE_PACKET_LENGTH     0x08
#define MULTI_OPCODE_TIMESTAMP         0x09
#define MULTI_OPCODE_TS_VALID_BYTE     0x0A
#define MULTI_OPCODE_MAX_TRANSIT_TIME  0x0B
#define MULTI_OPCODE_BRANCH            0x0C
#define MULTI_OPCODE_FIFO_FLUSH        0x0D
#define MULTI_OPCODE_STOP              0x0F

/* Instruction field constant definitions
 *
 * NOTE - The "relative addressing" bit is a carryover from a different
 *        packet engine design.  It is not supported yet; but is retained
 *        in the TEMPLATE instruction word definitions for possible future
 *        implementation.
 */
#define MULTI_OPCODE_SHIFT 28

#define MULTI_TEMPLATE_RELATIVE_BIT 0x08000000
#define MULTI_TEMPLATE_COUNT_MASK        0x01F
#define MULTI_TEMPLATE_COUNT_SHIFT          22

#define MULTI_INGRESS_REALIGN_BIT 0x00010000
#define MULTI_INGRESS_LEVEL_MASK       0x7FF
#define MULTI_INGRESS_LEVEL_SHIFT          5
#define MULTI_PORT_SELECT_MASK         0x01F
#define MULTI_PORT_SELECT_SHIFT            0

/* Bits and shift for fixed-length "cycles to offset an insertion" fields */
#define MULTI_OFFSET_COUNT_BITS     5
#define MULTI_OFFSET_COUNT_MASK 0x01F

/* Mask and shift for timestamp valid byte values */
#define MULTI_TS_VALID_BYTE_MASK  0x0FF
#define MULTI_TS_VALID_BYTE_BITS  8

/* Definitions for accumulator opcodes */
#define ACCUM_SELECT_MASK     0x01
#define ACCUM_SELECT_SHIFT      27
#define ACCUM_VALUE_MASK     0x0FF
#define ACCUM_VALUE_BITS         8
#define ACCUM_VALUE_ROLLOVER   256

/* Mask value for maximum transit time values */
#define MAX_TRANSIT_TIME_MASK  0x0FFFFFFF

/* Mask value appropriate for packet length parameters */
#define PACKET_LENGTH_MASK  0x000007FF

/* Enumerated values for branch conditions */
#define MULTI_BRANCH_ALWAYS  0
#define MULTI_BRANCH_EQ      1
#define MULTI_BRANCH_NEQ     2
#define MULTI_BRANCH_LT      3
#define MULTI_BRANCH_LT_EQ   4
#define MULTI_BRANCH_GT      5
#define MULTI_BRANCH_GT_EQ   6

/* Mask and shift value for branch condition fields */
#define BRANCH_COND_MASK   0x07
#define BRANCH_COND_SHIFT     8

/* Shift value for FIFO flush fracions */
#define FLUSH_FRACT_SHIFT  5

/* Each of these macros accepts a MultiPacketizerCaps struct as a parameter */
#define MULTI_ENGINE_DATA_WIDTH(packetizerCaps)         (packetizerCaps.engineDataWidth)
#define MULTI_ENGINE_CODE_ADDRESS_MASK(packetizerCaps)  (packetizerCaps.maxInstructions - 1)
#define MULTI_PAYLOAD_WORD_COUNT_MASK(packetizerCaps)   (packetizerCaps.wordCountMask)
#define MULTI_WORD_COUNT_BITS(packetizerCaps)           (packetizerCaps.wordCountBits)
#define MULTI_QUADLET_COUNT_MASK(packetizerCaps)        (packetizerCaps.quadletCountMask)
#define MULTI_QUADLET_COUNT_BITS(packetizerCaps)        (packetizerCaps.quadletCountBits)
#define MULTI_BYTE_COUNT_MASK(packetizerCaps)           ((packetizerCaps.quadletCountMask << 2) | 0x03)
#define MULTI_BYTE_COUNT_BITS(packetizerCaps)           (packetizerCaps.quadletCountBits + 2)

/* Definitions for a PAYLOAD instruction field */
#define PAYLOAD_SHIFT_INDIRECT_BIT(packetizerCaps)           \
  (0x00000001 << (MULTI_WORD_COUNT_BITS(packetizerCaps) +    \
                  MULTI_QUADLET_COUNT_BITS(packetizerCaps) + \
                  MULTI_OFFSET_COUNT_BITS +                  \
                  MULTI_QUADLET_COUNT_BITS(packetizerCaps)))

/**
 * Returns a TEMPLATE instruction
 *
 * @param templateAddress - Address of the first word in the template RAM to emit
 * @param wordCount       - Number of template words to emit
 */
#define MULTI_PACKETIZER_TEMPLATE(templateAddress, wordCount)      \
  ((uint32_t) ((MULTI_OPCODE_TEMPLATE << MULTI_OPCODE_SHIFT)     | \
               ((((wordCount) - 1) & MULTI_TEMPLATE_COUNT_MASK) << \
                MULTI_TEMPLATE_COUNT_SHIFT)                      | \
               (templateAddress)))

/**
 * Returns a LOAD_ACCUM instruction
 *
 * @param packetizerCaps   - Multimedia packetizer device structure
 * @param whichAccumulator - Index of the accumulator to load a byte into
 * @param loadAddress      - Microcode RAM address of the data location to load; the low byte will be loaded.
 */
#define MULTI_PACKETIZER_LOAD_ACCUM(packetizerCaps, whichAccumulator, loadAddress)   \
  ((uint32_t) ((MULTI_OPCODE_LOAD_ACCUM << MULTI_OPCODE_SHIFT)                     | \
               (((whichAccumulator) & ACCUM_SELECT_MASK) << ACCUM_SELECT_SHIFT)    | \
               ((loadAddress) & MULTI_ENGINE_CODE_ADDRESS_MASK(packetizerCaps))))

/**  
 * Returns a STORE_ACCUM instruction
 *
 * @param packetizerCaps   - Multimedia packetizer device structure
 * @param whichAccumulator - Index of the accumulator to store; it will be stored to the low byte
 *                           of microcode RAM at the address specified.
 * @param storeAddress     - Address of the data location to store to
 */
#define MULTI_PACKETIZER_STORE_ACCUM(packetizerCaps, whichAccumulator, storeAddress)   \
  ((uint32_t) ((MULTI_OPCODE_STORE_ACCUM << MULTI_OPCODE_SHIFT)                      | \
               (((whichAccumulator) & ACCUM_SELECT_MASK) << ACCUM_SELECT_SHIFT)      | \
               ((storeAddress) & MULTI_ENGINE_CODE_ADDRESS_MASK(packetizerCaps))))

/** 
 * Returns an ADD_ACCUM instruction
 *
 * @param packetizerCaps   - Multimedia packetizer device structure
 * @param whichAccumulator - Index of the accumulator to insert
 * @param addend           - Addend to increment the accumulator by
 */
#define MULTI_PACKETIZER_ADD_ACCUM(packetizerCaps, whichAccumulator, addend)      \
  ((uint32_t) ((MULTI_OPCODE_ADD_ACCUM << MULTI_OPCODE_SHIFT)                   | \
               (((whichAccumulator) & ACCUM_SELECT_MASK) << ACCUM_SELECT_SHIFT) | \
               ((addend) & ACCUM_VALUE_MASK)))
  
/** 
 * Returns an INSERT_ACCUM instruction
 *
 * @param packetizerCaps   - Multimedia packetizer device structure
 * @param whichAccumulator - Index of the accumulator to insert
 * @param accumShift       - Shift, in bytes, to apply to the accumulator value
 * @param accumOffset      - Offset into the template data words at which to place the shifted
 *                           accumulator value
 * @param postAddend       - Addend to increment the accumulator by, post-insertion
 */
#define MULTI_PACKETIZER_INSERT_ACCUM(packetizerCaps, whichAccumulator, accumShift, accumOffset, postAddend)   \
  ((uint32_t) ((MULTI_OPCODE_INSERT_ACCUM << MULTI_OPCODE_SHIFT)                                             | \
               (((whichAccumulator) & ACCUM_SELECT_MASK) << ACCUM_SELECT_SHIFT)                              | \
               ((postAddend) & ACCUM_VALUE_MASK)                                                             | \
               (((accumOffset) & MULTI_OFFSET_COUNT_MASK) << ACCUM_VALUE_BITS)                               | \
               (((accumShift) & MULTI_BYTE_COUNT_MASK(packetizerCaps)) << \
                (MULTI_OFFSET_COUNT_BITS + ACCUM_VALUE_BITS))))

/**
 * Definitions for specifying different payload offset modes
 *
 * e_PayloadShiftImmediate - Payload shift operand is an immediate value
 * e_PayloadShiftIndirect  - Payload shift operand is an accumulator index
 */
typedef enum {
  e_PayloadShiftImmediate = 0,
  e_PayloadShiftIndirect  = 1,
} PayloadShiftMode;

/**
 * Returns a PAYLOAD instruction
 *
 * @param packetizerCaps  - Multimedia packetizer device structure
 * @param payloadWords    - Number of words of payload data to pull from the ingress
 *                          and incorporate into the packet
 * @param payloadFraction - Number of quadlets which are a fraction of a full word at
 *                          the end of the packet.
 * @param offsetCount     - Number of cycles to delay before beginning the insertion
 * @param quadletShift    - Number of quadlets by which the payload data must be shifted 
 *                          right in order to align with previous quadlets within the
 *                          packet engine's data word
 * @param shiftMode       - Value indicating whether the quadlet_Shift parameter is an
 *                          index register number instead of an immediate value.
 * @param payloadMask     - Number of data quadlets which must be masked to zero on the
 *                          first word of each packet, beginning with the most-significant.
 */
#define MULTI_PACKETIZER_PAYLOAD(packetizerCaps, payloadWords, payloadFraction, offsetCount, quadletShift, shiftMode, payloadMask) \
  ((uint32_t) ((MULTI_OPCODE_PAYLOAD << MULTI_OPCODE_SHIFT)                                         | \
               ((payloadFraction) & MULTI_QUADLET_COUNT_MASK(packetizerCaps))                       | \
               ((((payloadWords) - 1) & MULTI_PAYLOAD_WORD_COUNT_MASK(packetizerCaps)) <<             \
                MULTI_QUADLET_COUNT_BITS(packetizerCaps))                                           | \
               (((offsetCount) & MULTI_OFFSET_COUNT_MASK) <<                                          \
                (MULTI_WORD_COUNT_BITS(packetizerCaps) + MULTI_QUADLET_COUNT_BITS(packetizerCaps))) | \
               (((quadletShift) & MULTI_QUADLET_COUNT_MASK(packetizerCaps)) <<                        \
                (MULTI_WORD_COUNT_BITS(packetizerCaps) +                                              \
                 MULTI_QUADLET_COUNT_BITS(packetizerCaps) +                                           \
                 MULTI_OFFSET_COUNT_BITS))                                                          | \
               (shiftMode ? PAYLOAD_SHIFT_INDIRECT_BIT(packetizerCaps) : 0x00000000)                | \
               (((payloadMask) & MULTI_QUADLET_COUNT_MASK(packetizerCaps)) <<                         \
                (MULTI_WORD_COUNT_BITS(packetizerCaps) +                                              \
                 MULTI_QUADLET_COUNT_BITS(packetizerCaps) +                                           \
                 MULTI_OFFSET_COUNT_BITS +                                                            \
                 MULTI_QUADLET_COUNT_BITS(packetizerCaps) +                                           \
                 1))))
                
  
/**
 * Returns an INGRESS_CONFIG instruction
 *
 * @param whichPort   - Index of the port to configure
 * @param readyLevel  - Fill level at which the ingress should indicate "ready"
 * @param realignPort - Flag indicating whether to re-align the port to the next
 *                      ingress frame start
 */
#define MULTI_PACKETIZER_INGRESS_CONFIG(whichPort, readyLevel, realignPort)               \
  ((uint32_t) ((MULTI_OPCODE_INGRESS_CONFIG << MULTI_OPCODE_SHIFT)                      | \
               ((whichPort) & MULTI_PORT_SELECT_MASK)                                   | \
               (((readyLevel) & MULTI_INGRESS_LEVEL_MASK) << MULTI_INGRESS_LEVEL_SHIFT) | \
               ((realignPort) ? MULTI_INGRESS_REALIGN_BIT : 0x00000000)))

/**
 * Returns a PACKET_LENGTH instruction
 *
 * @param nextLength - Length, in bytes, to be declared for the next packet
 */
#define MULTI_PACKETIZER_PACKET_LENGTH(nextLength)                  \
  ((uint32_t) ((MULTI_OPCODE_PACKET_LENGTH << MULTI_OPCODE_SHIFT) | \
               ((nextLength) & PACKET_LENGTH_MASK)))

/**
 * Returns a TIMESTAMP instruction
 *
 * @param packetizerCaps  - Multimedia packetizer device structure
 * @param timestampShift  - Shift, in quadlets, to apply to timestamps
 * @param timestampOffset - Offset into the template data at which to place the shifted
 *                          timestamp quadlet
 * @param flagsShift      - Shift, in bytes, to apply to timestamp flags
 * @param flagsOffset     - Offset into the template data at which to place the shifted
 *                          timestamp flags byte
 */
#define MULTI_PACKETIZER_TIMESTAMP(packetizerCaps, timestampShift, timestampOffset, flagsShift, flagsOffset)   \
  ((uint32_t) ((MULTI_OPCODE_TIMESTAMP << MULTI_OPCODE_SHIFT)                                                | \
               (((flagsOffset) & MULTI_OFFSET_COUNT_MASK) <<                                                   \
                (MULTI_BYTE_COUNT_BITS(packetizerCaps) +                                                       \
                 MULTI_OFFSET_COUNT_BITS +                                                                     \
                 MULTI_QUADLET_COUNT_BITS(packetizerCaps)))                                                  | \
               (((flagsShift) & MULTI_BYTE_COUNT_MASK(packetizerCaps)) <<                                      \
                (MULTI_OFFSET_COUNT_BITS + MULTI_QUADLET_COUNT_BITS(packetizerCaps)))                        | \
               (((timestampOffset) & MULTI_OFFSET_COUNT_MASK) << MULTI_QUADLET_COUNT_BITS(packetizerCaps))   | \
               ((timestampShift) & MULTI_QUADLET_COUNT_MASK(packetizerCaps))))

/**
 * Returns a TS_VALID_BYTE instruction
 *
 * @param packetizerCaps - Multimedia packetizer device structure
 * @param tsValidByte    - Byte value to apply to template data when the timestamp is valid
 * @param tsValidShift   - Shift, in bytes, to apply to the timestamp valid byte
 * @param tsValidOffset  - Offset into the template data words at which to place the shifted
 *                         timestamp valid byte
 */
#define MULTI_PACKETIZER_TS_VALID_BYTE(packetizerCaps, tsValidByte, tsValidShift, tsValidOffset)   \
  ((uint32_t) ((MULTI_OPCODE_TS_VALID_BYTE << MULTI_OPCODE_SHIFT)                                | \
               (((tsValidShift) & MULTI_BYTE_COUNT_MASK(packetizerCaps)) <<                        \
                (MULTI_OFFSET_COUNT_BITS + MULTI_TS_VALID_BYTE_BITS))                            | \
               (((tsValidOffset) & MULTI_OFFSET_COUNT_MASK) << MULTI_TS_VALID_BYTE_BITS)         | \
               ((tsValidByte) & MULTI_TS_VALID_BYTE_MASK)))

/**
 * Returns a MAX_TRANSIT_TIME instruction
 *
 * @param maxTransitTime - Maximum time, in nanoseconds, that the packets are permitted to
 *                         consume traversing the network to the farthest Listener.
 */
#define MULTI_PACKETIZER_MAX_TRANSIT_TIME(maxTransitTime)              \
  ((uint32_t) ((MULTI_OPCODE_MAX_TRANSIT_TIME << MULTI_OPCODE_SHIFT) | \
               (maxTransitTime & MAX_TRANSIT_TIME_MASK)))

/**
 * Returns a BRANCH instruction
 *
 * @param whichAccumulator - Index of the accumulator to branch upon
 * @param branchCondition  - Condition for which the branch is taken
 * @param branchValue      - Value used in evaluating the branch condition
 */
#define MULTI_PACKETIZER_BRANCH(whichAccumulator, branchCondition, branchValue)   \
  ((uint32_t) ((MULTI_OPCODE_BRANCH << MULTI_OPCODE_SHIFT)                      | \
               (((whichAccumulator) & ACCUM_SELECT_MASK) << ACCUM_SELECT_SHIFT) | \
               (((branchCondition) & BRANCH_COND_MASK) << BRANCH_COND_SHIFT)    | \
               ((branchValue) & ACCUM_VALUE_MASK)))

/**
 * Returns an instruction word containing a branch target.
 *
 * @param targetAddress - Target address to branch to
 */
#define MULTI_PACKETIZER_BRANCH_TARGET(targetAddress) ((uint32_t) targetAddress)

/**
 * Returns a FIFO_FLUSH instruction
 *
 * @param packetizerCaps - Multimedia packetizer device structure
 * @param whichPort      - Index of the port to flush a fraction of a word from.
 *                         The flush is performed once the payload has completed.
 * @param flushFraction  - Number of quadlets (fraction of a full word) to flush
 */
#define MULTI_PACKETIZER_FIFO_FLUSH(packetizerCaps, whichPort, flushFraction)   \
  ((uint32_t) ((MULTI_OPCODE_FIFO_FLUSH << MULTI_OPCODE_SHIFT)                | \
               (((flushFraction) & MULTI_QUADLET_COUNT_MASK(packetizerCaps)) << \
                FLUSH_FRACT_SHIFT) |                                            \
               ((whichPort) & MULTI_PORT_SELECT_MASK)))
  

/**
 * Returns a STOP instruction, stopping the microengine.
 */
#define MULTI_PACKETIZER_STOP  ((uint32_t) (MULTI_OPCODE_STOP << MULTI_OPCODE_SHIFT))

#endif
