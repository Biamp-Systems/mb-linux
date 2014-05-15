/*
 *  linux/drivers/misc/biamp/biamp_sparrow_dsp.c
 *
 *  Lab X Technologies AVB time domain multiplexer (TDM) driver
 *
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

#include "linux/biamp_sparrow_dsp_defs.h"
#include "biamp_sparrow_dsp.h"
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/platform_device.h>
#include <xio.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_platform.h>
#endif // CONFIG_OF

 //#define _LABXDEBUG

/* Driver name and the revision range of hardware expected.
 * This driver will work with revision 1.1 only.
  */
#define DRIVER_NAME "biamp_sparrow_dsp"
#define DRIVER_VERSION_MIN  0x10
#define DRIVER_VERSION_MAX  0x10
#define REVISION_FIELD_BITS  4
#define REVISION_FIELD_MASK  (0x0F)

/* Maximum number of dsp modules and instance count */
#define MAX_INSTANCES 16
static uint32_t instanceCount;

static struct sparrow_dsp* devices[MAX_INSTANCES] = {};
static bool debugOn = false;

/*
 * Character device hook functions
 */

static int dsp_open(struct inode *inode, struct file *filp)
{
  struct sparrow_dsp *dsp;
  unsigned long flags;
  int returnValue = 0;
  int deviceIndex;

  /* Search for the device amongst those which successfully were probed */
  for(deviceIndex = 0; deviceIndex < MAX_INSTANCES; deviceIndex++) {
    if ((devices[deviceIndex] != NULL) && (devices[deviceIndex]->miscdev.minor == iminor(inode))) {
      /* Retain the device pointer within the file pointer for future navigation */
      filp->private_data = devices[deviceIndex];
      break;
    }
  }

  /* Ensure the device was actually found */
  if(deviceIndex >= MAX_INSTANCES) {
    printk(KERN_WARNING DRIVER_NAME ": Could not find device for node (%d, %d)\n",
           imajor(inode), iminor(inode));
    return(-ENODEV);
  }

  /* TODO - Ensure the local audio hardware is reset */
  dsp = (struct sparrow_dsp*)filp->private_data;

  /* Lock the mutex and ensure there is only one owner */
  preempt_disable();
  spin_lock_irqsave(&dsp->mutex, flags);
  if(dsp->opened) {
    returnValue = -1;
  } else {
    dsp->opened = true;
  }

  /* Invoke the open() operation on the derived driver, if there is one */
  if((dsp->derivedFops != NULL) &&
     (dsp->derivedFops->open != NULL)) {
    dsp->derivedFops->open(inode, filp);
  }

  spin_unlock_irqrestore(&dsp->mutex, flags);
  preempt_enable();
  
  return(returnValue);
}

static int dsp_release(struct inode *inode, struct file *filp)
{
  struct sparrow_dsp *dsp = (struct sparrow_dsp *)filp->private_data;
  unsigned long flags;

  preempt_disable();
  spin_lock_irqsave(&dsp->mutex, flags);
  dsp->opened = false;

  /* Invoke the release() operation on the derived driver, if there is one */
  if((dsp->derivedFops != NULL) &&
     (dsp->derivedFops->release != NULL)) {
    dsp->derivedFops->release(inode, filp);
  }

  spin_unlock_irqrestore(&dsp->mutex, flags);
  preempt_enable();
  return(0);
}

static void set_sparrow_dsp_data(struct sparrow_dsp *dsp,
                                SparrowDspInfo *dspInfo) {
  XIo_Out32(REGISTER_ADDRESS(dsp, dspInfo->addr), dspInfo->data);
}

static void get_sparrow_dsp_data(struct sparrow_dsp *dsp,
                                SparrowDspInfo *dspInfo) {
  dspInfo->data = XIo_In32(REGISTER_ADDRESS(dsp, dspInfo->addr));
}

