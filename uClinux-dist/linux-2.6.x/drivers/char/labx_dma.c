/*
 *  linux/drivers/net/labx_avb/labx_dma.c
 *
 *  Lab X Technologies DMA coprocessor
 *
 *  Written by Eldridge Chris Wulff (chris.wulff@labxtechnologies.com)
 *
 *  Copyright (C) 2009 Lab X Technologies LLC, All Rights Reserved.
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

/* System headers */
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>
#include <linux/labx_dma.h>
#include <linux/dma_coprocessor_defs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <net/genetlink.h>
#include <net/netlink.h>
#include <xio.h>

#define DRIVER_NAME "labx_dma"

/* Hardware version number definitions for detecting compatability */
#define DRIVER_VERSION_MIN  0x10
#define DRIVER_VERSION_MAX  0x13
#define CAPS_INDEX_VERSION  0x11 /* First version with # index counters in CAPS word */

/* Number of milliseconds we will wait before bailing out of a synced write */
#define SYNCED_WRITE_TIMEOUT_MSECS  (100)

/* Number of milliseconds to wait before permitting consecutive events from
 * being propagated up to userspace
 */
#define EVENT_THROTTLE_MSECS (100)

/* Interrupt service routine for the instance */
static irqreturn_t labx_dma_interrupt(int irq, void *dev_id) {
  struct labx_dma *dma = (struct labx_dma*) dev_id;
  uint32_t maskedFlags;
  uint32_t irqMask;
  irqreturn_t returnValue = IRQ_NONE;

  /* Read the interrupt flags and immediately clear them */
  maskedFlags = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_FLAGS_REG));
  irqMask = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG));
  maskedFlags &= irqMask;
  XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_FLAGS_REG), maskedFlags);

  /* Detect the synchronized write IRQ */
  if((maskedFlags & DMA_SYNC_IRQ) != 0) {
    /* Wake up all threads waiting for a synchronization event */
    wake_up_interruptible(&(dma->syncedWriteQueue));
    returnValue = IRQ_HANDLED;
  }

  /* Detect the status ready IRQ */
  if((maskedFlags & DMA_STAT_READY_IRQ) != 0) {
    /* Disarm the status FIFO interrupt while the status thread handles the present
     * event(s).  This permits the status thread to limit the rate at which events
     * are accepted and propagated up to userspace.
     */
    irqMask &= ~DMA_STAT_READY_IRQ;
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG), irqMask);

    /* Wake up the Netlink thread to consume status data */
    dma->statusReady = DMA_NEW_STATUS_READY;
    wake_up_interruptible(&(dma->statusFifoQueue));
    returnValue = IRQ_HANDLED;
  }

  /* Return whether this was an IRQ we handled or not */
  return(returnValue);
}

/* Generic Netlink family definition */
static struct genl_family events_genl_family = {
  .id      = GENL_ID_GENERATE,
  .hdrsize = 0,
  .name    = DMA_EVENTS_FAMILY_NAME,
  .version = DMA_EVENTS_FAMILY_VERSION,
  .maxattr = DMA_EVENTS_A_MAX,
};

/* Multicast groups */
static struct genl_multicast_group labx_dma_mcast = {
  .name = DMA_EVENTS_STATUS_GROUP,
};

/* Method to conditionally transmit a Netlink packet containing one or more
 * packets of information from the status FIFO
 */
