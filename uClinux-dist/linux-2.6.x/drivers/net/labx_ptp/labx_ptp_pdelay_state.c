/*
 *  linux/drivers/net/labx_ptp_pdelay_state.c
 *
 *  Lab X Technologies Precision Time Protocol (PTP) driver
 *  PTP peer delay state machine processing
 *
 *  Written by Chris Wulff (chris.wulff@labxtechnologies.com)
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

#include "labx_ptp.h"

/* Define this to get some extra debug on path delay messages */
/* #define PATH_DELAY_DEBUG */

static void computePdelayRateRatio(struct ptp_device *ptp, uint32_t port)
{
  if (ptp->ports[port].initPdelayRespReceived == FALSE)
  {
    /* Capture the initial PDELAY response */
    ptp->ports[port].initPdelayRespReceived = TRUE;
    ptp->ports[port].neighborRateRatioValid = ptp->ports[port].isMeasuringDelay;
    ptp->ports[port].pdelayRespTxTimestampI = ptp->ports[port].pdelayRespTxTimestamp;
    ptp->ports[port].pdelayRespRxTimestampI = ptp->ports[port].pdelayRespRxTimestamp;
  }
  else
  {
    PtpTime difference;
    PtpTime difference2;
    uint64_t nsResponder;
    uint64_t nsRequester;
    uint64_t rateRatio;
    int shift;

    timestamp_difference(&ptp->ports[port].pdelayRespTxTimestamp, &ptp->ports[port].pdelayRespTxTimestampI, &difference);
    timestamp_difference(&ptp->ports[port].pdelayRespRxTimestamp, &ptp->ports[port].pdelayRespRxTimestampI, &difference2);

    /* Keep rolling the interval forward or we will react really slowly to changes in our
     * rate relative to our neighbor.
     */
    ptp->ports[port].pdelayRespTxTimestampI = ptp->ports[port].pdelayRespTxTimestamp;
    ptp->ports[port].pdelayRespRxTimestampI = ptp->ports[port].pdelayRespRxTimestamp;

    /* The raw differences have been computed; sanity-check the peer delay timestamps; if the 
     * initial Tx or Rx timestamp is later than the present one, the initial ones are bogus and
     * must be replaced.
     */
    if((difference.secondsUpper & 0x80000000) |
       (difference2.secondsUpper & 0x80000000)) {
      ptp->ports[port].initPdelayRespReceived = FALSE;
      ptp->ports[port].neighborRateRatioValid = FALSE;
    } else {
      nsResponder = ((uint64_t)difference.secondsLower) * 1000000000ULL + (uint64_t)difference.nanoseconds;
      nsRequester = ((uint64_t)difference2.secondsLower) * 1000000000ULL + (uint64_t)difference2.nanoseconds;

      for (shift = 0; shift < 31; shift++)
        {
          if (nsResponder & (1ULL<<(63-shift))) break;
        }

      if ((nsRequester >> (31-shift)) != 0) {
        rateRatio = (nsResponder << shift) / (nsRequester >> (31-shift));
        ptp->ports[port].neighborRateRatio = (uint32_t)rateRatio;

        ptp->ports[port].neighborRateRatioValid = TRUE;
      }

#ifdef PATH_DELAY_DEBUG
      printk("Responder delta: %08X%08X.%08X (%llu ns)\n", difference.secondsUpper,
             difference.secondsLower, difference.nanoseconds, nsResponder);
      printk("Requester delta: %08X%08X.%08X (%llu ns)\n", difference2.secondsUpper,
             difference2.secondsLower, difference2.nanoseconds, nsRequester);
      printk("Rate ratio: %08X (shift %d)\n", ptp->ports[port].neighborRateRatio, shift);
#endif
    } /* if(differences are sane) */
  }
}

