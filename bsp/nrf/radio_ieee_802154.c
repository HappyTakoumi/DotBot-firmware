/**
 * @file
 * @ingroup bsp_radio_ieee_802154
 *
 * @brief  nRF52833-specific definition of the "radio" bsp module.
 *
 * @author Said Alvarado-Marin <said-alexander.alvarado-marin@inria.fr>
 * @author Simoes Raphael <raphael.simoes@inria.fr>
 *
 * @copyright Inria, 2024
 */
#include <nrf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "radio_ieee_802154.h"

//=========================== defines ==========================================

#if defined(NRF5340_XXAA) && defined(NRF_NETWORK)
#define NRF_RADIO NRF_RADIO_NS
#endif

#define IEEE802154_FRAME_LEN_MAX (127U)
#if defined(NRF5340_XXAA) && defined(NRF_NETWORK)
#define RADIO_INTERRUPT_PRIORITY 2
#else
#define RADIO_INTERRUPT_PRIORITY 1
#endif

#define RADIO_TIFS          1000U  ///< Inter frame spacing in us
#define RADIO_SHORTS_COMMON (RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos) |                 \
                                (RADIO_SHORTS_END_DISABLE_Enabled << RADIO_SHORTS_END_DISABLE_Pos) |             \
                                (RADIO_SHORTS_ADDRESS_RSSISTART_Enabled << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) | \
                                (RADIO_SHORTS_DISABLED_RSSISTOP_Enabled << RADIO_SHORTS_DISABLED_RSSISTOP_Pos)
#define RADIO_INTERRUPTS (RADIO_INTENSET_DISABLED_Enabled << RADIO_INTENSET_DISABLED_Pos) | \
                             (RADIO_INTENSET_ADDRESS_Enabled << RADIO_INTENSET_ADDRESS_Pos)
#define RADIO_STATE_IDLE 0x00
#define RADIO_STATE_RX   0x01
#define RADIO_STATE_TX   0x02
#define RADIO_STATE_BUSY 0x04

typedef struct __attribute__((packed)) {
    uint8_t length;                             ///< Length of the payload + MIC (if any)
    uint8_t payload[IEEE802154_FRAME_LEN_MAX];  ///< Payload + MIC (if any)
} ieee_radio_pdu_t;

typedef struct {
    ieee_radio_pdu_t       pdu;       ///< Variable that stores the radio PDU (protocol data unit) that arrives and the radio packets that are about to be sent for IEEE 802.15.4.
    radio_ieee_802154_cb_t callback;  ///< Function pointer, stores the callback to use in the RADIO_Irq handler.
    uint8_t                state;     ///< Internal state of the radio
} radio_vars_t;

//=========================== variables ========================================

static const uint8_t _chan_to_freq[40] = {
    4, 6, 8,
    10, 12, 14, 16, 18,
    20, 22, 24, 28,
    30, 32, 34, 36, 38,
    40, 42, 44, 46, 48,
    50, 52, 54, 56, 58,
    60, 62, 64, 66, 68,
    70, 72, 74, 76, 78,
    2, 26, 80  // Advertising channels
};

static radio_vars_t radio_vars = { 0 };

//========================== prototypes ========================================

static void _radio_enable(void);

//=========================== public ===========================================

void db_radio_ieee_802154_init(radio_ieee_802154_cb_t callback) {

#if defined(NRF5340_XXAA)
    // On nrf53 configure constant latency mode for better performances
    NRF_POWER_NS->TASKS_CONSTLAT = 1;
#endif

    // Reset radio to its initial values
    NRF_RADIO->POWER = (RADIO_POWER_POWER_Disabled << RADIO_POWER_POWER_Pos);
    NRF_RADIO->POWER = (RADIO_POWER_POWER_Enabled << RADIO_POWER_POWER_Pos);

#if defined(NRF5340_XXAA)
    // Copy all the RADIO trim values from FICR into the target addresses (from errata v1.6 - 3.29 [158])
    for (uint32_t index = 0; index < 32ul && NRF_FICR_NS->TRIMCNF[index].ADDR != (uint32_t *)0xFFFFFFFFul; index++) {
        if (((uint32_t)NRF_FICR_NS->TRIMCNF[index].ADDR & 0xFFFFF000ul) == (volatile uint32_t)NRF_RADIO_NS) {
            *((volatile uint32_t *)NRF_FICR_NS->TRIMCNF[index].ADDR) = NRF_FICR_NS->TRIMCNF[index].DATA;
        }
    }
#endif

    // General configuration of the radio.
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Ieee802154_250Kbit << RADIO_MODE_MODE_Pos);  // Configure 802.15.4 mode

#if defined(NRF5340_XXAA)
    // From errata v1.6 - 3.15 [117] RADIO: Changing MODE requires additional configuration
    if (mode == DB_RADIO_BLE_2MBit) {
        *((volatile uint32_t *)0x41008588) = *((volatile uint32_t *)0x01FF0084);
    } else {
        *((volatile uint32_t *)0x41008588) = *((volatile uint32_t *)0x01FF0080);
    }