static int tx_netlink_status(struct labx_dma *dma) {
  struct sk_buff *skb;
  struct nlattr *statusArray;
  void *msgHead;
  int returnValue = 0;
  uint32_t maskedFlags;
  uint32_t numStatusPackets;

  /* First evaluate whether there are any packets to be sent at all */
  if(dma->statusTail != dma->statusHead) {
    /* We will send a packet, allocate a socket buffer */
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if(skb == NULL) return(-ENOMEM);

    /* Create the message headers */
    msgHead = genlmsg_put(skb, 
                          0, 
                          dma->netlinkSequence++, 
                          &events_genl_family, 
                          0, 
                          DMA_EVENTS_C_STATUS_PACKETS);
    if(msgHead == NULL) {
      returnValue = -ENOMEM;
      goto tx_failure;
    }

    /* Write the DMA device's ID, properly translated for userspace, to identify the
     * message source.  The full ID is required since this driver can be used encapsulated 
     * within any number of more complex devices.
     */
    returnValue = nla_put_u32(skb, DMA_EVENTS_A_DMA_DEVICE, new_encode_dev(dma->deviceNode));
    if(returnValue != 0) goto tx_failure;

    /* Set an attribute indicating whether the status FIFO overflowed before
     * this packet could be sent.  This is indicated by the overflow IRQ bit,
     * which is cleared unconditionally once read.
     */
    maskedFlags = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_FLAGS_REG));
    maskedFlags &= DMA_STAT_OVRFLW_IRQ;
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_FLAGS_REG), maskedFlags);
    returnValue = nla_put_u32(skb, 
                              DMA_EVENTS_A_STATUS_OVERFLOW, 
                              ((maskedFlags != 0) ? DMA_STATUS_FIFO_OVERFLOW :
                                                    DMA_STATUS_FIFO_GOOD));
    if(returnValue != 0) goto tx_failure;

    /* Begin a nested attribute of status FIFO packets */
    statusArray = nla_nest_start(skb, DMA_EVENTS_A_STATUS_ARRAY);
    if(statusArray == NULL) goto tx_failure;

    /* Determine how many packets will be written and record this */
    numStatusPackets = (dma->statusHead - dma->statusTail);
    if(numStatusPackets < 0) numStatusPackets += MAX_STATUS_PACKETS_PER_DGRAM;
    returnValue = nla_put_u32(skb, DMA_STATUS_ARRAY_A_LENGTH, numStatusPackets);
    if(returnValue != 0) goto tx_failure;

    /* Iterate through the circular buffer of status packets until all have
     * been consumed.  Each status FIFO packet is encoded as an entry in an
     * overall Netlink table.
     */
    while(dma->statusTail != dma->statusHead) {
      DMAStatusPacket *statusPacket = dma->statusTail;
      struct nlattr *packetNesting;
      uint32_t wordIndex;
      int32_t nestIndex;

      /* Each packet is itself a nested table of words; begin the nesting */
      packetNesting = nla_nest_start(skb, DMA_STATUS_ARRAY_A_PACKETS);
      if(packetNesting == NULL) goto tx_failure;
      
      /* Write the length of the packet and then its raw words */
      returnValue = nla_put_u32(skb, 
                                DMA_STATUS_PACKET_A_LENGTH, 
                                statusPacket->packetLength);
      if(returnValue != 0) goto tx_failure;

      nestIndex = DMA_STATUS_PACKET_A_WORDS;
      for(wordIndex = 0; wordIndex < statusPacket->packetLength; wordIndex++) {
        returnValue = nla_put_u32(skb, nestIndex++, statusPacket->packetData[wordIndex]);
        if(returnValue != 0) goto tx_failure;
      }
      
      /* Close the nesting for the present status packet */
      nla_nest_end(skb, packetNesting);

      /* Advance the tail pointer, wrapping at the end of the buffers */
      dma->statusTail++;
      if((dma->statusTail - dma->statusPackets) >= MAX_STATUS_PACKETS_PER_DGRAM) {
        dma->statusTail = dma->statusPackets;
      }
    }

    /* Close the nested array of status packets */
    nla_nest_end(skb, statusArray);

    /* Finalize the message and multicast it */
    genlmsg_end(skb, msgHead);
    returnValue = genlmsg_multicast(skb, 0, labx_dma_mcast.id, GFP_ATOMIC);

    switch(returnValue) {
    case 0:
    case -ESRCH:
      // Success or no process was listening, simply break
      break;

    default:
      // This is an actual error, print the return code
      printk(KERN_INFO DRIVER_NAME ": Failure delivering multicast Netlink message: %d\n",
             returnValue);
      goto tx_failure;
    }
  } /* if(any packets to send) */

 tx_failure:
  return(returnValue);
}