static void computePropTime(struct ptp_device *ptp, uint32_t port)
{
  if (ptp->ports[port].neighborRateRatioValid)
  {
    PtpTime difference;
    PtpTime difference2;
    uint64_t nsResponder;
    uint64_t nsRequester;

    timestamp_difference(&ptp->ports[port].pdelayRespTxTimestamp, &ptp->ports[port].pdelayReqRxTimestamp, &difference);
    timestamp_difference(&ptp->ports[port].pdelayRespRxTimestamp, &ptp->ports[port].pdelayReqTxTimestamp, &difference2);

    nsResponder = ((uint64_t)difference.secondsLower) * 1000000000ULL + (uint64_t)difference.nanoseconds;
    nsRequester = ((uint64_t)difference2.secondsLower) * 1000000000ULL + (uint64_t)difference2.nanoseconds;

    if(nsResponder < ((((uint64_t)ptp->ports[port].neighborRateRatio) * nsRequester) >> 31) + 1)
    ptp->ports[port].neighborPropDelay = (((((uint64_t)ptp->ports[port].neighborRateRatio) * nsRequester) >> 31) - nsResponder) >> 1;
    else
    {
      ptp->ports[port].neighborPropDelay = ((((((uint64_t)ptp->ports[port].neighborRateRatio) * nsRequester) >> 31) - nsResponder) >> 1) + 50;
      #ifdef PATH_DELAY_DEBUG
      printk("Negative Propagation delay detected, Using Hack to add 50ns\n");
      #endif
    }


#ifdef PATH_DELAY_DEBUG
    if(port == 0)
    {
    printk("Responder delta: %08X%08X.%08X (%llu ns)\n", difference.secondsUpper,
      difference.secondsLower, difference.nanoseconds, nsResponder);
    printk("Requester delta: %08X%08X.%08X (%llu ns)\n", difference2.secondsUpper,
      difference2.secondsLower, difference2.nanoseconds, nsRequester);
    printk("Prop Delay: %08X\n", ptp->ports[port].neighborPropDelay);
      printk("Prop Delay Threshold: %08X\n", ptp->ports[port].neighborPropDelayThresh);
    }
#endif
  }
}