static void set_sparrow_dsp_data_bit(struct sparrow_dsp *dsp,
                                     SparrowDspInfo *dspInfo) {
  uint32_t reg = XIo_In32(REGISTER_ADDRESS(dsp, dspInfo->addr));
  reg |= dspInfo->mask;
  XIo_Out32(REGISTER_ADDRESS(dsp, dspInfo->addr), reg);
}

static void clear_sparrow_dsp_data_bit(struct sparrow_dsp *dsp,
                                            SparrowDspInfo *dspInfo) {
  uint32_t reg = XIo_In32(REGISTER_ADDRESS(dsp, dspInfo->addr));
  reg &= ~dspInfo->mask;
  XIo_Out32(REGISTER_ADDRESS(dsp, dspInfo->addr), reg);
}

static void modify_sparrow_dsp_data(struct sparrow_dsp *dsp,
                                    SparrowDspInfo *dspInfo) {
  uint32_t reg = XIo_In32(REGISTER_ADDRESS(dsp, dspInfo->addr));
  reg &= (~dspInfo->mask);
  reg |= (dspInfo->data & dspInfo->mask);
  XIo_Out32(REGISTER_ADDRESS(dsp, dspInfo->addr), reg);
}

/* I/O control operations for the driver */
static int dsp_ioctl(struct inode *inode,
                     struct file *filp,
                     unsigned int command,
                     unsigned long arg) {
  // Switch on the request
  int returnValue = 0;
  SparrowDspInfo dspInfo;
  struct sparrow_dsp *dsp = (struct sparrow_dsp *)filp->private_data;

  switch(command) {
  case IOC_GET_SPARROW_DSP_REG:
    if(copy_from_user(&dspInfo, (void __user*)arg, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    get_sparrow_dsp_data(dsp, &dspInfo);
    if(copy_to_user((void __user*)arg, &dspInfo, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    break;
  
  case IOC_SET_SPARROW_DSP_REG:
    if(copy_from_user(&dspInfo, (void __user*)arg, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    set_sparrow_dsp_data(dsp, &dspInfo);
    break;

  case IOC_SET_SPARROW_DSP_REG_BIT:
    if(copy_from_user(&dspInfo, (void __user*)arg, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    set_sparrow_dsp_data_bit(dsp, &dspInfo);
    break;
  
  case IOC_CLEAR_SPARROW_DSP_REG_BIT:
    if(copy_from_user(&dspInfo, (void __user*)arg, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    clear_sparrow_dsp_data_bit(dsp, &dspInfo);
    break;
  
  case IOC_MODIFY_SPARROW_DSP_REG:
    if(copy_from_user(&dspInfo, (void __user*)arg, sizeof(SparrowDspInfo)) != 0) {
      return(-EFAULT);
    }
    modify_sparrow_dsp_data(dsp, &dspInfo);
    break;
  
  default:
    if(debugOn) {
      printk("TDM (ioctl): bad ioctl call (command = %u)\n", command);
    }
    returnValue = -EINVAL;
    break;

  }
  /* Return an error code appropriate to the command */
  return(returnValue);
}

/* Interrupt service routine for the driver */
static irqreturn_t biamp_sparrow_dsp_interrupt(int irq, void *dev_id) {
  //struct sparrow_dsp *dsp = (struct sparrow_dsp*) dev_id;
  //uint32_t maskedFlags;
  //uint32_t irqMask;

  // Read the interrupt flags and immediately clear them 
  //maskedFlags = XIo_In32(REGISTER_ADDRESS(tdm, TDM_IRQ_FLAGS_REG));
  //irqMask = XIo_In32(REGISTER_ADDRESS(tdm, TDM_IRQ_MASK_REG));
  //maskedFlags &= irqMask;
  //XIo_Out32(REGISTER_ADDRESS(tdm, TDM_IRQ_FLAGS_REG), maskedFlags);

  // Detect the DMA error IRQ 
  //if((maskedFlags & DMA_ERROR_IRQ) != 0) {
    // TEMPORARY - Just announce this and treat it as a one-shot.
    //             Ultimately this should be communicated via generic Netlink.
    //irqMask &= ~DMA_ERROR_IRQ;
    //XIo_Out32(REGISTER_ADDRESS(tdm, TDM_IRQ_MASK_REG), irqMask);
    //printk("%s: DMA error!\n", tdm->name);
  //}
  
  return(IRQ_HANDLED);
}

/* Character device file operations structure */
static struct file_operations dsp_fops = {
  .open	   = dsp_open,
  .release = dsp_release,
  .ioctl   = dsp_ioctl,
  .owner   = THIS_MODULE,
};

/* Resets the state of the passed instance */
static void reset_dsp(struct sparrow_dsp *dsp) {

  /* Restore the instance registers to initial values */
  
  return;
}

/* Function containing the "meat" of the probe mechanism - this is used by
 * the OpenFirmware probe as well as the standard platform device mechanism.
 * This is exported to allow polymorphic drivers to invoke it.
 * @param name - Name of the instance
 * @param pdev - Platform device structure
 * @param addressRange - Resource describing the hardware's I/O range
 * @param irq          - Resource describing the hardware's IRQ
 * @param derivedData  - Pointer for derived driver to make use of
 * @param newInstance  - Pointer to the new driver instance, NULL if unused
 */
int sparrow_dsp_probe(const char *name,
                      struct platform_device *pdev,
                      struct resource *addressRange,
                      struct resource *irq,
                      void *derivedData,
                      struct sparrow_dsp **newInstance) {
  struct sparrow_dsp *dsp;
  unsigned int versionMajor;
  unsigned int versionMinor;
  unsigned int versionCompare;
  int returnValue;
  uint32_t deviceIndex;

  /* Create and populate a device structure */
  dsp = (struct sparrow_dsp*) kzalloc(sizeof(struct sparrow_dsp), GFP_KERNEL);
  if(!dsp) return(-ENOMEM);

  dsp->physicalAddress = addressRange->start;
  dsp->addressRangeSize = ((addressRange->end - addressRange->start) + 1);

  snprintf(dsp->name, NAME_MAX_SIZE, "%s%u", name, instanceCount++);
  dsp->name[NAME_MAX_SIZE - 1] = '\0';
  if(request_mem_region(dsp->physicalAddress, dsp->addressRangeSize,
		  dsp->name) == NULL) {
    returnValue = -ENOMEM;
    goto free;
  }

  dsp->virtualAddress =
    (void*) ioremap_nocache(dsp->physicalAddress, dsp->addressRangeSize);
  if(!dsp->virtualAddress) {
    returnValue = -ENOMEM;
    goto release;
  }

  printk("DSP virtualAddress = 0x%08X, phys = 0x%08X, size = 0x%08X\n",
         (uint32_t) dsp->virtualAddress,
         (uint32_t) dsp->physicalAddress,
         (uint32_t) dsp->addressRangeSize);

  /* Retain the IRQ and register our handler, if an IRQ resource was supplied.
   * For now, there is no TDM IRQ, so this is unused.
   */
  if(irq != NULL) {
    dsp->irq = irq->start;
    returnValue = request_irq(dsp->irq, &biamp_sparrow_dsp_interrupt, IRQF_DISABLED,
    		              dsp->name, dsp);
    if (returnValue) {
      printk(KERN_ERR "%s : Could not allocate Biamp Sparrow DSP interrupt (%d).\n",
    		  dsp->name, dsp->irq);
      goto unmap;
    }
  } else dsp->irq = NO_IRQ_SUPPLIED;

  /* Inspect and check the version to ensure it lies within the range of hardware
   * we support.  For now, there is no TDM version register, so this is hardwired.
   */
#if 0
  dsp->version = XIo_In32(REGISTER_ADDRESS(dsp, DSP_REVISION_REG));
#else
  dsp->version = DRIVER_VERSION_MIN;
#endif
  versionMajor = ((dsp->version >> REVISION_FIELD_BITS) & REVISION_FIELD_MASK);
  versionMinor = (dsp->version & REVISION_FIELD_MASK);
  versionCompare = ((versionMajor << REVISION_FIELD_BITS) | versionMinor);
  if((versionCompare < DRIVER_VERSION_MIN) | 
     (versionCompare > DRIVER_VERSION_MAX)) {
    printk(KERN_INFO "%s: Found incompatible hardware version %u.%u at 0x%08X\n",
    		dsp->name, versionMajor, versionMinor, (uint32_t)dsp->physicalAddress);
    returnValue = -ENXIO;
    goto unmap;
  }
 
  /* Initialize other resources */
  spin_lock_init(&dsp->mutex);
  dsp->opened = false;

  /* Reset the state of the dsp */
  reset_dsp(dsp);

  /* Add as a misc device */
  dsp->miscdev.minor = MISC_DYNAMIC_MINOR;
  dsp->miscdev.name = dsp->name;
  dsp->miscdev.fops = &dsp_fops;
  returnValue = misc_register(&dsp->miscdev);
  if (returnValue) {
    printk(KERN_WARNING DRIVER_NAME ": Unable to register misc device.\n");
    goto unmap;
  }

  /* Provide navigation between the device structures */
  platform_set_drvdata(pdev, dsp);
  dsp->pdev = pdev;
  dev_set_drvdata(dsp->miscdev.this_device, dsp);

  /* Announce the device */
  printk(KERN_INFO "%s: Found Biamp Systems Sparrow DSP v %u.%u at 0x%08X", dsp->name, versionMajor, versionMinor, (uint32_t)dsp->physicalAddress);

  /* Locate and occupy the first available device index for future navigation in
   * the call to dsp_open()
   */
  for (deviceIndex = 0; deviceIndex < MAX_INSTANCES; deviceIndex++) {
    if (NULL == devices[deviceIndex]) {
      devices[deviceIndex] = dsp;
      break;
    }
  }

  /* Ensure that we haven't been asked to probe for too many devices */
  if(deviceIndex >= MAX_INSTANCES) {
    printk(KERN_WARNING DRIVER_NAME ": Maximum device count (%d) exceeded during probe\n",
           MAX_INSTANCES);
    goto unmap;
  }

  /* Return success, setting the return pointer if valid */
  if(newInstance != NULL) *newInstance = dsp;
  return(returnValue);

 unmap:
  iounmap(dsp->virtualAddress);
 release:
  release_mem_region(dsp->physicalAddress, dsp->addressRangeSize);
 free:
  kfree(dsp);
  return(returnValue);
}

#ifdef CONFIG_OF

static int sparrow_dsp_platform_remove(struct platform_device *pdev);

/* Probe for registered devices */
static int __devinit sparrow_dsp_of_probe(struct of_device *ofdev, const struct of_device_id *match)
{
  struct resource r_mem_struct = {};
  struct resource r_irq_struct = {};
  struct resource *addressRange = &r_mem_struct;
  struct resource *irq          = &r_irq_struct;
  struct platform_device *pdev  = to_platform_device(&ofdev->dev);
  const char *full_name = dev_name(&ofdev->dev);
  const char *name;
  int rc = 0;

  printk(KERN_INFO "Device Tree Probing \'%s\'\n",ofdev->node->name);

  if ((name = strchr(full_name, '.')) == NULL) {
	  name = full_name;
  } else {
	  ++name;
  }
  /* Obtain the resources for this instance */
  rc = of_address_to_resource(ofdev->node, 0, addressRange);
  if(rc) {
    dev_warn(&ofdev->dev, "Invalid address\n");
    return(rc);
  }

  rc = of_irq_to_resource(ofdev->node, 0, irq);
  if(rc == NO_IRQ) {
    /* No IRQ was defined; null the resource pointer to indicate interrupt unused */
    irq = NULL;
  }

  /* Dispatch to the generic function */
  return(sparrow_dsp_probe(name, pdev, addressRange, irq, NULL, NULL));
}

static int __devexit sparrow_dsp_of_remove(struct of_device *dev)
{
  struct platform_device *pdev = to_platform_device(&dev->dev);
  sparrow_dsp_platform_remove(pdev);
  return(0);
}


/* Directly compatible with Biamp Systems Sparrow DSP peripherals.
 *
 */
static struct of_device_id dsp_of_match[] = {
  { .compatible = "xlnx,biamp-sparrow-dsp-1.00.a", },
  { /* end of list */ },
};


static struct of_platform_driver of_sparrow_dsp_driver = {
  .name		= DRIVER_NAME,
  .match_table	= dsp_of_match,
  .probe   	= sparrow_dsp_of_probe,
  .remove       = __devexit_p(sparrow_dsp_of_remove),
};
#endif

/*
 * Platform device hook functions
 */

/* Probe for registered devices */
static int sparrow_dsp_platform_probe(struct platform_device *pdev) {
  struct resource *addressRange;
  struct resource *irq;

  /* Obtain the resources for this instance */
  addressRange = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!addressRange) {
    printk(KERN_ERR "%s: IO resource address not found.\n", pdev->name);
    return(-ENXIO);
  }

  /* Attempt to obtain the IRQ; if none is specified, the resource pointer is
   * NULL, and polling will be used.
   */
  irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

  /* Dispatch to the generic function */
  return(sparrow_dsp_probe(pdev->name, pdev, addressRange, irq, NULL, NULL));
}

/* This is exported to allow polymorphic drivers to invoke it. */
int sparrow_dsp_remove(struct sparrow_dsp *dsp) {
  int deviceIndex;
  
  reset_dsp(dsp);
  iounmap(dsp->virtualAddress);
  release_mem_region(dsp->physicalAddress, dsp->addressRangeSize);
  kfree(dsp);

  misc_deregister(&dsp->miscdev);

  for (deviceIndex = 0; deviceIndex < MAX_INSTANCES; deviceIndex++) {
    if (dsp == devices[deviceIndex]) {
      devices[deviceIndex] = NULL;
      break;
    }
  }
  return(0);
}

static int sparrow_dsp_platform_remove(struct platform_device *pdev) {
  struct sparrow_dsp *dsp;

  /* Get a handle to the dsp and begin shutting it down */
  dsp = platform_get_drvdata(pdev);
  if(!dsp) return(-1);
  return(sparrow_dsp_remove(dsp));
}

/* Platform device driver structure */
static struct platform_driver sparrow_dsp_driver = {
  .probe  = sparrow_dsp_platform_probe,
  .remove = sparrow_dsp_platform_remove,
  .driver = {
  .name = DRIVER_NAME,
  }
};

/* Driver initialization and exit */
static int __init sparrow_dsp_driver_init(void)
{
  int returnValue;
  printk(KERN_INFO DRIVER_NAME ": Sparrow DSP driver\n");
  printk(KERN_INFO DRIVER_NAME ": Copyright(c) Biamp Systems\n");

#ifdef CONFIG_OF
  returnValue = of_register_platform_driver(&of_sparrow_dsp_driver);
#endif

  /* Initialize the instance counter */ 
  instanceCount = 0;

  /* Register as a platform device driver */
  if((returnValue = platform_driver_register(&sparrow_dsp_driver)) < 0) {
    printk(KERN_INFO DRIVER_NAME ": Failed to register platform driver\n");
    return(returnValue);
  }

  return(0);
}

static void __exit sparrow_dsp_driver_exit(void)
{
  /* Unregister as a platform device driver */
  platform_driver_unregister(&sparrow_dsp_driver);
}

module_init(sparrow_dsp_driver_init);
module_exit(sparrow_dsp_driver_exit);

MODULE_AUTHOR("Albert M. Hajjar <albert.hajjar@labxtechnologies.com");
MODULE_DESCRIPTION("Biamp Systems Sparrow DSP Driver");
MODULE_LICENSE("GPL");
