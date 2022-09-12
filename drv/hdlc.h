#ifndef __HDLC_H
#define __HDLC_H

/**
 * @file hdlc.h
 * @addtogroup DRV
 *
 * @brief  Cross-platform declaration "hdlc" driver module.
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @copyright Inria, 2022
 */

#include <stdlib.h>
#include <stdint.h>

//=========================== definitions ======================================

typedef enum {
    DB_HDLC_STATE_IDLE,       ///< Waiting for incoming HDLC frames
    DB_HDLC_STATE_RECEIVING,  ///< An HDLC frame is being received
    DB_HDLC_STATE_READY,      ///< An HDLC frame is ready to be decoded
    DB_HDLC_STATE_ERROR,      ///< The FCS value is invalid
} db_hdlc_state_t;

//=========================== public ===========================================

/**
 * @brief   Handle a byte received in HDLC internal state
 *
 * @param[in]   byte    The received byte
 */
db_hdlc_state_t db_hdlc_rx_byte(uint8_t byte);

/**
 * @brief   Decode an HDLC frame
 *
 * @param[in]   payload     Decoded payload contained in the input buffer
 * @return the number of bytes decoded
 */
size_t db_hdlc_decode(uint8_t *output);

#endif