/* 802.1AS MDPdelayReq state machine (11.2.15.3) entry actions */
static void MDPdelayReq_StateMachine_SetState(struct ptp_device *ptp, uint32_t port, MDPdelayReq_State_t newState)
{
#ifdef PATH_DELAY_DEBUG
   printk("MDPdelayReq: Set State %d (port index %d)\n", newState, port);
#endif

  ptp->ports[port].mdPdelayReq_State = newState;

  switch (newState)
  {
    default:
    case MDPdelayReq_NOT_ENABLED:
      ptp->ports[port].asCapable = FALSE;

      /* Track consecutive multiple pdelay responses for AVnu_PTP-5 PICS */
      ptp->ports[port].pdelayResponses = 0;
      ptp->ports[port].multiplePdelayResponses = 0;
      ptp->ports[port].prevPdelayReqSequenceId = 0;
      break;

    case MDPdelayReq_INITIAL_SEND_PDELAY_REQ:
      ptp->ports[port].initPdelayRespReceived = FALSE;
      ptp->ports[port].neighborRateRatio = 0x80000000; // 1.0 fixed point 1.31
      ptp->ports[port].rcvdMDTimestampReceive = FALSE;
      ptp->ports[port].pdelayReqSequenceId = 0x0000; // TODO: spec says random()
      ptp->ports[port].rcvdPdelayResp = FALSE;
      ptp->ports[port].rcvdPdelayRespFollowUp = FALSE;
      ptp->ports[port].lostResponses = 0;
      ptp->ports[port].isMeasuringDelay = FALSE;
      ptp->ports[port].asCapable = FALSE;
      ptp->ports[port].neighborRateRatioValid = FALSE;

      /* Track consecutive multiple pdelay responses for AVnu_PTP-5 PICS */
      ptp->ports[port].pdelayResponses = 0;
      ptp->ports[port].multiplePdelayResponses = 0;
      ptp->ports[port].prevPdelayReqSequenceId = 0;

      ptp->ports[port].pdelayIntervalTimer = 0; // currentTime ("now" is zero ticks)
      transmit_pdelay_request(ptp, port);
      break;

    case MDPdelayReq_LOST_RESPONSE:
        ptp->ports[port].lostResponses++;
#ifdef DEBUG_OUTPUT
        if(port == 0)
          printk("lostResponses %d\n", ptp->ports[port].lostResponses);
#endif
      if (ptp->ports[port].lostResponses > ptp->ports[port].allowedLostResponses)
      {
        ptp->ports[port].asCapable = FALSE;
#ifdef DEBUG_OUTPUT
        if(port == 0)
            printk("asCapable %d\n", ptp->ports[port].asCapable);
#endif
      }
      break;

    case MDPdelayReq_RESET:
      ptp->ports[port].initPdelayRespReceived = FALSE;
      ptp->ports[port].rcvdPdelayResp = FALSE;
      ptp->ports[port].rcvdPdelayRespFollowUp = FALSE;
      ptp->ports[port].isMeasuringDelay = FALSE;
      break;

    case MDPdelayReq_SEND_PDELAY_REQ:
      ptp->ports[port].pdelayReqSequenceId++;
          ptp->ports[port].pdelayResponses = 0;
      ptp->ports[port].pdelayIntervalTimer = 0; // currentTime ("now" is zero ticks)
      transmit_pdelay_request(ptp, port);
      break;

    case MDPdelayReq_WAITING_FOR_PDELAY_RESP:
      ptp->ports[port].rcvdMDTimestampReceive = FALSE;
      break;

    case MDPdelayReq_WAITING_FOR_PDELAY_RESP_FOLLOW_UP:
      get_source_port_id(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespPtr, ptp->ports[port].lastRxRespSourcePortId);
      ptp->ports[port].rcvdPdelayResp = FALSE;

      /* Obtain the peer delay request receive timestamp that our peer has just sent.
       * (Trsp2 - responder local clock) */
      get_timestamp(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespPtr,
        &ptp->ports[port].pdelayReqRxTimestamp);

      /* Capture the hardware timestamp at which we received this packet, and hang on to
       * it for delay and rate calculation. (Trsp4 - our local clock) */
      get_local_hardware_timestamp(ptp, port, RECEIVED_PACKET,
        ptp->ports[port].rcvdPdelayRespPtr, &ptp->ports[port].pdelayRespRxTimestamp);
      break;

    case MDPdelayReq_WAITING_FOR_PDELAY_INTERVAL_TIMER:
      ptp->ports[port].rcvdPdelayRespFollowUp = FALSE;

      /* Obtain the follow up timestamp for delay and rate calculation.
       * (Trsp3 - responder local clock) */
      get_timestamp(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespFollowUpPtr,
        &ptp->ports[port].pdelayRespTxTimestamp);

      if (ptp->ports[port].computeNeighborRateRatio)
      {
        computePdelayRateRatio(ptp, port);
      }
      if (ptp->ports[port].computeNeighborPropDelay)
      {
        computePropTime(ptp, port);
      }

      if (ptp->ports[port].isMeasuringDelay != FALSE)
      {
#ifdef PATH_DELAY_DEBUG
      {
        int i;
        printk("AS CHECK: pd %d, pdt %d, pidc %d, nrrv %d\n", ptp->ports[port].neighborPropDelay,
            ptp->ports[port].neighborPropDelayThresh, compare_clock_identity(ptp->ports[port].lastRxRespSourcePortId, ptp->properties.grandmasterIdentity),
          ptp->ports[port].neighborRateRatioValid);

        printk("AS CHECK: rxSourcePortID:");
          for (i=0; i<PORT_ID_BYTES; i++) printk("%02X", ptp->ports[port].lastRxRespSourcePortId[i]);
        printk("\n");
        printk("AS CHECK: thisClock:     ");
        /* for (i=0; i<PTP_CLOCK_IDENTITY_CHARS; i++) printk("%02X", ptp->properties.grandmasterIdentity[i]); */
        printk("\n");
      }
#endif

      /* AS capable if the delay is low enough, the pdelay response is not from us, and we have a valid ratio */
      if ((ptp->ports[port].neighborPropDelay <= ptp->ports[port].neighborPropDelayThresh) &&
            (compare_clock_identity(ptp->ports[port].lastRxRespSourcePortId, ptp->properties.grandmasterIdentity) != 0) &&
          ptp->ports[port].neighborRateRatioValid)
      {
        ptp->ports[port].asCapable = TRUE;
          #ifdef DEBUG_OUTPUT
            if (port == 0)
              printk("asCapable %d\n", ptp->ports[port].asCapable);
          #endif
      }
      else
      {
        if (ptp->ports[port].asCapable && (ptp->ports[port].neighborPropDelay > ptp->ports[port].neighborPropDelayThresh)) {
          printk("Disabling port %d. Delay over threshold (%d ns > %d ns)\n", port + 1,
            ptp->ports[port].neighborPropDelay, ptp->ports[port].neighborPropDelayThresh);
        }
        ptp->ports[port].asCapable = FALSE;
          #ifdef DEBUG_OUTPUT
            if (port == 0)
              printk("asCapable %d\n", ptp->ports[port].asCapable);
          #endif
        }
      }
      ptp->ports[port].lostResponses = 0;
      #ifdef DEBUG_OUTPUT
        if (port == 0)
          printk("lostResponses %d\n", ptp->ports[port].lostResponses);
      #endif
      ptp->ports[port].isMeasuringDelay = TRUE;
      break;
  } 
}