/* Kernel thread used to produce Netlink packets based upon the status FIFO */
static int netlink_thread(void *data) {
  struct labx_dma *dma = (struct labx_dma*) data;
  uint32_t fifoFlags;
  uint32_t fifoDataWord;
  uint32_t addToPacket;
  uint32_t finishPacket;
  uint32_t irqMask;
  DMAStatusPacket *nextStatusHead;

  /* Use the "__" version to avoid using a memory barrier, no need since
   * we're going to the running state
   */
  __set_current_state(TASK_RUNNING);

  do {
    set_current_state(TASK_INTERRUPTIBLE);

    /* Go to sleep only if the status FIFO is empty */
    wait_event_interruptible(dma->statusFifoQueue, (dma->statusReady == DMA_NEW_STATUS_READY) || kthread_should_stop());

    if (kthread_should_stop()) break;

    __set_current_state(TASK_RUNNING);

    /* Set the status flag as "idle"; the status ready IRQ is triggered only
     * when the FIFO goes from an empty to not-empty state, so there's no race
     * condition here.
     */
    dma->statusReady = DMA_STATUS_IDLE;

    /* Read from the status FIFO as long as it is either not empty, or the word
     * at its output was just popped by the last read and hasn't yet been "seen".
     */
    fifoFlags = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_STATUS_FIFO_FLAGS_REG));
    while(((fifoFlags & DMA_STATUS_FIFO_EMPTY) == 0) |
          ((fifoFlags & DMA_STATUS_READ_POPPED) != 0)) {
      /* Presume we aren't adding to or ending the packet (we will check if needed) */
      addToPacket = 0;
      finishPacket = 0;

      /* If there is a word popped by the last read, the begin / end flags in the status
       * register correspond to it right now.  Once we go and read the data word itself,
       * the flags may relate to the *next* popped word, if one exists.
       */
      if((fifoFlags & DMA_STATUS_READ_POPPED) != 0) {
        /* This word will be added to the packet */
        addToPacket = 1;

        /* The read word and flags are valid from the previous read! */
        if((fifoFlags & DMA_STATUS_FIFO_BEGIN) != 0) {
          /* This signifies the beginning of a status packet, reset the index counter. */
          dma->statusIndex = 0;
        } else {
          /* This is either a normal status word or the one flagged as the "end" of a packet */
          dma->statusIndex++;
        }

        /* See if this is the end of the packet - note that it is entirely possible for a
         * single-word packet to be pushed, which has both the "begin" and "end" flags set
         */
        finishPacket = ((fifoFlags & DMA_STATUS_FIFO_END) != 0);
      } /* if(FIFO word ready) */
          
      /* Since we're here, it means that we need to read some FIFO data: either to pop the
       * first word coming out of an empty state, "priming" the process, to process a word
       * which was popped by the last read, or both!
       */
      fifoDataWord = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_STATUS_FIFO_DATA_REG));

      /* Add the new word to the packet data, if it was valid */
      if(addToPacket != 0) {
        dma->statusHead->packetData[dma->statusIndex] = fifoDataWord;

        /* Commit the present packet if this was the last word */
        if(finishPacket != 0) {
          dma->statusHead->packetLength = (dma->statusIndex + 1);
          
          /* Pre-compute the next location of the status head, wrapping to the beginning
           * of the status packet ring when the end has been hit.
           */
          nextStatusHead = (dma->statusHead + 1);
          if((nextStatusHead - dma->statusPackets) >= MAX_STATUS_PACKETS_PER_DGRAM) {
            nextStatusHead = dma->statusPackets;
          }

          /* Check to see whether we are about to "catch up" with the tail pointer;
           * if so, it's time to transmit a Netlink packet of multiple status packets
           * before we overflow with status data.
           */
          if(nextStatusHead == dma->statusTail) tx_netlink_status(dma);

          /* Advance the head pointer to its new location */
          dma->statusHead = nextStatusHead;
        }
      }
          
      /* Re-sample the FIFO status flags for the next iteration */
      fifoFlags = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_STATUS_FIFO_FLAGS_REG));
    } /* while(FIFO words pending) */

    /* We have fallen out of the FIFO-draining loop, meaning that we have emptied
     * it and processed the last word which was available.  Transmit a Netlink
     * packet if there are any complete status packets to be sent.
     */
    tx_netlink_status(dma);

    /* Before returning to waiting, optionally sleep a little bit and then
     * re-enable the IRQs which trigger us.  There should be no need to disable
     * interrupts to prevent a race condition since the only part of the ISR
     * which modifies the IRQ mask is the servicing of the IRQs which are at
     * present disabled.
     *
     * Inserting a delay here, in conjunction with the disabling of the event
     * IRQ flags by the ISR, effectively limits the rate at which events can
     * be generated and sent up to user space.  Using an ISR / waitqueue is,
     * however, more responsive to occasional events than straight-up polling.
     */
    msleep(EVENT_THROTTLE_MSECS);
    irqMask = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG));
    irqMask |= DMA_STAT_READY_IRQ;
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG), irqMask);
  } while(!kthread_should_stop());

  return(0);
}

