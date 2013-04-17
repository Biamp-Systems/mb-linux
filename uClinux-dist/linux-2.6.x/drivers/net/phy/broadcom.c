/*
 *	drivers/net/phy/broadcom.c
 *
 *	Broadcom BCM5411, BCM5421 and BCM5461 Gigabit Ethernet
 *	transceivers.
 *
 *	Copyright (c) 2006  Maciej W. Rozycki
 *
 *	Inspired by code written by Amy Fong.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/broadcom_leds.h>
#include <linux/err.h>

#define DUMP_PHY_REGISTERS 1

#define PHY_ID_BCM50610			0x0143bd60
#define PHY_ID_BCM54610			0x0143bd63

#define MII_BCM54XX_ECR			0x10	/* BCM54xx extended control register */
#define MII_BCM54XX_ECR_IM		0x1000	/* Interrupt mask */
#define MII_BCM54XX_ECR_IF		0x0800	/* Interrupt force */

#define MII_BCM54XX_ESR			0x11	/* BCM54xx extended status register */
#define MII_BCM54XX_ESR_IS		0x1000	/* Interrupt status */

#define MII_BCM54XX_EXP_DATA		0x15	/* Expansion register data */
#define MII_BCM54XX_EXP_SEL		0x17	/* Expansion register select */
#define MII_BCM54XX_RCV_ERR_CNTR        0x12    /* Receive error counter */
#define MII_BCM54XX_RCV_NOT_OK_CNTR	0x14    /* Receiver NOT_OK Counter */
#define MII_BCM54XX_EXP_SEL_SSD		0x0e00	/* Secondary SerDes select */
#define MII_BCM54XX_EXP_SEL_ER		0x0f00	/* Expansion register select */

#define MII_BCM54XX_AUX_CTL		0x18	/* Auxiliary control register */
#define MII_BCM54XX_ISR			0x1a	/* BCM54xx interrupt status register */
#define MII_BCM54XX_IMR			0x1b	/* BCM54xx interrupt mask register */
#define MII_BCM54XX_TEST_REG1		0x1e    /* BCM54xx test register 1 */
#define MII_BCM54XX_INT_CRCERR		0x0001	/* CRC error */
#define MII_BCM54XX_INT_LINK		0x0002	/* Link status changed */
#define MII_BCM54XX_INT_SPEED		0x0004	/* Link speed change */
#define MII_BCM54XX_INT_DUPLEX		0x0008	/* Duplex mode changed */
#define MII_BCM54XX_INT_LRS		0x0010	/* Local receiver status changed */
#define MII_BCM54XX_INT_RRS		0x0020	/* Remote receiver status changed */
#define MII_BCM54XX_INT_SSERR		0x0040	/* Scrambler synchronization error */
#define MII_BCM54XX_INT_UHCD		0x0080	/* Unsupported HCD negotiated */
#define MII_BCM54XX_INT_NHCD		0x0100	/* No HCD */
#define MII_BCM54XX_INT_NHCDL		0x0200	/* No HCD link */
#define MII_BCM54XX_INT_ANPR		0x0400	/* Auto-negotiation page received */
#define MII_BCM54XX_INT_LC		0x0800	/* All counters below 128 */
#define MII_BCM54XX_INT_HC		0x1000	/* Counter above 32768 */
#define MII_BCM54XX_INT_MDIX		0x2000	/* MDIX status change */
#define MII_BCM54XX_INT_PSERR		0x4000	/* Pair swap error */

#define MII_BCM54XX_SHD			0x1c	/* 0x1c shadow registers */
#define MII_BCM54XX_SHD_WRITE		0x8000
#define MII_BCM54XX_SHD_VAL(x)		((x & 0x1f) << 10)
#define MII_BCM54XX_SHD_DATA(x)		((x & 0x3ff) << 0)

// Control for PHY REG 0x00

#define BCM5481_AUTO_NEGOTIATE_ENABLE           0x1000

/*
 * AUXILIARY CONTROL SHADOW ACCESS REGISTERS.  (PHY REG 0x18)
 */
#define MII_BCM54XX_AUXCTL_SHADOW_MASK 7
#define MII_BCM54XX_AUXCTL_SHADOW_READSHIFT 12

#define MII_BCM54XX_AUXCTL_ACTL_TX_6DB		0x0400
#define MII_BCM54XX_AUXCTL_ACTL_SMDSP_ENA	0x0800

#define MII_BCM54XX_AUXCTL_MISC_WREN	        0x8000
#define MII_BCM54XX_AUXCTL_PMII_HPE             0x0040
#define MII_BCM54XX_AUXCTL_MISC_FORCE_AMDIX	0x0200
#define MII_BCM54XX_AUXCTL_MISC_RDSEL_MISC	0x7000

#define MII_BCM54XX_AUXCTL_SHDWSEL_MISC 	0x0007
#define MII_BCM54XX_AUXCTL_SHDWSEL_AUXCTL	0x0000
#define MII_BCM54XX_AUXCTL_SHDWSEL_PMII         0x0002

// Control for CRC error counter selector
#define MII_BCM54XX_RECEIVER_NOT_OK		0x8000

/*
 * Broadcom LED source encodings.  These are used in BCM5461, BCM5481,
 * BCM5482, and possibly some others.
 */
#define BCM_LED_SRC_LINKSPD1	0x0
#define BCM_LED_SRC_LINKSPD2	0x1
#define BCM_LED_SRC_XMITLED	0x2
#define BCM_LED_SRC_ACTIVITYLED	0x3
#define BCM_LED_SRC_FDXLED	0x4
#define BCM_LED_SRC_SLAVE	0x5
#define BCM_LED_SRC_INTR	0x6
#define BCM_LED_SRC_QUALITY	0x7
#define BCM_LED_SRC_RCVLED	0x8
#define BCM_LED_SRC_MULTICOLOR1	0xa
#define BCM_LED_SRC_OPENSHORT	0xb
#define BCM_LED_SRC_OFF		0xe	/* Tied high */
#define BCM_LED_SRC_ON		0xf	/* Tied low */

/*
 * BCM5481: Shadow registers
 * Shadow values go into bits [14:10] of register 0x1c to select a shadow
 * register to access.
 */