/* 802.1AS MDPdelayReq state machine (11.2.15.3) transitions */
void MDPdelayReq_StateMachine(struct ptp_device *ptp, uint32_t port)
{
  if(ptp->properties.delayMechanism != PTP_DELAY_MECHANISM_P2P) {
    /* The PDELAY state machine should only be active in P2P mode */
    return;
  }

//  printk("PTP IDX %d, PE %d, PTTE %d, PDIT %d, PDRI %d\n", port, ptp->ports[port].portEnabled,
//    ptp->ports[port].pttPortEnabled, ptp->ports[port].pdelayIntervalTimer, PDELAY_REQ_INTERVAL_TICKS(ptp, port));

  if (!ptp->ports[port].portEnabled || !ptp->ports[port].pttPortEnabled)
  {
    if (ptp->ports[port].mdPdelayReq_State != MDPdelayReq_NOT_ENABLED)
    {
      /* Disabling the port immediately forces the state machine into the disabled state */
      MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_NOT_ENABLED);
    }
  }
  else
  {
    MDPdelayReq_State_t prevState;
    do
    {
    uint8_t rxRequestingPortId[PORT_ID_BYTES];
    uint8_t txRequestingPortId[PORT_ID_BYTES];
    uint32_t rxSequenceId = 0;
    uint32_t txSequenceId = 0;
    uint8_t rxFUPRequestingPortId[PORT_ID_BYTES];
    uint8_t txFUPRequestingPortId[PORT_ID_BYTES];
    uint32_t rxFUPSequenceId = 0;
    uint32_t txFUPSequenceId = 0;
    uint8_t *txBuffer;
    uint8_t rxFUPSourcePortId[PORT_ID_BYTES];
    uint8_t rxRespSourcePortId[PORT_ID_BYTES];

    memset(rxRequestingPortId, 0, PORT_ID_BYTES);
    memset(txRequestingPortId, 0, PORT_ID_BYTES);
    memset(rxFUPRequestingPortId, 0, PORT_ID_BYTES);
    memset(txFUPRequestingPortId, 0, PORT_ID_BYTES);
    memset(rxFUPSourcePortId, 0, PORT_ID_BYTES);
    memset(rxRespSourcePortId, 0, PORT_ID_BYTES);

      /* Grab some information needed for comparisons if we got a PDelay Response */
    if (ptp->ports[port].rcvdPdelayResp)
    {
      get_rx_requesting_port_id(ptp, port, ptp->ports[port].rcvdPdelayRespPtr, rxRequestingPortId);
      txBuffer = get_output_buffer(ptp,port,PTP_TX_PDELAY_REQ_BUFFER);
      get_source_port_id(ptp, port, TRANSMITTED_PACKET, txBuffer, txRequestingPortId);
      rxSequenceId = get_sequence_id(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespPtr);
      txSequenceId = get_sequence_id(ptp, port, TRANSMITTED_PACKET, txBuffer);
      get_source_port_id(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespPtr, rxRespSourcePortId);
    }
    if (ptp->ports[port].rcvdPdelayRespFollowUp)
    {
      get_rx_requesting_port_id(ptp, port, ptp->ports[port].rcvdPdelayRespFollowUpPtr, rxFUPRequestingPortId);
      txBuffer = get_output_buffer(ptp,port,PTP_TX_PDELAY_REQ_BUFFER);
      get_source_port_id(ptp, port, TRANSMITTED_PACKET, txBuffer, txFUPRequestingPortId);
      rxFUPSequenceId = get_sequence_id(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespFollowUpPtr);
      txFUPSequenceId = get_sequence_id(ptp, port, TRANSMITTED_PACKET, txBuffer);
      get_source_port_id(ptp, port, RECEIVED_PACKET, ptp->ports[port].rcvdPdelayRespFollowUpPtr, rxFUPSourcePortId);
    }

      prevState = ptp->ports[port].mdPdelayReq_State;

      switch (ptp->ports[port].mdPdelayReq_State)
      {
        default:
        case MDPdelayReq_NOT_ENABLED:
          if (ptp->ports[port].portEnabled && ptp->ports[port].pttPortEnabled)
          {
#ifdef PATH_DELAY_DEBUG
            printk("Port index %d enabled\n", port);
#endif

            /* Port (and timesync on it) became enabled */
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_INITIAL_SEND_PDELAY_REQ);
          }
          else
          {
            /* Don't time when this port is not enabled */
            ptp->ports[port].pdelayIntervalTimer = 0;
          }
          break;

        case MDPdelayReq_LOST_RESPONSE:
          MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_RESET);
          break;

        case MDPdelayReq_RESET:
#ifdef PATH_DELAY_DEBUG
          printk("Resetting port index %d\n", port);
#endif
          MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_SEND_PDELAY_REQ);
          break;

        case MDPdelayReq_INITIAL_SEND_PDELAY_REQ:
        case MDPdelayReq_SEND_PDELAY_REQ:
          if (ptp->ports[port].rcvdMDTimestampReceive)
          {
#ifdef PATH_DELAY_DEBUG
            printk("PDelay Request Tx Timestamp available (port index %d).\n", port);
#endif

            /* The transmit timestamp for the request is available */
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_WAITING_FOR_PDELAY_RESP);
          }
          else if (ptp->ports[port].pdelayIntervalTimer >= PDELAY_REQ_INTERVAL_TICKS(ptp, port))
          {
            /* We didn't see a timestamp for some reason (this can happen on startup sometimes) */
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_RESET);
#ifdef PATH_DELAY_DEBUG
            printk("ptp: We didn't see a timestamp for some reason on port %d\n", port);