int32_t labx_dma_probe(struct labx_dma *dma,
                       uint32_t deviceMajor,
                       uint32_t deviceMinor, 
                       const char *name, 
                       int32_t microcodeWords, 
                       int32_t irq,
                       struct labx_dma_callbacks *dmaCallbacks) {
  uint32_t capsWord;
  uint32_t versionWord;
  uint32_t versionMajor;
  uint32_t versionMinor;
  uint32_t versionCompare;
  uint32_t maxMicrocodeWords;
  int32_t returnValue = 0;

  /* Retain the name of the encapsulating device instance, as well as the device
   * node information and IRQ.
   */
  dma->name       = name;
  dma->deviceNode = MKDEV(deviceMajor, deviceMinor);
  dma->irq        = irq;
  dma->callbacks  = dmaCallbacks;
  
  /* Read the capabilities word to determine how many of the lowest
   * address bits are used to index into the microcode RAM, and therefore how
   * many bits an address sub-range field gets shifted up.  Each instruction is
   * 32 bits, and therefore inherently eats two lower address bits.
   */
  capsWord = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_CAPABILITIES_REG));

  /* Inspect and check the version */
  versionWord = XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_REVISION_REG));
  versionMajor = ((versionWord >> DMA_REVISION_FIELD_BITS) & DMA_REVISION_FIELD_MASK);
  versionMinor = (versionWord & DMA_REVISION_FIELD_MASK);
  versionCompare = ((versionMajor << DMA_REVISION_FIELD_BITS) | versionMinor);
  if((versionCompare < DRIVER_VERSION_MIN) | 
     (versionCompare > DRIVER_VERSION_MAX)) {
    dma->regionShift = 0;

    printk(KERN_INFO "Found incompatible hardware version %d.%d at %p\n",
           versionMajor, versionMinor, dma->virtualAddress);
    return(-ENODEV);
  }

  /* Decode the various bits in the capabilities word */
  if (versionCompare < CAPS_INDEX_VERSION) {
    dma->capabilities.indexCounters = 4;
  } else {
    dma->capabilities.indexCounters = (capsWord >> DMA_CAPS_INDEX_SHIFT) & DMA_CAPS_INDEX_MASK;
  }
  dma->capabilities.alus = (capsWord >> DMA_CAPS_ALU_SHIFT) & DMA_CAPS_ALU_MASK;
  dma->capabilities.dmaChannels = (capsWord >> DMA_CAPS_CHANNELS_SHIFT) & DMA_CAPS_CHANNELS_MASK;
  dma->capabilities.parameterAddressBits = (capsWord >> DMA_CAPS_PARAM_ADDRESS_BITS_SHIFT) & DMA_CAPS_PARAM_ADDRESS_BITS_MASK;
  dma->capabilities.codeAddressBits = (capsWord >> DMA_CAPS_CODE_ADDRESS_BITS_SHIFT) & DMA_CAPS_CODE_ADDRESS_BITS_MASK;
  dma->regionShift = (dma->capabilities.codeAddressBits + 2);

  /* Either infer the number of microcode words available from the code address bits,
   * or sanity check the specified amount against the same.
   */
  maxMicrocodeWords = (0x01 << dma->capabilities.codeAddressBits);
  if(microcodeWords <= 0) {
    /* Encapsulating device doesn't know the exact microcode size, assume that
     * the full microcode address space is available for use.
     */
    dma->capabilities.microcodeWords = maxMicrocodeWords;
  } else {
    /* Sanity-check the value we've been provided */
    if(microcodeWords > maxMicrocodeWords) {
      printk(KERN_INFO "(labx-dma, \"%s\") : Microcode size (%d) exceeds maximum of %d words\n",
             dma->name, microcodeWords, maxMicrocodeWords);
      return(-ENODEV);
    }
    dma->capabilities.microcodeWords = microcodeWords;
  }

  /* Check to see if the hardware has a status FIFO */
  dma->capabilities.hasStatusFifo = 
    ((capsWord & DMA_CAPS_STATUS_FIFO_BIT) ? DMA_HAS_STATUS_FIFO : DMA_NO_STATUS_FIFO);

  /* Initialize the waitqueue used for synchronized writes */
  init_waitqueue_head(&(dma->syncedWriteQueue));

  /* Initialize the waitqueue used for status FIFO events */
  init_waitqueue_head(&(dma->statusFifoQueue));

  /* Initialize the data structures storing status information */
  dma->statusIndex = 0;
  dma->statusHead = &dma->statusPackets[0];
  dma->statusTail = &dma->statusPackets[0];

  /* Ensure that no interrupts are enabled */
  XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG), DMA_NO_IRQS);

  /* Run the thread assigned to converting status FIFO words into Netlink packets
   * after initializing its state to wait for the ISR
   */
  dma->statusReady = DMA_STATUS_IDLE;
  dma->netlinkSequence = 0;
  dma->netlinkTask = kthread_run(netlink_thread, (void*) dma, "%s:netlink", dma->name);
  if (IS_ERR(dma->netlinkTask)) {
    printk(KERN_ERR "%s: DMA Netlink task creation failed\n", dma->name);
    return(-EIO);
  }

  /* Call the irq setup function or request the IRQ if one was supplied */
  if((NULL != dma->callbacks) && (NULL != dma->callbacks->irqSetup)) {
    dma->callbacks->irqSetup(dma);
  } else if(dma->irq != DMA_NO_IRQ_SUPPLIED) {
    uint32_t irqMask;

    /* Permit the IRQ to be shared with an enclosing hardware module */
    returnValue = request_irq(dma->irq, 
                              &labx_dma_interrupt, 
                              IRQF_SHARED,
                              dma->name, 
                              dma);
    if (returnValue) {
      printk(KERN_ERR "%s : Could not allocate Lab X DMA interrupt (%d).\n",
             dma->name, dma->irq);
    }

    /* Clear all interrupt flags at the beginning and enable the IRQs:
     *
     * DMA_SYNC_IRQ - Used to detect successful synchronized writes
     * DMA_STAT_READY_IRQ - Used to detect status data arrival in the status FIFO
     *                      (if so equipped).
     */
    irqMask = DMA_SYNC_IRQ;
    if(dma->capabilities.hasStatusFifo == DMA_HAS_STATUS_FIFO) {
      irqMask |= DMA_STAT_READY_IRQ;
    }
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_FLAGS_REG), DMA_ALL_IRQS);
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_IRQ_ENABLE_REG), irqMask);
  }

  /* Make a note if the instance is inferring its microcode size */
  printk(KERN_INFO "\nFound DMA unit %d.%d at %p: %d index counters, %d channels, %d alus\n", versionMajor, versionMinor,
    dma->virtualAddress, dma->capabilities.indexCounters, dma->capabilities.dmaChannels, dma->capabilities.alus);
  printk(KERN_INFO "  %d param bits, %d code bits, %d microcode words%s, %s status FIFO\n",
         dma->capabilities.parameterAddressBits,
         dma->capabilities.codeAddressBits, 
         dma->capabilities.microcodeWords,
         ((microcodeWords <= 0) ? " (INFERRED)" : ""),
         ((dma->capabilities.hasStatusFifo == DMA_HAS_STATUS_FIFO) ? "has" : "no"));

  /* Make a note if the instance has a status FIFO but no IRQ was supplied */
  if((NULL != dma->callbacks) && (NULL != dma->callbacks->irqSetup)) {
    printk(KERN_INFO "  Using supplied IRQ setup.\n\n");
  } else if(dma->irq != DMA_NO_IRQ_SUPPLIED) {
    printk(KERN_INFO "  IRQ %d\n\n", dma->irq);
  } else if(dma->capabilities.hasStatusFifo == DMA_HAS_STATUS_FIFO) {
    printk(KERN_WARNING "  Status FIFO services are unavailable due to lack of an IRQ\n\n");
  } else printk("\n");

  return(returnValue);
}