#define BCM5481_SHD_CLKALIGN 0x03	/* 00011: Clock Alignment Control register */
#define BCM5481_SHD_DELAY_ENA (1 << 9)	/* RGMII Transmit Clock Delay enable */
#define BCM5481_AUX_SKEW_ENA  (1 << 8)	/* RGMII RXD to RXC Skew enable */

/*
 * BCM54XX: Shadow registers
 * Shadow values go into bits [14:10] of register 0x1c to select a shadow
 * register to access.
 */
#define BCM54XX_SHD_LEDS12	0x0d	/* 01101: LED 1 & 2 Selector */
#define BCM54XX_SHD_LEDS34	0x0e	/* 01110: LED 3 & 4 Selector */
#define BCM54XX_SHD_LEDS_LEDH(src)	((src & 0xf) << 4)
#define BCM54XX_SHD_LEDS_LEDL(src)	((src & 0xf) << 0)

/*
 * BCM5482: Shadow registers
 * Shadow values go into bits [14:10] of register 0x1c to select a shadow
 * register to access.
 */
#define BCM5482_SPARE_CTRL3     0x05    /* 00101: Spare Control 3 */
#define BCM5482_SHD_LEDS1	0x0d	/* 01101: LED Selector 1 */
					/* LED3 / ~LINKSPD[2] selector */
#define BCM5482_SHD_LEDS1_LED3(src)	((src & 0xf) << 4)
					/* LED1 / ~LINKSPD[1] selector */
#define BCM5482_SHD_LEDS1_LED1(src)	((src & 0xf) << 0)
#define BCM5482_SHD_SSD		0x14	/* 10100: Secondary SerDes control */
#define BCM5482_SHD_SSD_LEDM	0x0008	/* SSD LED Mode enable */
#define BCM5482_SHD_SSD_EN	0x0001	/* SSD enable */
#define BCM5482_SHD_MODE	0x1f	/* 11111: Mode Control Register */
#define BCM5482_SHD_MODE_1000BX	0x0001	/* Enable 1000BASE-X registers */
#define BCM5482_PRIM_SERD_CTRL  0x16
#define BCM5482_MISC_1000_CTRL2 0x17
#define BCM5482_SIGDETECT_EN    0x20  
#define BCM5482_CLK125_OUTPUT   0x01    /* Bit 0: Enable CLK125 output */ 

/*
 * EXPANSION SHADOW ACCESS REGISTERS.  (PHY REG 0x15, 0x16, and 0x17)
 */
#define MII_BCM54XX_EXP_AADJ1CH0		0x001f
#define  MII_BCM54XX_EXP_AADJ1CH0_SWP_ABCD_OEN	0x0200
#define  MII_BCM54XX_EXP_AADJ1CH0_SWSEL_THPF	0x0100
#define MII_BCM54XX_EXP_AADJ1CH3		0x601f
#define  MII_BCM54XX_EXP_AADJ1CH3_ADCCKADJ	0x0002
#define MII_BCM54XX_EXP_EXP08			0x0F08
#define  MII_BCM54XX_EXP_EXP08_RJCT_2MHZ	0x0001
#define  MII_BCM54XX_EXP_EXP08_EARLY_DAC_WAKE	0x0200
#define MII_BCM54XX_EXP_EXP75			0x0f75
#define  MII_BCM54XX_EXP_EXP75_VDACCTRL		0x003c
#define MII_BCM54XX_EXP_EXP96			0x0f96
#define  MII_BCM54XX_EXP_EXP96_MYST		0x0010
#define MII_BCM54XX_EXP_EXP97			0x0f97
#define  MII_BCM54XX_EXP_EXP97_MYST		0x0c0c
#define MII_BCM54XX_MULTICOLOR_LED              0x0004

/*
 * BCM5482: Secondary SerDes registers
 */
#define BCM5482_SSD_1000BX_CTL		0x00	/* 1000BASE-X Control */
#define BCM5482_SSD_1000BX_CTL_PWRDOWN	0x0800	/* Power-down SSD */
#define BCM5482_SSD_SGMII_SLAVE		0x15	/* SGMII Slave Register */
#define BCM5482_SSD_SGMII_SLAVE_EN	0x0002	/* Slave mode enable */
#define BCM5482_SSD_SGMII_SLAVE_AD	0x0001	/* Slave auto-detection */

/*
 * Device flags for PHYs that can be configured for different operating
 * modes.
 */
#define PHY_BCM_FLAGS_VALID		0x80000000
#define PHY_BCM_FLAGS_INTF_XAUI		0x00000020
#define PHY_BCM_FLAGS_INTF_SGMII	0x00000010
#define PHY_BCM_FLAGS_MODE_1000BX	0x00000002
#define PHY_BCM_FLAGS_MODE_COPPER	0x00000001

MODULE_DESCRIPTION("Broadcom PHY driver");
MODULE_AUTHOR("Maciej W. Rozycki");
MODULE_LICENSE("GPL");

#define DRIVER_NAME "broadcom"
#define MAX_LED_PHYS 8
static struct phy_device *aPhys[MAX_LED_PHYS];
static int phy_instances = 0;

/*
 * Indirect register access functions for the 1000BASE-T/100BASE-TX/10BASE-T
 * 0x1c shadow registers.
 */
static int bcm54xx_shadow_read(struct phy_device *phydev, u16 shadow)
{
	phy_write(phydev, MII_BCM54XX_SHD, MII_BCM54XX_SHD_VAL(shadow));
	return MII_BCM54XX_SHD_DATA(phy_read(phydev, MII_BCM54XX_SHD));
}

static int bcm54xx_shadow_write(struct phy_device *phydev, u16 shadow, u16 val)
{
	return phy_write(phydev, MII_BCM54XX_SHD,
			 MII_BCM54XX_SHD_WRITE |
			 MII_BCM54XX_SHD_VAL(shadow) |
			 MII_BCM54XX_SHD_DATA(val));
}

/* Indirect register access functions for the Expansion Registers */
static int bcm54xx_exp_read(struct phy_device *phydev, u8 regnum)
{
	int val;

	val = phy_write(phydev, MII_BCM54XX_EXP_SEL, regnum);
	if (val < 0)
		return val;

	val = phy_read(phydev, MII_BCM54XX_EXP_DATA);

	/* Restore default value.  It's O.K. if this write fails. */
	phy_write(phydev, MII_BCM54XX_EXP_SEL, 0);

	return val;
}