#endif

          }
          break;

        case MDPdelayReq_WAITING_FOR_PDELAY_RESP:
          if ((ptp->ports[port].pdelayIntervalTimer >= PDELAY_REQ_INTERVAL_TICKS(ptp, port)))
          { /* Timeout */
            #ifdef DEBUG_OUTPUT
              if (port == 0)
                printk("Timeout WAITING_FOR_PDELAY_RESP\n");
            #endif
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_LOST_RESPONSE);
          }
          else
          if (ptp->ports[port].rcvdPdelayResp)
          { // Response received
            if ((compare_port_ids(rxRequestingPortId, txRequestingPortId) != 0) ||  // If the responder says it's not responding to us
              (compare_port_ids(rxRespSourcePortId, txRequestingPortId) == 0) ||    // If we're responding to ourself
              (rxSequenceId != txSequenceId))                                       // If the sequence number does not match
            { // Non-matching response
#if defined(PATH_DELAY_DEBUG) || defined(DEBUG_OUTPUT)
                if (port == 0)
                  printk("Resetting %d: intervalTimer %d, reqInterval %d, rcvdPdelayResp %d, rcvdPdelayRespPtr %d, rxSequence %d, txSequence %d\n",
              port, ptp->ports[port].pdelayIntervalTimer, PDELAY_REQ_INTERVAL_TICKS(ptp, port), ptp->ports[port].rcvdPdelayResp,
                    (int) ptp->ports[port].rcvdPdelayRespPtr, rxSequenceId, txSequenceId);
#endif
#ifdef PATH_DELAY_DEBUG
            int i;
            printk("rxRequestingPortID:");
            for (i=0; i<PORT_ID_BYTES; i++) printk("%02X", rxRequestingPortId[i]);
            printk("\n");
            printk("txRequestingPortID:");
            for (i=0; i<PORT_ID_BYTES; i++) printk("%02X", txRequestingPortId[i]);
            printk("\n");
#endif
              // Ignore this resp.  Processing it is causing rapid fire req's.
              //MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_LOST_RESPONSE);
              ptp->ports[port].rcvdPdelayResp = FALSE;
          }
            else
            { // Matching response
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_WAITING_FOR_PDELAY_RESP_FOLLOW_UP);
          }
          }
          break;

        case MDPdelayReq_WAITING_FOR_PDELAY_RESP_FOLLOW_UP:
          if (ptp->ports[port].pdelayIntervalTimer >= PDELAY_REQ_INTERVAL_TICKS(ptp, port))
          { // Timeout
#ifdef DEBUG_OUTPUT
              if (port == 0)
                printk("Timeout WAITING_FOR_PDELAY_RESP_FOLLOW_UP\n");

  #endif
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_LOST_RESPONSE);
            break;
          }

          if (ptp->ports[port].rcvdPdelayResp)
          { // Response received
            ptp->ports[port].rcvdPdelayResp = FALSE;
#ifdef DEBUG_OUTPUT
              if (port == 0)
                printk("Response received WAITING_FOR_PDELAY_RESP_FOLLOW_UP\n");
#endif
            if ((compare_port_ids(rxRequestingPortId, txRequestingPortId) != 0) ||  // If the responder says it's not responding to us
              (compare_port_ids(rxRespSourcePortId, txRequestingPortId) == 0) ||    // If we're responding to ourself
              (rxSequenceId != txSequenceId))                                       // If the sequence number does not match
            { // Non-matching response
#ifdef DEBUG_OUTPUT
                if (port == 0)
                  printk("Non-matching response received WAITING_FOR_PDELAY_RESP_FOLLOW_UP\n");
#endif
              // Ignore this resp.  Processing it is causing rapid fire req's.
             //MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_LOST_RESPONSE);
          }
            else
            { // Matching duplicate response
               /* AVnu_PTP-5 from AVnu Combined Endpoint PICS D.0.0.1
                   Cease pDelay_Req transmissions if more than one
                   pDelay_Resp messages have been received for each of
                   three consecutive pDelay_Req messages. */
#ifdef DEBUG_OUTPUT
                if (port == 0) {
                  printk("Duplicate response received WAITING_FOR_PDELAY_RESP_FOLLOW_UP\n");
                  printk("pdelayResponses %d multiplePdelayResponses %d prevPdelayReqSequenceId %d txSequenceId %d\n",
                  ptp->ports[port].pdelayResponses, ptp->ports[port].multiplePdelayResponses, ptp->ports[port].prevPdelayReqSequenceId, txSequenceId);
                }
#endif
                if (ptp->ports[port].pdelayResponses == 2)
                { /* We've received more than one response to a req */
                    ptp->ports[port].multiplePdelayResponses++;
                    if (ptp->ports[port].multiplePdelayResponses > 1)
                    { /* We've had more than one multiple response occurrence */
                        if ((ptp->ports[port].prevPdelayReqSequenceId + 1) == txSequenceId)
                        { /* The occurrences are on consecutive reqs */
                            if (ptp->ports[port].multiplePdelayResponses >= 3)
                            { /* We've had too many multiple responses, disable */
                                printk("Disabling AS on port %d due to multiple pdelay responses (%d %d).\n", port+1, ptp->ports[port].pdelayResponses, ptp->ports[port].multiplePdelayResponses);
                                ptp->ports[port].multiplePdelayTimer = ((5 * 60 * 1000) / PTP_TIMER_TICK_MS);
                                ptp->ports[port].portEnabled = FALSE;
                                MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_NOT_ENABLED);
                                break;
                            }
                        }
                        else
                        { /* The occurrences are not consecutive */
                            /* ...so this becomes the first of a new series */
                            ptp->ports[port].multiplePdelayResponses = 1;
                        }
                    }
                    /* Save our sequence number to be compared against next time around */
                    ptp->ports[port].prevPdelayReqSequenceId = txSequenceId;
                }
            }
          }
          if (ptp->ports[port].rcvdPdelayRespFollowUp)
          { // Received follow up
            if ((rxFUPSequenceId != txFUPSequenceId) ||                                               // If the sequence number does not match
                (compare_port_ids(rxFUPRequestingPortId, txFUPRequestingPortId) != 0) ||              // If the responder says it's not responding to us
                (compare_port_ids(ptp->ports[port].lastRxRespSourcePortId, rxFUPSourcePortId) != 0))  // If the response and follow up are not from the same responder
            { // Non-matching follow up
              // Ignore this follow up.  Processing it is causing failure of 15.2b1 due to rapid fire req's.
              //MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_LOST_RESPONSE);
              ptp->ports[port].rcvdPdelayRespFollowUp = FALSE;
            }
            else
            { // Matching follow up
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_WAITING_FOR_PDELAY_INTERVAL_TIMER);
          }
         }
          break;

        case MDPdelayReq_WAITING_FOR_PDELAY_INTERVAL_TIMER:
          if (ptp->ports[port].pdelayIntervalTimer >= PDELAY_REQ_INTERVAL_TICKS(ptp, port))
          {
            /* Request interval timer expired */
            MDPdelayReq_StateMachine_SetState(ptp, port, MDPdelayReq_SEND_PDELAY_REQ);
          }
          break;
      }

    } while (prevState != ptp->ports[port].mdPdelayReq_State);
  }
}

