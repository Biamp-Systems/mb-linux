/*
 * xilinx_spi.h
 *
 * Xilinx SPI controller driver extended functionality
 *
 * Author: Lab X Technologies, LLC
 *  scott.wagner@labxtechnologies.com
 *
 * 2011 (c) Lab X Technologies, LLC.  This file is licensed under the
 * terms of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

#ifndef __LINUX_SPI_XILINX_SPI_H
#define __LINUX_SPI_XILINX_SPI_H
#include <linux/spi/spi.h>
//#define SPI_SELECT_PULSEWIDTH       3       /* mS slave select pulse width */

/**
 * struct xspi_platform_data - Platform data of the Xilinx SPI driver
 * @num_chipselect:	Number of chip select by the IP.
 * @little_endian:	If registers should be accessed little endian or not.
 * @bits_per_word:	Number of bits per word.
 * @devices:		Devices to add when the driver is probed.
 * @num_devices:	Number of devices in the devices array.
 */
struct xspi_platform_data {
	u16 num_chipselect;
	bool little_endian;
	u8 bits_per_word;
	struct spi_board_info *devices;
	u8 num_devices;
};

/* Strobe slave select: Generate an active pulse on the device's slave select
 * line for SPI_SELECT_PULSEWIDTH mS, or if SPI_SELECT_PULSEWIDTH is undefined,
 * for about 100 nS.
 */
void spi_strobe_ssel(struct spi_device *spi);
/* Reset transmit buffer: In slave mode only, reset the transmit buffer pointer
 * to the beginning of the transmit buffer.  This will be done automatically by
 * the assertion of SLAVE_SELECT, or may be done explicitly if SLAVE_SELECT is
 * not used.
 */
void spi_reset_transmit_buffer(struct spi_device *spi);

#endif // not defined __LINUX_SPI_XILINX_SPI_H