static int bcm54xx_exp_write(struct phy_device *phydev, u16 regnum, u16 val)
{
	int ret;

	ret = phy_write(phydev, MII_BCM54XX_EXP_SEL, regnum);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MII_BCM54XX_EXP_DATA, val);

	/* Restore default value.  It's O.K. if this write fails. */
	phy_write(phydev, MII_BCM54XX_EXP_SEL, 0);

	return ret;
}

static int bcm54xx_auxctl_read(struct phy_device *phydev, u16 shadow)
{
        phy_write(phydev, MII_BCM54XX_AUX_CTL, ((shadow << MII_BCM54XX_AUXCTL_SHADOW_READSHIFT) | MII_BCM54XX_AUXCTL_SHDWSEL_MISC));
	return phy_read(phydev, MII_BCM54XX_AUX_CTL);
}

static int bcm54xx_auxctl_write(struct phy_device *phydev, u16 shadow, u16 val)
{
	return phy_write(phydev, MII_BCM54XX_AUX_CTL, ((val & ~MII_BCM54XX_AUXCTL_SHADOW_MASK) | shadow));
}

static int bcm50610_a0_workaround(struct phy_device *phydev)
{
	int err;

	err = bcm54xx_auxctl_write(phydev,
				   MII_BCM54XX_AUXCTL_SHDWSEL_AUXCTL,
				   MII_BCM54XX_AUXCTL_ACTL_SMDSP_ENA |
				   MII_BCM54XX_AUXCTL_ACTL_TX_6DB);
	if (err < 0)
		return err;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_EXP08,
				MII_BCM54XX_EXP_EXP08_RJCT_2MHZ	|
				MII_BCM54XX_EXP_EXP08_EARLY_DAC_WAKE);
	if (err < 0)
		goto error;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_AADJ1CH0,
				MII_BCM54XX_EXP_AADJ1CH0_SWP_ABCD_OEN |
				MII_BCM54XX_EXP_AADJ1CH0_SWSEL_THPF);
	if (err < 0)
		goto error;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_AADJ1CH3,
					MII_BCM54XX_EXP_AADJ1CH3_ADCCKADJ);
	if (err < 0)
		goto error;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_EXP75,
				MII_BCM54XX_EXP_EXP75_VDACCTRL);
	if (err < 0)
		goto error;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_EXP96,
				MII_BCM54XX_EXP_EXP96_MYST);
	if (err < 0)
		goto error;

	err = bcm54xx_exp_write(phydev, MII_BCM54XX_EXP_EXP97,
				MII_BCM54XX_EXP_EXP97_MYST);

error:
	bcm54xx_auxctl_write(phydev,
			     MII_BCM54XX_AUXCTL_SHDWSEL_AUXCTL,
			     MII_BCM54XX_AUXCTL_ACTL_TX_6DB);

	return err;
}

/* Specify LED default behavior in kernel boot arguments with something like "broadcom.led_default=19,0,1,-1",
 * where the (exactly) four array values 19,0,1,-1 correspond to the desired default behavior of the LEDs
 * in broadcom_leds.h (in this case LED1 <= BC_PHY_LED_ACTIVITY, LED2 <= OFF, LED3 <= ON, LED4 <= default).
 */
static int led_default[4] = {BC_PHY_LED_DEFAULT, BC_PHY_LED_DEFAULT, BC_PHY_LED_DEFAULT, BC_PHY_LED_DEFAULT};
module_param_array(led_default, int, NULL, 0);
MODULE_PARM_DESC(led_default, "Broadcom PHY LEDs default behavior");

void bc_phy_led_set(int phyno, enum BC_PHY_LEDSEL whichLed, enum BC_PHY_LEDVAL val)
{
	u16 reg;
	struct phy_device *phy;
	if (phyno >= MAX_LED_PHYS || (phy = aPhys[phyno]) == 0) {
		return;
	}
	if (val == BC_PHY_LED_OFF || val == BC_PHY_LED_OFF_INVERTED) {
		val = BCM_LED_SRC_OFF;
	} else if (val == BC_PHY_LED_ON || val == BC_PHY_LED_ON_INVERTED) {
		val = BCM_LED_SRC_ON;
	} else if (val >= BC_PHY_LED_LINKSPD1 && val <= BC_PHY_LED_SRC_ON) {
		val &= 0xf;
	} else if (whichLed >= BC_PHY_LED1 && whichLed <= BC_PHY_LED4) {
		val = led_default[whichLed - BC_PHY_LED1];
	} else {
		val = BCM_LED_SRC_OFF;
	}
	if (val == BCM_LED_SRC_OFF &&
			(led_default[whichLed - BC_PHY_LED1] == BC_PHY_LED_OFF_INVERTED ||
			led_default[whichLed - BC_PHY_LED1] == BC_PHY_LED_ON_INVERTED)) {
		val = BCM_LED_SRC_ON;
	} else if (val == BCM_LED_SRC_ON &&
			(led_default[whichLed - BC_PHY_LED1] == BC_PHY_LED_OFF_INVERTED ||
			led_default[whichLed - BC_PHY_LED1] == BC_PHY_LED_ON_INVERTED)) {
		val = BCM_LED_SRC_OFF;
	}

	switch (whichLed) {
	case BC_PHY_LED1:
		reg = bcm54xx_shadow_read(phy, BCM54XX_SHD_LEDS12);
		reg = (reg & 0xf0) | BCM54XX_SHD_LEDS_LEDL(val);
		bcm54xx_shadow_write(phy, BCM54XX_SHD_LEDS12, reg);
		break;
	case BC_PHY_LED2:
		reg = bcm54xx_shadow_read(phy, BCM54XX_SHD_LEDS12);
		reg = (reg & 0xf) | BCM54XX_SHD_LEDS_LEDH(val);
		bcm54xx_shadow_write(phy, BCM54XX_SHD_LEDS12, reg);
		break;
	case BC_PHY_LED3:
		reg = bcm54xx_shadow_read(phy, BCM54XX_SHD_LEDS34);
		reg = (reg & 0xf0) | BCM54XX_SHD_LEDS_LEDL(val);
		bcm54xx_shadow_write(phy, BCM54XX_SHD_LEDS34, reg);
		break;
	case BC_PHY_LED4:
		reg = bcm54xx_shadow_read(phy, BCM54XX_SHD_LEDS34);
		reg = (reg & 0xf) | BCM54XX_SHD_LEDS_LEDH(val);
		bcm54xx_shadow_write(phy, BCM54XX_SHD_LEDS34, reg);
		break;
	}
}
EXPORT_SYMBOL(bc_phy_led_set);