/* 802.1AS LinkDelaySyncIntervalSetting state machine (11.2.17.2) entry actions */
static void LinkDelaySyncIntervalSetting_StateMachine_SetState(struct ptp_device *ptp, uint32_t port, LinkDelaySyncIntervalSetting_State_t newState)
{
#ifdef PATH_DELAY_DEBUG
  printk("LinkDelaySyncIntervalSetting: Set State %d (port index %d)\n", newState, port);
#endif

  ptp->ports[port].linkDelaySyncIntervalSetting_State = newState;

  switch (newState)
  {
    default:
    case LinkDelaySyncIntervalSetting_NOT_ENABLED:
      break;

    case LinkDelaySyncIntervalSetting_INITIALIZE:
      ptp->ports[port].currentLogPdelayReqInterval = ptp->ports[port].initialLogPdelayReqInterval;
      ptp->ports[port].currentLogSyncInterval = ptp->ports[port].initialLogSyncInterval;
      ptp->ports[port].computeNeighborRateRatio = TRUE;
      ptp->ports[port].computeNeighborPropDelay = TRUE;
      ptp->ports[port].rcvdSignalingMsg1 = FALSE;
      break;

    case LinkDelaySyncIntervalSetting_SET_INTERVALS:
    {
      uint32_t offset = LINK_DELAY_INTERVAL_OFFSET;
      uint32_t packetWord = read_packet(ptp->ports[port].rcvdSignalingPtr, &offset);
      int8_t linkDelayInterval = (int8_t)((packetWord >> 24) & 0xFF);
      int8_t timeSyncInterval = (int8_t)((packetWord >> 16) & 0xFF);
      //int8_t announceInterval = (int8_t)((packetWord >> 8) & 0xFF);
      uint8_t flags = (uint8_t)(packetWord & 0xFF);

      switch (linkDelayInterval)
      {
        case (-128): /* don't change the interval */
          break;
        case 126: /* set interval to initial value */
          ptp->ports[port].currentLogPdelayReqInterval = ptp->ports[port].initialLogPdelayReqInterval;
          break;
        default: /* use the indicated value */
          ptp->ports[port].currentLogPdelayReqInterval = linkDelayInterval;
          break;
      }

      switch (timeSyncInterval)
      {
        case (-128): /* don't change the interval */
          break;
        case 126: /* set interval to initial value */
          ptp->ports[port].currentLogSyncInterval = ptp->ports[port].initialLogSyncInterval;
          break;
        default: /* use indicated value */
          ptp->ports[port].currentLogSyncInterval = timeSyncInterval;
          break;
      }

      ptp->ports[port].computeNeighborRateRatio = flags & (1<<0);
      ptp->ports[port].computeNeighborPropDelay = flags & (1<<1);

      ptp->ports[port].rcvdSignalingMsg1 = FALSE;
      break;
    }
  }
}

