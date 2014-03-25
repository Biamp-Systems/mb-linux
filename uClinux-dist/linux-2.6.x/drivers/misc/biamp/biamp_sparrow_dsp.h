/*
 *  linux/drivers/net/labx_avb/labx_tdm_audio.h
 *
 *  Lab X Technologies Audio time division multiplexing driver
 *
 *  Written by Albert M. Hajjar (albert.hajjar@labxtechnologies.com)
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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#define NAME_MAX_SIZE    (256)

#if 0
#define DBG(f, x...) printk(DRIVER_NAME " [%s()]: " f, __func__,## x)
#else
#define DBG(f, x...)
#endif

#define REGISTER_ADDRESS(device, offset) \
  ((uintptr_t)device->virtualAddress | (offset << 2))

#define NO_IRQ_SUPPLIED   (-1)

struct sparrow_dsp {
  /* Misc device */
  struct miscdevice miscdev;  

  /* Pointer back to the platform device */
  struct platform_device *pdev;

  /* Mutex for the device instance */
  spinlock_t mutex;
  bool opened;
  
  /* File operations and private data for a polymorphic
   * driver to use
   */
  struct file_operations *derivedFops;
  void *derivedData;
    
  /* Name for use in identification */
  char name[NAME_MAX_SIZE];
  /* Device version */
  uint32_t version;
      
  /* Physical and virtual base address */
  uintptr_t      physicalAddress;
  uintptr_t      addressRangeSize;
  void __iomem  *virtualAddress;

  /* Interrupt request number */
  int32_t irq;
             
  uint32_t initialVal;

};