static int bc5481_auto_negotiate_enable;
module_param(bc5481_auto_negotiate_enable, int, 0);
MODULE_PARM_DESC(bc5481_auto_negotiate_enable, "Enable Broadcom 5481 auto-negotiate behaviour");

static int bc5481_high_performance_enable;
module_param(bc5481_high_performance_enable, int, 0);
MODULE_PARM_DESC(bc5481_high_performance_enable, "Enable Broadcom 5481 high-performance behaviour");

static int bc5482_clk125_output_enable;
module_param(bc5482_clk125_output_enable, int, 0);
MODULE_PARM_DESC(bc5482_clk125_output_enable, "Enable Broadcom 5482 125Mhz output clock");

#ifdef DUMP_PHY_REGISTERS
static void dump_phy_reg(struct phy_device *phydev)
{
  unsigned long int val;
  val = (unsigned long int)phy_read(phydev, MII_PHYSID1) << 16 | phy_read(phydev, MII_PHYSID2);
  printk("Ethernet PHY registers for phy %d, ID 0x%08lx:\n"
      "==============================================\n", phydev->addr, val);
  val = phy_read(phydev, MII_BMCR);
  printk("0x00 - MII Control register:                           0x%04lx\n", val);
  val = phy_read(phydev, MII_BMSR);
  printk("0x01 - MII Status  register:                           0x%04lx\n", val);
  val = phy_read(phydev, MII_ADVERTISE);
  printk("0x04 - Auto-negotiation Advertisement register:        0x%04lx\n", val);
  val = phy_read(phydev, MII_LPA);
  printk("0x05 - Auto-negotiation Link Partner Ability register: 0x%04lx\n", val);
  val = phy_read(phydev, MII_EXPANSION);
  printk("0x06 - Auto-negotiation Expansion register:            0x%04lx\n", val);
  val = phy_read(phydev, 0x07);
  printk("0x07 - Next page Transmit register:                    0x%04lx\n", val);
  val = phy_read(phydev, 0x08);
  printk("0x08 - Link Partner Received Next Page register:       0x%04lx\n", val);
  val = phy_read(phydev, MII_CTRL1000);
  printk("0x09 - 1000Base-T Control register:                    0x%04lx\n", val);
  val = phy_read(phydev, MII_STAT1000);
  printk("0x0A - 1000Base-T Status register:                     0x%04lx\n", val);
  val = phy_read(phydev, MII_ESTATUS);
  printk("0x0F - IEEE Extended Status register:                  0x%04lx\n", val);
  val = phy_read(phydev, 0x10);
  printk("0x10 - IEEE Extended Control register:                 0x%04lx\n", val);
  val = phy_read(phydev, 0x11);
  printk("0x11 - PHY Extended Status register:                   0x%04lx\n", val);
  val = phy_read(phydev, MII_DCOUNTER);
  printk("0x12 - Receive Error Counter register:                 0x%04lx\n", val);
  val = phy_read(phydev, MII_FCSCOUNTER);
  printk("0x13 - False Carrier Sense Counter register:           0x%04lx\n", val);
  val = phy_read(phydev, MII_NWAYTEST);
  printk("0x14 - Receiver NOT_OK Counter register:               0x%04lx\n", val);
  val = bcm54xx_exp_read(phydev, 0);
  printk("0x17:0 - Receive/Transmit Packet Counter register:     0x%04lx\n", val);
  val = bcm54xx_exp_read(phydev, 1);
  printk("0x17:1 - Expansion Interrupt Status register:          0x%04lx\n", val);
  val = bcm54xx_exp_read(phydev, 4);
  printk("0x17:4 - Multicolor LED Selector register:             0x%04lx\n", val);
  val = bcm54xx_exp_read(phydev, 5);
  printk("0x17:5 - Multicolor LED Flash Rate Controls register:  0x%04lx\n", val);
  val = bcm54xx_exp_read(phydev, 6);
  printk("0x17:6 - Multicolor LED Programmable Blink Contrl reg: 0x%04lx\n", val);
  val = bcm54xx_auxctl_read(phydev, 0);
  printk("0x18:0 - Auxilary Control register:                    0x%04lx\n", val);
  val = bcm54xx_auxctl_read(phydev, 1);
  printk("0x18:1 - 10BaseT register:                             0x%04lx\n", val);
  val = bcm54xx_auxctl_read(phydev, 2);
  printk("0x18:2 - Power MII Control register:                   0x%04lx\n", val);
  val = bcm54xx_auxctl_read(phydev, 4);
  printk("0x18:4 - Misc Test register:                           0x%04lx\n", val);
  val = bcm54xx_auxctl_read(phydev, 7);
  printk("0x18:7 - Misc Control register:                        0x%04lx\n", val);
  val = phy_read(phydev, 0x19);
  printk("0x19 - Auxilary Status Summary register:               0x%04lx\n", val);
  val = phy_read(phydev, MII_RESV2);
  printk("0x1A - Interrupt Status register:                      0x%04lx\n", val);
  val = phy_read(phydev, MII_TPISTATUS);
  printk("0x1B - Interrupt Mask register:                        0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x02);
  printk("0x1C:2 - Spare Control 1 register:                     0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x03);
  printk("0x1C:3 - Clock Alignment Control register:             0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x04);
  printk("0x1C:4 - Spare Control 2 register:                     0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, BCM5482_SPARE_CTRL3);
  printk("0x1C:5 - Spare Control 3 register:                     0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x08);
  printk("0x1C:8 - LED Status register:                          0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x09);
  printk("0x1C:9 - LED Control register:                         0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x0A);
  printk("0x1C:A - Auto Power-down register:                     0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x0D);
  printk("0x1C:D - LED Selector 1 register:                      0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x0E);
  printk("0x1C:E - LED Selector 2 register:                      0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x0F);
  printk("0x1C:F - LED GPIO Control/Status register:             0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x13);
  printk("0x1C:13 - SerDES 100BASE-FX Control register:          0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x14);
  printk("0x1C:14 - Secondary SerDES Control register:           0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x15);
  printk("0x1C:15 - SGMII Slave register:                        0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x16);
  printk("0x1C:16 - Primary SerDes Control:                      0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x17);
  printk("0x1C:17 - Misc 1000BASE-X Control 2:                   0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x18);
  printk("0x1C:18 - SGMII/Media Converter register:              0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1A);
  printk("0x1C:1A - Auto-negotiation Debug register:             0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1B);
  printk("0x1C:1B - Auxilary 1000BASE-X Control register:        0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1C);
  printk("0x1C:1C - Auxilary 1000BASE-X Status register:         0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1D);
  printk("0x1C:1D - Misc 1000BASE-X Status register:             0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1E);
  printk("0x1C:1E - Copper/Fiber Auto-detect Medium register:    0x%04lx\n", val);
  val = bcm54xx_shadow_read(phydev, 0x1F);
  printk("0x1C:1F - Mode Control register:                       0x%04lx\n", val);
  val = phy_read(phydev, 0x1D);
  printk("0x1D - PHY Extended Status register:                   0x%04lx\n", val);
  val = phy_read(phydev, 0x1E);
  printk("0x1E - HCD Sumary register:                            0x%04lx\n", val);
  return;
}
#endif

static ssize_t crc_error_counter_show(struct class *c, char *buf)
{
	int i;
	int length = 0;

	for(i = 0; i < phy_instances; i++)
	{
		length += snprintf(buf+length, PAGE_SIZE, "phy_addr: %p, eth%d: %d\n", aPhys[i], i, phy_read(aPhys[i], MII_BCM54XX_RCV_NOT_OK_CNTR));
	}	
        return (length);
}

static ssize_t rcv_error_counter_show(struct class *c, char *buf)
{
	int i;
	int length = 0;

	for(i = 0; i < phy_instances; i++)
	{
		length += snprintf(buf+length, PAGE_SIZE, "phy_addr: %p, eth%d: %d\n", aPhys[i], i, phy_read(aPhys[i], MII_BCM54XX_RCV_ERR_CNTR));
	}	
        return (length);
}

static void dump_phy_regs_show(struct class *c, char *buf)
{
	int i;

	for(i = 0; i < phy_instances; i++)
	{
                dump_phy_reg(aPhys[i]);
	}	
}
static struct class_attribute broadcom_class_attrs[] = {
        __ATTR_RO(crc_error_counter),
        __ATTR_RO(rcv_error_counter),
        __ATTR_RO(dump_phy_regs),
        __ATTR_NULL,
};

static struct class broadcom_phy_class = {
	.name = DRIVER_NAME,
	.owner = THIS_MODULE,
	.class_attrs = broadcom_class_attrs,
};

static int bcm54xx_config_init(struct phy_device *phydev)
{
	int reg, err, i;
	u32 phyaddr;
	
	reg = phy_read(phydev, MII_BCM54XX_ECR);
	if (reg < 0)
		return reg;

	/* Mask interrupts globally.  */
	reg |= MII_BCM54XX_ECR_IM;
	err = phy_write(phydev, MII_BCM54XX_ECR, reg);
	if (err < 0)
		return err;

	/* Unmask events we are interested in.  */
	reg = ~(MII_BCM54XX_INT_DUPLEX |
		MII_BCM54XX_INT_SPEED |
		MII_BCM54XX_INT_LINK);
	err = phy_write(phydev, MII_BCM54XX_IMR, reg);
	if (err < 0)
		return err;

	if (phydev->drv->phy_id == PHY_ID_BCM50610) {
		err = bcm50610_a0_workaround(phydev);
		if (err < 0)
			return err;
	}

	// Enable auto-negotiation and high-performance modes
	if (bc5481_auto_negotiate_enable != 0) {
	        reg = phy_read(phydev, 0x00);
		reg = reg | BCM5481_AUTO_NEGOTIATE_ENABLE;
		phy_write(phydev, 0x00, reg);
		printk("Auto-Negotiate Enable: %d (0x%04X) => %d\n",
	                ((reg & BCM5481_AUTO_NEGOTIATE_ENABLE) != 0), reg,
	                ((phy_read(phydev, 0x00) & BCM5481_AUTO_NEGOTIATE_ENABLE) != 0));
	}

	if (bc5481_high_performance_enable != 0) {
	        reg = bcm54xx_auxctl_read(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_PMII);
                reg |= MII_BCM54XX_AUXCTL_PMII_HPE;
                bcm54xx_auxctl_write(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_PMII, reg);
		printk("High-Performance Enable: %d (0x%04X) => %d\n",
		       ((reg & MII_BCM54XX_AUXCTL_PMII_HPE) != 0), reg,
		       ((bcm54xx_auxctl_read(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_PMII) & MII_BCM54XX_AUXCTL_PMII_HPE) == 0));
	}

	if (bc5482_clk125_output_enable == 0) {
	        reg = bcm54xx_shadow_read(phydev, BCM5482_SPARE_CTRL3);
		reg = reg & ~BCM5482_CLK125_OUTPUT;
		bcm54xx_shadow_write(phydev, BCM5482_SPARE_CTRL3, reg);
		printk("Clock 125Mhz Enable: %d (0x%04X) => %d\n",
		       ((reg & ~BCM5482_CLK125_OUTPUT) != 0), reg,
		       ((bcm54xx_shadow_read(phydev, BCM5482_SPARE_CTRL3) & BCM5482_CLK125_OUTPUT) != 0));
	}

	for (phyaddr = phydev->addr, i = -1; phyaddr != 0; phyaddr >>= 1)
		++i;
	if (i >= 0 && i < MAX_LED_PHYS) {
		aPhys[i] = phydev;
		if (led_default[0] < 0) {
			led_default[0] = (bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS12) & 0xF) | 0x10;
		}
		bc_phy_led_set(i, BC_PHY_LED1, led_default[0]);
		if (led_default[1] < 0) {
			led_default[1] = ((bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS12) >> 4) & 0xF) | 0x10;
		}
		bc_phy_led_set(i, BC_PHY_LED2, led_default[1]);
		if (led_default[2] < 0) {
			led_default[2] = (bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS34) & 0xF) | 0x10;
		}
		bc_phy_led_set(i, BC_PHY_LED3, led_default[2]);
		if (led_default[3] < 0) {
			led_default[3] = ((bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS34) >> 4) & 0xF) | 0x10;
		}
		bc_phy_led_set(i, BC_PHY_LED4, led_default[3]);
	}

	// Enable full 16-bit CRC counter
	reg = phy_read(phydev, MII_BCM54XX_TEST_REG1);
	reg = reg | MII_BCM54XX_RECEIVER_NOT_OK;
	phy_write(phydev, MII_BCM54XX_TEST_REG1, reg);
	//printk("CRC error counter enable for %p: 0x%04X\n", aPhys[i], phy_read(phydev, MII_BCM54XX_TEST_REG1));

	phy_instances++;
	return 0;
}

static int bcm5482_config_init(struct phy_device *phydev)
{
	int err, reg;

	err = bcm54xx_config_init(phydev);

	if (phydev->dev_flags & PHY_BCM_FLAGS_MODE_1000BX) {
		/*
		 * Enable secondary SerDes and its use as an LED source
		 */
		reg = bcm54xx_shadow_read(phydev, BCM5482_SHD_SSD);
		bcm54xx_shadow_write(phydev, BCM5482_SHD_SSD,
				     reg |
				     BCM5482_SHD_SSD_LEDM |
				     BCM5482_SHD_SSD_EN);

		/*
		 * Enable SGMII slave mode and auto-detection
		 */
		reg = BCM5482_SSD_SGMII_SLAVE | MII_BCM54XX_EXP_SEL_SSD;
		err = bcm54xx_exp_read(phydev, reg);
		if (err < 0)
			return err;
		err = bcm54xx_exp_write(phydev, reg, err |
					BCM5482_SSD_SGMII_SLAVE_EN |
					BCM5482_SSD_SGMII_SLAVE_AD);
		if (err < 0)
			return err;

		/*
		 * Disable secondary SerDes powerdown
		 */
		reg = BCM5482_SSD_1000BX_CTL | MII_BCM54XX_EXP_SEL_SSD;
		err = bcm54xx_exp_read(phydev, reg);
		if (err < 0)
			return err;
		err = bcm54xx_exp_write(phydev, reg,
					err & ~BCM5482_SSD_1000BX_CTL_PWRDOWN);
		if (err < 0)
			return err;

		/*
		 * Select 1000BASE-X register set (primary SerDes)
		 */
		reg = bcm54xx_shadow_read(phydev, BCM5482_SHD_MODE);
		bcm54xx_shadow_write(phydev, BCM5482_SHD_MODE,
				     reg | BCM5482_SHD_MODE_1000BX);

		/*
		 * LED1=ACTIVITYLED, LED3=LINKSPD[2]
		 * (Use LED1 as secondary SerDes ACTIVITY LED)
		 */
		bcm54xx_shadow_write(phydev, BCM5482_SHD_LEDS1,
			BCM5482_SHD_LEDS1_LED1(BCM_LED_SRC_ACTIVITYLED) |
			BCM5482_SHD_LEDS1_LED3(BCM_LED_SRC_LINKSPD2));

		/*
		 * Auto-negotiation doesn't seem to work quite right
		 * in this mode, so we disable it and force it to the
		 * right speed/duplex setting.  Only 'link status'
		 * is important.
		 */
		phydev->autoneg = AUTONEG_DISABLE;
		phydev->speed = SPEED_1000;
		phydev->duplex = DUPLEX_FULL;
	}

	return err;
}

static int bcm5482_read_status(struct phy_device *phydev)
{
	int err;
        enum BC_PHY_LEDVAL val;
	u16 reg;

	err = genphy_read_status(phydev);
	
        /* Update multicolor LEDs based on link status, the inverse logic
           of the LINKACTIVITY1 and LINKACTIVITY2 signals on the BCM5482
           cause this to be used. */

        if (phydev->link) 
        {
                val = BCM_LED_SRC_OFF;
                reg = bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS12);
                reg = (reg & 0xf) | BCM54XX_SHD_LEDS_LEDH(val);
                bcm54xx_shadow_write(phydev, BCM54XX_SHD_LEDS12, reg);
	        val = BCM_LED_SRC_OFF;
        	reg = bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS34);
        	reg = (reg & 0xf0) | BCM54XX_SHD_LEDS_LEDL(val);
        	bcm54xx_shadow_write(phydev, BCM54XX_SHD_LEDS34, reg);
	} 
        else 
        {
                val = BCM_LED_SRC_ON;
                reg = bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS12);
                reg = (reg & 0xf) | BCM54XX_SHD_LEDS_LEDH(val);
	        bcm54xx_shadow_write(phydev, BCM54XX_SHD_LEDS12, reg);
	        val = BCM_LED_SRC_ON;
                reg = bcm54xx_shadow_read(phydev, BCM54XX_SHD_LEDS34);
                reg = (reg & 0xf0) | BCM54XX_SHD_LEDS_LEDL(val);
		bcm54xx_shadow_write(phydev, BCM54XX_SHD_LEDS34, reg);
        }

	if (phydev->dev_flags & PHY_BCM_FLAGS_MODE_1000BX) {
		/*
		 * Only link status matters for 1000Base-X mode, so force
		 * 1000 Mbit/s full-duplex status
		 */
		if (phydev->link) {
			phydev->speed = SPEED_1000;
			phydev->duplex = DUPLEX_FULL;
	        }
        }

	return err;
}

static int bcm54xx_ack_interrupt(struct phy_device *phydev)
{
	int reg;

	/* Clear pending interrupts.  */
	reg = phy_read(phydev, MII_BCM54XX_ISR);
	if (reg < 0)
		return reg;

	return 0;
}

static int bcm54xx_config_intr(struct phy_device *phydev)
{
	int reg, err;

	reg = phy_read(phydev, MII_BCM54XX_ECR);
	if (reg < 0)
		return reg;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		reg &= ~MII_BCM54XX_ECR_IM;
	else
		reg |= MII_BCM54XX_ECR_IM;

	err = phy_write(phydev, MII_BCM54XX_ECR, reg);
	return err;
}

static int bcm5481_config_aneg(struct phy_device *phydev)
{
	int ret;
	u16 clkalign;

	/* Do generic Aneg firsly. */
	ret = genphy_config_aneg(phydev);

	/* Then we can set up the delay. */
	#ifdef CONFIG_BCM8451_TX_CLOCKALIGN
	/* RGMII Transmit Clock Delay: The RGMII transmit timing can be adjusted
			 * by software control. TXD-to-GTXCLK clock delay time can be increased
			 * by approximately 1.9 ns for 1000BASE-T mode, and between 2 ns to 6 ns
			 * when in 10BASE-T or 100BASE-T mode by setting Register 1ch, SV 00011,
			 * bit 9 = 1. Enabling this timing adjustment eliminates the need for
			 * board trace delays as required by the RGMII specification.
	    	 */
	clkalign = bcm54xx_shadow_read(phydev, BCM5481_SHD_CLKALIGN);
	if ((clkalign & BCM5481_SHD_DELAY_ENA) == 0) {
		/* Turn on the BCM5481 RGMII Transmit Clock Delay */
		bcm54xx_shadow_write(phydev, BCM5481_SHD_CLKALIGN,
				BCM5481_SHD_DELAY_ENA);
		printk("Set BCM5481 shadow delay for phy %d from 0x%x to 0x%x\n",
				phydev->addr, clkalign,
				bcm54xx_shadow_read(phydev, BCM5481_SHD_CLKALIGN));
	}
	#endif
	#ifdef CONFIG_BCM8451_RX_CLOCKSKEW
	/*
	 * Skew time can be increased by approximately 1.9ns for
	 * 1000BASE-T mode, 4 ns for 100BASE-T mode, and 50 ns for
	 * 10BASE-T mode by setting Register 18h, SV 111, bit 8 = 1.
	 * Enabling this timing adjustment eliminates the need for board
	 * trace delays, as required by the RGMII specification.  This
	 * at least helps BCM5481 to successfuly receive packets
	 * on MPC8360E-RDK board. Peter Barada <peterb@logicpd.com>
	 * says: "This sets delay between the RXD and RXC signals
	 * instead of using trace lengths to achieve timing".
	 * Reimplemented to use the (slightly) clearer aux control
	 * shadow register read-write functions.
	 */
	/* Set RDX-to-RXC clk delay. */
	clkalign = bcm54xx_auxctl_read(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_MISC);
	if ((clkalign & BCM5481_AUX_SKEW_ENA) == 0) {
		/* Set RDX-RXC skew. */
		bcm54xx_auxctl_write(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_MISC,
				clkalign | BCM5481_AUX_SKEW_ENA);
		printk("Set BCM5481 shadow RXD-to-RXC clock skew for phy %d from 0x%x to 0x%x\n",
				phydev->addr, clkalign,
				bcm54xx_auxctl_read(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_MISC));
	}
	#endif
	return ret;
}


/* MII_CTRL1000 register bits */
#define BCM54610_NORMAL_MASK     (0x1FFF)
#define BCM54610_TEST_TX_DIST    (0x8000)
#define BCM54610_TEST_SLV_JITTER (0x6000)
#define BCM54610_TEST_MST_JITTER (0x4000)
#define BCM54610_TEST_TX_WAVE    (0x2000)
#define BCM54610_MST_SLV_AUTO    (0x0000)
#define BCM54610_MST_SLV_MANUAL  (0x1000)
#define BCM54610_MANUAL_MASTER   (0x0800)
#define BCM54610_MANUAL_SLAVE    (0x0000)

/* Loopback register bits */
#define BCM54610_LBR_EXT_LOOPBACK    (0x8000)
#define BCM54610_LBR_TX_TEST_MODE    (0x0000)
#define BCM54610_LBR_TX_NORMAL_MODE  (0x0400)

static void bcm54610_set_test_mode(struct phy_device *phydev, u32 mode) {
  /* Configure the PHY for the selected test mode */
  switch(mode) {
  case PHY_TEST_EXT_LOOP:
    phy_write(phydev, MII_CTRL1000, 
	      (BCM54610_MST_SLV_MANUAL | BCM54610_MANUAL_MASTER));
    phy_write(phydev, MII_BMCR, (BMCR_FULLDPLX | BMCR_SPEED1000));
    phy_write(phydev, MII_LBRERROR, 
	      (BCM54610_LBR_EXT_LOOPBACK | BCM54610_LBR_TX_NORMAL_MODE));
    phydev->autoneg = AUTONEG_DISABLE;
    printk("BCM54610 external loopback configured; insert loopback jumper\n");
    break;

  case PHY_TEST_INT_LOOP:
    phy_write(phydev, MII_BMCR, 
              (BMCR_LOOPBACK | BMCR_FULLDPLX | BMCR_SPEED1000));
    phydev->autoneg = AUTONEG_DISABLE;
    printk("BCM54610 internal loopback configured\n");
    break;

  case PHY_TEST_TX_WAVEFORM:
    printk("IEEE 802.3ba Transmit Waveform Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_TX_WAVE);
    break;

  case PHY_TEST_MASTER_JITTER:
    printk("IEEE 802.3ba Master Jitter Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_MST_JITTER);
    break;

  case PHY_TEST_SLAVE_JITTER:
    printk("IEEE 802.3ba Slave Jitter Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_SLV_JITTER);
    break;

  case PHY_TEST_TX_DISTORTION:
    printk("IEEE 802.3ba Transmit Distortion Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_TX_DIST);
    break;

  default:
    /* No test mode, normal operation */
    phy_write(phydev, MII_LBRERROR, BCM54610_LBR_TX_NORMAL_MODE);
    phy_write(phydev, MII_BMCR, (BMCR_ANENABLE | BMCR_FULLDPLX | BMCR_SPEED1000));
    phy_write(phydev, MII_CTRL1000, 
              (ADVERTISE_1000FULL | BCM54610_MST_SLV_AUTO));
    phydev->autoneg = AUTONEG_ENABLE;
    printk("BCM54610 set for normal operation\n");
  }
}

static struct phy_driver bcm5411_driver = {
	.phy_id		= 0x00206070,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5411",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm5421_driver = {
	.phy_id		= 0x002060e0,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5421",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm5461_driver = {
	.phy_id		= 0x002060c0,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5461",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm5464_driver = {
	.phy_id		= 0x002060b0,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5464",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static void bcm5481_set_test_mode(struct phy_device *phydev, u32 mode) {
  /* Configure the PHY for the selected test mode */
  /* Note that the bcm5481 works just like the bcm54610 in test mode setup,
   * so we just reuse the bcm54610 defines */
  switch(mode) {
  case PHY_TEST_EXT_LOOP:
    phy_write(phydev, MII_CTRL1000,
	      (BCM54610_MST_SLV_MANUAL | BCM54610_MANUAL_MASTER));
    phy_write(phydev, MII_BMCR, (BMCR_FULLDPLX | BMCR_SPEED1000));
    phy_write(phydev, MII_LBRERROR,
	      (BCM54610_LBR_EXT_LOOPBACK | BCM54610_LBR_TX_NORMAL_MODE));
    phydev->autoneg = AUTONEG_DISABLE;
    printk("BCM5481 external loopback configured; insert loopback jumper\n");
    break;

  case PHY_TEST_INT_LOOP:
    phy_write(phydev, MII_BMCR,
              (BMCR_LOOPBACK | BMCR_FULLDPLX | BMCR_SPEED1000));
    phydev->autoneg = AUTONEG_DISABLE;
    printk("BCM5481 internal loopback configured\n");
    break;

  case PHY_TEST_TX_WAVEFORM:
    printk("IEEE 802.3ba Transmit Waveform Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_TX_WAVE);
    break;

  case PHY_TEST_MASTER_JITTER:
    printk("IEEE 802.3ba Master Jitter Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_MST_JITTER);
    break;

  case PHY_TEST_SLAVE_JITTER:
    printk("IEEE 802.3ba Slave Jitter Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_SLV_JITTER);
    break;

  case PHY_TEST_TX_DISTORTION:
    printk("IEEE 802.3ba Transmit Distortion Test mode\n");
    phy_write(phydev, MII_CTRL1000, BCM54610_TEST_TX_DIST);
    break;

  default:
    /* No test mode, normal operation */
    phy_write(phydev, MII_LBRERROR, BCM54610_LBR_TX_NORMAL_MODE);
    phy_write(phydev, MII_BMCR, (BMCR_ANENABLE | BMCR_FULLDPLX | BMCR_SPEED1000));
    phy_write(phydev, MII_CTRL1000,
              (ADVERTISE_1000FULL | BCM54610_MST_SLV_AUTO));
    phydev->autoneg = AUTONEG_ENABLE;
    printk("BCM5481 set for normal operation\n");
  }
}

static struct phy_driver bcm5481_driver = {
	.phy_id		= 0x0143bca0,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5481",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= bcm5481_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.set_test_mode  = bcm5481_set_test_mode,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm5482_driver = {
	.phy_id		= 0x0143bcb0,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM5482",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm5482_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= bcm5482_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm50610_driver = {
	.phy_id		= PHY_ID_BCM50610,
	.phy_id_mask	= 0xffffffff,
	.name		= "Broadcom BCM50610",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm54610_driver = {
	.phy_id		= PHY_ID_BCM54610,
	.phy_id_mask	= 0xffffffff,
	.name		= "Broadcom BCM54610",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.set_test_mode  = bcm54610_set_test_mode,
	.driver 	= { .owner = THIS_MODULE },
};

static struct phy_driver bcm57780_driver = {
	.phy_id		= 0x03625d90,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom BCM57780",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= bcm54xx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= bcm54xx_ack_interrupt,
	.config_intr	= bcm54xx_config_intr,
	.driver 	= { .owner = THIS_MODULE },
};

static int __init broadcom_init(void)
{
	int ret;

	ret = phy_driver_register(&bcm5411_driver);
	if (ret)
		goto out_5411;
	ret = phy_driver_register(&bcm5421_driver);
	if (ret)
		goto out_5421;
	ret = phy_driver_register(&bcm5461_driver);
	if (ret)
		goto out_5461;
	ret = phy_driver_register(&bcm5464_driver);
	if (ret)
		goto out_5464;
	ret = phy_driver_register(&bcm5481_driver);
	if (ret)
		goto out_5481;
	ret = phy_driver_register(&bcm5482_driver);
	if (ret)
		goto out_5482;
	ret = phy_driver_register(&bcm50610_driver);
	if (ret)
		goto out_50610;
	ret = phy_driver_register(&bcm54610_driver);
	if (ret)
		goto out_54610;
	ret = phy_driver_register(&bcm57780_driver);
	if (ret)
		goto out_57780;
	class_register(&broadcom_phy_class);
	return (ret);

out_57780:
	phy_driver_unregister(&bcm54610_driver);
out_54610:
	phy_driver_unregister(&bcm50610_driver);
out_50610:
	phy_driver_unregister(&bcm5482_driver);
out_5482:
	phy_driver_unregister(&bcm5481_driver);
out_5481:
	phy_driver_unregister(&bcm5464_driver);
out_5464:
	phy_driver_unregister(&bcm5461_driver);
out_5461:
	phy_driver_unregister(&bcm5421_driver);
out_5421:
	phy_driver_unregister(&bcm5411_driver);
out_5411:
	class_register(&broadcom_phy_class);
	return (ret);
}

static void __exit broadcom_exit(void)
{
	phy_driver_unregister(&bcm57780_driver);
	phy_driver_unregister(&bcm54610_driver);
	phy_driver_unregister(&bcm50610_driver);
	phy_driver_unregister(&bcm5482_driver);
	phy_driver_unregister(&bcm5481_driver);
	phy_driver_unregister(&bcm5464_driver);
	phy_driver_unregister(&bcm5461_driver);
	phy_driver_unregister(&bcm5421_driver);
	phy_driver_unregister(&bcm5411_driver);
	class_unregister(&broadcom_phy_class);
}

module_init(broadcom_init);
module_exit(broadcom_exit);