/* 802.1AS LinkDelaySyncIntervalSetting state machine (11.2.17.2) transitions */
void LinkDelaySyncIntervalSetting_StateMachine(struct ptp_device *ptp, uint32_t port)
{
  if (!ptp->ports[port].portEnabled || !ptp->ports[port].pttPortEnabled)
  {
    if (ptp->ports[port].linkDelaySyncIntervalSetting_State != LinkDelaySyncIntervalSetting_NOT_ENABLED)
    {
      /* Disabling the port immediately forces the state machine into the disabled state */
      LinkDelaySyncIntervalSetting_StateMachine_SetState(ptp, port, LinkDelaySyncIntervalSetting_NOT_ENABLED);
    }
  }
  else
  {
    LinkDelaySyncIntervalSetting_State_t prevState;
    do
    {
      prevState = ptp->ports[port].linkDelaySyncIntervalSetting_State;

      switch (ptp->ports[port].linkDelaySyncIntervalSetting_State)
      {
        default:
        case LinkDelaySyncIntervalSetting_NOT_ENABLED:
          if (ptp->ports[port].portEnabled && ptp->ports[port].pttPortEnabled)
          {
            /* Port (and timesync on it) became enabled */
            LinkDelaySyncIntervalSetting_StateMachine_SetState(ptp, port, LinkDelaySyncIntervalSetting_INITIALIZE);
          }
          break;

        case LinkDelaySyncIntervalSetting_INITIALIZE:
        case LinkDelaySyncIntervalSetting_SET_INTERVALS:
          if (ptp->ports[port].rcvdSignalingMsg1)
          {
            LinkDelaySyncIntervalSetting_StateMachine_SetState(ptp, port, LinkDelaySyncIntervalSetting_SET_INTERVALS);
          }
          break;
      }

    } while (prevState != ptp->ports[port].linkDelaySyncIntervalSetting_State);
  }
}