/* Export the probe function for encapsulating drivers to use */
EXPORT_SYMBOL(labx_dma_probe);

/* Waits for a synchronized write to commit, using either polling or
 * an interrupt-driven waitqueue.
 */
static int32_t await_synced_write(struct labx_dma *dma) {
  int32_t returnValue = 0;

  /* Determine whether to use an interrupt or polling. */
  if(dma->irq != DMA_NO_IRQ_SUPPLIED) {
    int32_t waitResult;

    /* Place ourselves onto a wait queue if the synced write is flagged as
     * pending by the hardware, as this indicates that the microengine is active,
     * and we need to wait for it to finish a pass through all the microcode 
     * before the hardware will commit the write to its destination (a register
     * or microcode RAM.)  If the engine is inactive or the write already 
     * committed, we will not actually enter the wait queue.
     */
    waitResult =
      wait_event_interruptible_timeout(dma->syncedWriteQueue,
                                       ((XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_SYNC_REG)) & DMA_SYNC_PENDING) == 0),
                                       msecs_to_jiffies(SYNCED_WRITE_TIMEOUT_MSECS));

    /* If the wait returns zero, then the timeout elapsed; if negative, a signal
     * interrupted the wait.
     */
    if(waitResult == 0) { 
      if ((XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_SYNC_REG)) & DMA_SYNC_PENDING) == 0) {
        printk("Missed dma sync write queue interrupt, cleared and continuing...\n");
      } else {
        returnValue = -ETIMEDOUT; 
      }
    } else if(waitResult < 0) returnValue = -EAGAIN;
  } else {
    /* No interrupt was supplied during the device probe, simply poll for the bit. */
    while(XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_SYNC_REG)) & DMA_SYNC_PENDING);
  }

  /* Return success or "timed out" */
  return(returnValue);
}

