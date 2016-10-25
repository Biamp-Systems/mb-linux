
/******************************************************************
 *
 *  sport_switch_user.h
 *
 *  Created on: Jan 26, 2011
 *      Author: mjanew
 *    Revision: $Rev: 40514 $
 *
 *******************************************************************/

#ifndef _SPORT_SWITCH_USER_H_
#define _SPORT_SWITCH_USER_H_   "$Rev: 40514 $"

enum sport_switch_card_clock_mode
{
    SS_CARD_CLOCK_MASTER,
    SS_CARD_CLOCK_SLAVE,
};

enum sport_switch_port_mode
{
    SS_PORT_MODE_2CHNL = 0x00,
    SS_PORT_MODE_4CHNL = 0x01,
    SS_PORT_MODE_8CHNL = 0x02,
    SS_PORT_MODE_16CHNL = 0x03,
    SS_PORT_MODE_32CHNL = 0x04,
    SS_PORT_MODE_OFF = 0x07,
};

enum sport_switch_clock_src
{
    SS_MAINBOARD = 0x00,
    SS_SLOT_1 = 0x01,
    SS_SLOT_2 = 0x02,
    SS_SLOT_3 = 0x03,
    SS_SLOT_4 = 0x04,
    SS_SLOT_5 = 0x05,
    SS_SLOT_6 = 0x06,
    SS_SLOT_7 = 0x07,
    SS_SLOT_8 = 0x08,
    SS_SLOT_9 = 0x09,
    SS_SLOT_10 = 0x0A,
    SS_SLOT_11 = 0x0B,
    SS_SLOT_12 = 0x0C,
    SS_SLOT_13   = 0x0D,
    SS_AVB		 = 0x0E
};

/* the terms "input" and "output" are from the point of view of the sport
 * switch.  Adding an input card will require input ports on the switch. */
struct sport_switch_add_card_parameters
{
    enum sport_switch_card_clock_mode clock_direction;
    int card;
    int input_ports;
    int output_ports;
    enum sport_switch_port_mode channels_per_port;
};

/* ports and channels are zero-based values */

/* card is 1-based value */
struct sport_switch_add_route_parameters
{
    int src_card;
    int src_port;
    int src_channel;
    int dest_card;
    int dest_port;
    int dest_channel;
};

/* port and channel are zero-based values */

/* card is 1-based value */
struct sport_switch_remove_route_parameters
{
    int dest_card;
    int dest_port;
    int dest_channel;
};

/* first param is written by application, read by kernel,
   last param is written by kernel, read by application */
struct sport_switch_read_route_parameters
{
    unsigned int offset;
    unsigned int value;
};

/* first param is written by application, read by kernel,
   last param is written by kernel, read by application */
struct sport_switch_read_register_parameters
{
    unsigned int offset;
    unsigned int value;
};

/* tdmPort is written by application, read by kernel,
   last param is written by kernel, read by application */
struct sport_switch_read_enable_parameters
{
    unsigned int tdmPort;
    unsigned int value;
};

#define SS_IOC_MAGIC        'x'
#define SS_IOC_ADDCARD      _IOW(SS_IOC_MAGIC, 1, struct sport_switch_add_card_parameters)
#define SS_IOC_ADDROUTE     _IOW(SS_IOC_MAGIC, 2, struct sport_switch_add_route_parameters)
#define SS_IOC_REMOVEROUTE  _IOW(SS_IOC_MAGIC, 3, struct sport_switch_remove_route_parameters)
#define SS_IOC_CLEARROUTES  _IO(SS_IOC_MAGIC, 4)
#define SS_IOC_RESET        _IO(SS_IOC_MAGIC, 5)
#define SS_IOC_START        _IO(SS_IOC_MAGIC, 6)
#define SS_IOC_SETCLOCKSRC  _IOW(SS_IOC_MAGIC, 7, enum sport_switch_clock_src)
#define SS_IOC_READROUTE    _IOWR(SS_IOC_MAGIC, 8, struct sport_switch_read_route_parameters)
#define SS_IOC_READREGISTER _IOWR(SS_IOC_MAGIC, 9, struct sport_switch_read_register_parameters)
#define SS_IOC_ENABLE_FCLK  _IO(SS_IOC_MAGIC, 10)
#define SS_IOC_READENABLE   _IOWR(SS_IOC_MAGIC, 11, struct sport_switch_read_enable_parameters)
#define SS_IOC_MAX          _IOC_NR(12)

//
//  diagnostic ioctl
//

#define SS_IOC_DIAG_SNOOPY_TYPE		'p'
#define SS_IOC_DIAG_SNOOPY_ON		_IO(SS_IOC_DIAG_SNOOPY_TYPE, 1)
#define SS_IOC_DIAG_SNOOPY_OFF		_IO(SS_IOC_DIAG_SNOOPY_TYPE, 2)
#define SS_IOC_DIAG_SNAP			_IOR(SS_IOC_DIAG_SNOOPY_TYPE, 3, char *)

#endif