#endif

    db_radio_ieee_802154_set_tx_power(RADIO_TXPOWER_TXPOWER_0dBm);
    NRF_RADIO->PCNF0 = (8 << RADIO_PCNF0_LFLEN_Pos) |
                       (RADIO_PCNF0_PLEN_32bitZero << RADIO_PCNF0_PLEN_Pos) |
                       (RADIO_PCNF0_CRCINC_Include << RADIO_PCNF0_CRCINC_Pos);

    NRF_RADIO->PCNF1 = (127 << RADIO_PCNF1_MAXLEN_Pos);

    // Configuring the on-air radio address.
    NRF_RADIO->BASE0 = DEFAULT_NETWORK_ADDRESS_IEEE;
    // only send using logical address 0
    NRF_RADIO->TXADDRESS = 0UL;
    // only receive from logical address 0
    NRF_RADIO->RXADDRESSES = (RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos);

    // Inter frame spacing in us
    NRF_RADIO->TIFS = RADIO_TIFS;

    // CRC Config &

    NRF_RADIO->CRCCNF  = (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos) | (RADIO_CRCCNF_SKIPADDR_Ieee802154 << RADIO_CRCCNF_SKIPADDR_Pos);  // Checksum uses 2 bytes, and is enabled.
    NRF_RADIO->CRCINIT = 0;                                                                                                                 // initial value
    NRF_RADIO->CRCPOLY = 0x11021;
    // Configure pointer to PDU for EasyDMA
    NRF_RADIO->PACKETPTR = (uint32_t)&radio_vars.pdu;

    // Assign the callback function that will be called when a radio packet is received.
    radio_vars.callback = callback;
    radio_vars.state    = RADIO_STATE_IDLE;

    // Configure the external High-frequency Clock. (Needed for correct operation)
    db_hfclk_init();

    // Configure the Interruptions
    NVIC_SetPriority(RADIO_IRQn, RADIO_INTERRUPT_PRIORITY);  // Set priority for Radio interrupts to 1
    // Clear all radio interruptions
    NRF_RADIO->INTENCLR = 0xffffffff;
    NVIC_EnableIRQ(RADIO_IRQn);
}

void db_radio_ieee_802154_set_frequency(uint8_t freq) {

    NRF_RADIO->FREQUENCY = freq << RADIO_FREQUENCY_FREQUENCY_Pos;
}

void db_radio_ieee_802154_set_channel(uint8_t channel) {
    NRF_RADIO->FREQUENCY = (_chan_to_freq[channel] << RADIO_FREQUENCY_FREQUENCY_Pos);
}

void db_radio_ieee_802154_set_tx_power(uint8_t power) {
    NRF_RADIO->TXPOWER = (power << RADIO_TXPOWER_TXPOWER_Pos);
}

void db_radio_ieee_802154_set_network_address(uint32_t addr) {
    NRF_RADIO->BASE0 = addr;
}

void db_radio_ieee_802154_tx(const uint8_t *tx_buffer, uint8_t length) {

    radio_vars.pdu.length = length;
    memcpy(radio_vars.pdu.payload, tx_buffer, length);

    NRF_RADIO->SHORTS   = RADIO_SHORTS_COMMON | (RADIO_SHORTS_DISABLED_RXEN_Enabled << RADIO_SHORTS_DISABLED_RXEN_Pos);
    NRF_RADIO->INTENSET = RADIO_INTERRUPTS;

    if (radio_vars.state == RADIO_STATE_IDLE) {
        _radio_enable();
        db_radio_ieee_802154_tx_start();
    }

    radio_vars.state = RADIO_STATE_TX;
    while (radio_vars.state != RADIO_STATE_TX) {}
}

void db_radio_ieee_802154_rx(void) {
    NRF_RADIO->SHORTS   = RADIO_SHORTS_COMMON | (RADIO_SHORTS_DISABLED_RXEN_Enabled << RADIO_SHORTS_DISABLED_RXEN_Pos);
    NRF_RADIO->INTENSET = RADIO_INTERRUPTS;

    if (radio_vars.state == RADIO_STATE_IDLE) {
        _radio_enable();
        NRF_RADIO->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;
    }
    radio_vars.state = RADIO_STATE_RX;
}

void db_radio_ieee_802154_tx_start(void) {
    NRF_RADIO->TASKS_TXEN = RADIO_TASKS_TXEN_TASKS_TXEN_Trigger << RADIO_TASKS_TXEN_TASKS_TXEN_Pos;
}

void db_radio_ieee_802154_disable(void) {
    NRF_RADIO->INTENCLR        = RADIO_INTERRUPTS;
    NRF_RADIO->SHORTS          = 0;
    NRF_RADIO->EVENTS_TXREADY  = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->TASKS_DISABLE   = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger << RADIO_TASKS_DISABLE_TASKS_DISABLE_Pos;
    while (NRF_RADIO->EVENTS_DISABLED == 0) {}
    radio_vars.state = RADIO_STATE_IDLE;
}

int8_t db_radio_ieee_802154_rssi(void) {
    return (uint8_t)NRF_RADIO->RSSISAMPLE * -1;
}

//=========================== private ==========================================

static void _radio_enable(void) {
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->INTENSET        = RADIO_INTERRUPTS;
}

//=========================== interrupt handlers ===============================

/**
 * @brief Interruption handler for the Radio.
 *
 * This function will be called each time a radio packet is received.
 * it will clear the interrupt, copy the last received packet
 * and called the user-defined callback to process the package.
 *
 */
void RADIO_IRQHandler(void) {

    if (NRF_RADIO->EVENTS_ADDRESS) {
        NRF_RADIO->EVENTS_ADDRESS = 0;
        radio_vars.state |= RADIO_STATE_BUSY;
    }

    if (NRF_RADIO->EVENTS_DISABLED) {
        // Clear the Interrupt flag
        NRF_RADIO->EVENTS_DISABLED = 0;

        if (radio_vars.state == (RADIO_STATE_BUSY | RADIO_STATE_RX)) {
            if (radio_vars.callback) {
                radio_vars.callback(radio_vars.pdu.payload, radio_vars.pdu.length, NRF_RADIO->CRCSTATUS == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
            }
            radio_vars.state = RADIO_STATE_RX;
        } else {  // TX
            radio_vars.state = RADIO_STATE_RX;
        }
    }
}