/* Loads the passed microcode descriptor into the instance */
static int32_t load_descriptor(struct labx_dma *dma, ConfigWords *descriptor) {
  uint32_t wordIndex;
  uintptr_t wordAddress;
  uint32_t lastIndex;
  int32_t returnValue = 0;

  if (dma->regionShift == 0)
  {
    printk(KERN_ERR "Trying to load a descriptor on invalid DMA hardware (vaddr = %p)\n", dma->virtualAddress);
    return(-1);
  }

  /* Handle the last write specially for interlocks */
  lastIndex = descriptor->numWords;
  if(descriptor->interlockedLoad) lastIndex--;

  wordAddress = (DMA_MICROCODE_BASE(dma) + (descriptor->offset * sizeof(uint32_t)));
  /*  printk(KERN_INFO "DMA (%p) Descriptor load at %p (%p + %08X)\n", dma, (void*)wordAddress, dma->virtualAddress, descriptor->offset); */
  for(wordIndex = 0; wordIndex < lastIndex; wordIndex++) {
    XIo_Out32(wordAddress, descriptor->configWords[wordIndex]);
    wordAddress += sizeof(uint32_t);
  }

  /* If an interlocked load is requested, issue a sync command on the last write */
  if(descriptor->interlockedLoad) {
    /* Request a synchronized write for the last word and wait for it */
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_SYNC_REG), DMA_SYNC_NEXT_WRITE);
    XIo_Out32(wordAddress, descriptor->configWords[wordIndex]);
    returnValue = await_synced_write(dma);
  }

  return(returnValue);
}

/* Buffer for storing configuration words */
#define MAX_CONFIG_WORDS 1024
static uint32_t configWords[MAX_CONFIG_WORDS];

static int alloc_buffers(struct labx_dma* dma, DMAAlloc* alloc)
{
  /* Reuse configWords space to hold buffer pointers */
  void** pointers = (void**)configWords;
  int i,j;

  for (i=0; i<alloc->nBufs; i++)
  {
    if (alloc->size < 4096)
    {
      /* Force alignment */
      pointers[i] = (void*)(((uintptr_t)(kmalloc(alloc->size*2, GFP_DMA) + alloc->size-1)) & ~(alloc->size-1));
    }
    else
    {
      /* Page size chunks or greater will always be aligned */
      pointers[i] = kmalloc(alloc->size, GFP_DMA);
    }

    /* Zero out the buffers */
    memset(pointers[i], 0, alloc->size);

    if (NULL == pointers[i])
    {
      for (j=0; j<i; j++)
      {
        kfree(pointers[i]);
      }
      return -ENOMEM;
    }
  memset(pointers[i], 0, alloc->size);
  }

  return 0;
}

static int free_buffers(struct labx_dma* dma, DMAAlloc* alloc)
{
  /* Reuse configWords space to hold buffer pointers */
  void** pointers = (void**)configWords;
  int i;

  for (i=0; i<alloc->nBufs; i++)
  {
    kfree(pointers[i]);
  }

  return 0;
}

static void copy_descriptor(struct labx_dma *dma,
                            ConfigWords *descriptor) {
  uint32_t wordIndex;
  uintptr_t wordAddress;

  wordAddress = (DMA_MICROCODE_BASE(dma) + (descriptor->offset * sizeof(uint32_t)));
  for(wordIndex = 0; wordIndex < descriptor->numWords; wordIndex++) {
    descriptor->configWords[wordIndex] = XIo_In32(wordAddress);
    wordAddress += sizeof(uint32_t);
  }
}

/* Disables the engine as a whole; any enabled channels remain enabled */
static void disable_dma(struct labx_dma *dma) {
  XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_CONTROL_REG), DMA_DISABLE);
}

/* Opens the instance for use, placing the hardware into a known state */
int32_t labx_dma_open(struct labx_dma *dma) {
  /* Ensure that all channels and the DMA as a whole are disabled */
  disable_dma(dma);
  XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG), DMA_CHANNELS_NONE);
  return(0);
}
EXPORT_SYMBOL(labx_dma_open);

/* Closes the instance, disabling the hardware */
int32_t labx_dma_release(struct labx_dma *dma) {
  disable_dma(dma);
  return(0);
}
EXPORT_SYMBOL(labx_dma_release);

/* I/O control operations for the driver */
int labx_dma_ioctl(struct labx_dma* dma, unsigned int command, unsigned long arg) {
  int returnValue = 0;

  /* Switch on the request */
  switch(command) {
  case DMA_IOC_START_ENGINE:
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_CONTROL_REG), DMA_ENABLE);
    break;

  case DMA_IOC_STOP_ENGINE:
    disable_dma(dma);
    break;

  case DMA_IOC_LOAD_DESCRIPTOR:
    {
      ConfigWords userDescriptor;
      ConfigWords localDescriptor;

      if(copy_from_user(&userDescriptor, (void __user*)arg, sizeof(userDescriptor)) != 0) {
        return(-EFAULT);
      }

      /* Sanity-check the number of words against our maximum */
      if((userDescriptor.offset + userDescriptor.numWords) > dma->capabilities.microcodeWords) {
    	/* printk("Attempted load @ 0x%08X of %d words, DMA has %d total\n",
			userDescriptor.offset, userDescriptor.numWords, dma->capabilities.microcodeWords); */
        return(-ERANGE);
      }

      localDescriptor.offset          = userDescriptor.offset;
      localDescriptor.interlockedLoad = userDescriptor.interlockedLoad;
      localDescriptor.loadFlags       = userDescriptor.loadFlags;
      localDescriptor.configWords     = configWords;
      while(userDescriptor.numWords > 0) {
        /* Load in chunks, never exceeding our local buffer size */
        localDescriptor.numWords = ((userDescriptor.numWords > MAX_CONFIG_WORDS) ?
                                    MAX_CONFIG_WORDS : userDescriptor.numWords);
        if(copy_from_user(configWords, (void __user*)userDescriptor.configWords, 
                          (localDescriptor.numWords * sizeof(uint32_t))) != 0) {
          return(-EFAULT);
        }
        returnValue                 = load_descriptor(dma, &localDescriptor);
        userDescriptor.configWords += localDescriptor.numWords;
        localDescriptor.offset     += localDescriptor.numWords;
        userDescriptor.numWords    -= localDescriptor.numWords;
        if(returnValue < 0) break;
      }
    }
    break;

  case DMA_IOC_COPY_DESCRIPTOR:
    {
      ConfigWords userDescriptor;
      ConfigWords localDescriptor;

      /* Copy into our local descriptor, then obtain buffer pointer from userland */
      if(copy_from_user(&userDescriptor, (void __user*)arg, sizeof(userDescriptor)) != 0) {
        return(-EFAULT);
      }

      /* Sanity-check the number of words against our maximum */
      if((userDescriptor.offset + userDescriptor.numWords) > dma->capabilities.microcodeWords) {
        return(-ERANGE);
      }

      localDescriptor.offset      = userDescriptor.offset;
      localDescriptor.configWords = configWords;
      while(userDescriptor.numWords > 0) {
        /* Transfer in chunks, never exceeding our local buffer size */
        localDescriptor.numWords = ((userDescriptor.numWords > MAX_CONFIG_WORDS) ? 
                                    MAX_CONFIG_WORDS : userDescriptor.numWords);
        copy_descriptor(dma, &localDescriptor);
        if(copy_to_user((void __user*)userDescriptor.configWords, configWords, 
                        (localDescriptor.numWords * sizeof(uint32_t))) != 0) {
          return(-EFAULT);
        }
        userDescriptor.configWords += localDescriptor.numWords;
        localDescriptor.offset     += localDescriptor.numWords;
        userDescriptor.numWords    -= localDescriptor.numWords;
      }
    }
    break;

  case DMA_IOC_START_CHANNEL:
    /* printk(KERN_INFO "DMA (%p) Start Channel %08X (%p)\n", dma, (int)arg, (void*)DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG)); */
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG), 
              XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG)) | (1<<arg));
    break;

  case DMA_IOC_STOP_CHANNEL:
    /* printk(KERN_INFO "DMA (%p) Stop Channel %08X (%p)\n", dma, (int)arg, (void*)DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG)); */
    XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG), 
      XIo_In32(DMA_REGISTER_ADDRESS(dma, DMA_CHANNEL_ENABLE_REG)) & ~(1<<arg));
    break;

  case DMA_IOC_ALLOC_BUFFERS:
    {
      DMAAlloc alloc;

      if(copy_from_user(&alloc, (void __user*)arg, sizeof(alloc)) != 0) {
        return(-EFAULT);
      }

      if (alloc_buffers(dma, &alloc) < 0) return -ENOMEM;

      if(copy_to_user((void __user*)alloc.buffers, configWords,
                        (alloc.nBufs * sizeof(void*))) != 0) {
        return(-EFAULT);
      }
    }
    break;

  case DMA_IOC_FREE_BUFFERS:
    {
      DMAAlloc alloc;

      if(copy_from_user(&alloc, (void __user*)arg, sizeof(alloc)) != 0) {
        return(-EFAULT);
      }

      if(copy_from_user(&alloc, (void __user*)alloc.buffers, alloc.nBufs * sizeof(void*)) != 0) {
        return(-EFAULT);
      }

      if (free_buffers(dma, &alloc) < 0) return -EFAULT;
    }
    break;

  case DMA_IOC_SET_VECTOR:
    {
      DMAVector vector;
      if(copy_from_user(&vector, (void __user*)arg, sizeof(vector)) != 0) {
        return(-EFAULT);
      }

      XIo_Out32(DMA_REGISTER_ADDRESS(dma, DMA_VECTORS_BASE_ADDRESS + vector.channel), vector.address);
    }
    break;

  case DMA_IOC_GET_CAPS:
    {
      if(copy_to_user((void __user*)arg, &dma->capabilities, sizeof(DMACapabilities)) != 0) {
        return(-EFAULT);
      }
    }
    break;

  default:
    return(-EINVAL);
  }

  return(returnValue);
}
EXPORT_SYMBOL(labx_dma_ioctl);

int32_t labx_dma_remove(struct labx_dma *dma) {
  kthread_stop(dma->netlinkTask);
  if((NULL != dma->callbacks) && (NULL != dma->callbacks->irqTeardown)) {
    dma->callbacks->irqTeardown(dma);
  } else if(dma->irq != DMA_NO_IRQ_SUPPLIED) {
    free_irq(dma->irq, dma);
  }
  return(0);
}
EXPORT_SYMBOL(labx_dma_remove);

/* Driver initialization and exit */
static int __init labx_dma_driver_init(void) {
  int returnValue;

  printk(KERN_INFO DRIVER_NAME ": DMA Coprocessor Driver\n");
  printk(KERN_INFO DRIVER_NAME ": Copyright (c) Lab X Technologies, LLC\n");

  /* Simply register our Generic Netlink resources */
  printk(KERN_INFO DRIVER_NAME ": Registering Lab X DMA Generic Netlink family\n");
  
  /* Register the Generic Netlink family for use */
  returnValue = genl_register_family(&events_genl_family);
  if(returnValue != 0) {
    printk(KERN_INFO DRIVER_NAME ": Failed to register Generic Netlink family\n");
    goto register_failure;
  }

  /* Register multicast groups */
  returnValue = genl_register_mc_group(&events_genl_family, &labx_dma_mcast);
  if(returnValue != 0) {
    printk(KERN_INFO DRIVER_NAME ": Failed to register Generic Netlink multicast group\n");
    genl_unregister_family(&events_genl_family);
    goto register_failure;
  }

 register_failure:
  return(returnValue);
}

static void __exit labx_dma_driver_exit(void) {
  /* Unregister the Generic Netlink family */
  genl_unregister_family(&events_genl_family);
}

module_init(labx_dma_driver_init);
module_exit(labx_dma_driver_exit);

MODULE_AUTHOR("Chris Wulff");
MODULE_LICENSE("GPL");
