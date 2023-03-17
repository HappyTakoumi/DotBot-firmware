/**
 * @file rpm.c
 * @addtogroup BSP
 *
 * @brief  nRF52833-specific definition of the "rpm" bsp module.
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @copyright Inria, 2022
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <nrf.h>
#include "rpm.h"
#include "timer.h"

//=========================== defines ==========================================

#define RPM_LEFT_PIN          (17)          ///< Number of the pin connected to the left magnetic encoder
#define RPM_LEFT_PORT         (NRF_P0)      ///< Port of the pin connected to the left magnetic encoder
#define RPM_LEFT_TIMER        (NRF_TIMER0)  ///< Timer peripheral used to count left cycles
#define RPM_LEFT_PPI_CHAN     (0)           ///< PPI channel used between left side timer and gpio
#define RPM_LEFT_GPIOTE_CHAN  (0)           ///< GPIOTE channel used for left side gpio event
#define RPM_RIGHT_PIN         (15)          ///< Number of the pin connected to the right magnetic encoder
#define RPM_RIGHT_PORT        (NRF_P0)      ///< Port of the pin connected to the right magnetic encoder
#define RPM_RIGHT_TIMER       (NRF_TIMER1)  ///< Timer peripheral used to count right cycles
#define RPM_RIGHT_PPI_CHAN    (1)           ///< PPI channel used between right side timer and gpio
#define RPM_RIGHT_GPIOTE_CHAN (1)           ///< GPIOTE channel used for right side gpio event
#define RPM_UPDATE_PERIOD_MS  (50)          ///< Counters update period in ms

/**
 * Helper macro to compute speed in cm/s
 *
 * computed from the number of cycles measured within the last 50ms (one rotation is 3.77mm distance of the wheel)
 */
#define RPM_CYCLES_TO_SPEED(cycles) (float)(377.0 * cycles / RPM_UPDATE_PERIOD_MS)

/**
 * Helper macro to compute rotation per minute
 *
 * 1 cycle corresponds to one rotation, so convert to the number of minutes, given the RTC frequency of 50ms
 */
#define RPM_CYCLES_TO_RPM(cycles) (60 * 1000 * cycles / RPM_UPDATE_PERIOD_MS)

/**
 * Helper macro to compute rotation per second
 *
 * 1 cycle corresponds to one rotation, so convert to the number of seconds, given the RTC frequency of 50ms
 */
#define RPM_CYCLES_TO_RPS(cycles) (cycles * 1000 / RPM_UPDATE_PERIOD_MS)

/**
 * Helper struct used to store internal state variables
 */
typedef struct {
    uint32_t last_left_counts;
    uint32_t previous_left_counts;
    uint32_t last_right_counts;
    uint32_t previous_right_counts;
} rpm_vars_t;

//=========================== variables ========================================

/*
 * Global variable used to store cycle counts at the beginning and at the end of
 * an RTC timeframe (50ms), for each side
 */
static rpm_vars_t _rpm_vars;

//=========================== prototypes =======================================

/**
 * Compute the number of the left encoder cycles during the last 50ms time
 * frame. The function takes into account timer overflows.
 */
static uint32_t _db_rpm_left_cycles(void);

/**
 * Compute the number of the right encoder cycles during the last 50ms time
 * frame. The function takes into account timer overflows.
 */
static uint32_t _db_rpm_right_cycles(void);

static void _update_counters(void);

//=========================== public ===========================================

void db_rpm_init(void) {
    // Configure pin connected to left magnetic encoder sensor, input pullup
    RPM_LEFT_PORT->PIN_CNF[RPM_LEFT_PIN] |= (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
    RPM_LEFT_PORT->PIN_CNF[RPM_LEFT_PIN] &= ~(1UL << GPIO_PIN_CNF_INPUT_Pos);
    NRF_GPIOTE->CONFIG[RPM_LEFT_GPIOTE_CHAN] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
                                               (RPM_LEFT_PIN << GPIOTE_CONFIG_PSEL_Pos) |
                                               (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // Configure pin connected to right magnetic encoder sensor, input pullup
    RPM_RIGHT_PORT->PIN_CNF[RPM_RIGHT_PIN] |= (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
    RPM_RIGHT_PORT->PIN_CNF[RPM_RIGHT_PIN] &= ~(1UL << GPIO_PIN_CNF_INPUT_Pos);
    NRF_GPIOTE->CONFIG[RPM_RIGHT_GPIOTE_CHAN] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
                                                (RPM_RIGHT_PIN << GPIOTE_CONFIG_PSEL_Pos) |
                                                (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // Configure and clear timers
    // Timers are configured in counter mode with 32bit length
    RPM_LEFT_TIMER->MODE        = TIMER_MODE_MODE_LowPowerCounter;
    RPM_LEFT_TIMER->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
    RPM_LEFT_TIMER->INTENCLR    = 1;
    RPM_LEFT_TIMER->TASKS_CLEAR = 1;

    RPM_RIGHT_TIMER->MODE        = TIMER_MODE_MODE_LowPowerCounter;
    RPM_RIGHT_TIMER->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
    RPM_RIGHT_TIMER->INTENCLR    = 1;
    RPM_RIGHT_TIMER->TASKS_CLEAR = 1;

    // Configure gpiote/timer count PPI
    //   - PO.17 (left magnetic encoder) event connected to PPI channel 0 which fires Timer 0 capture 0
    //   - PO.15 (right magnetic encoder) event connected to PPI channel 1 which fires Timer 0 capture 1
    NRF_PPI->CH[RPM_LEFT_PPI_CHAN].EEP  = (uint32_t)&NRF_GPIOTE->EVENTS_IN[RPM_LEFT_GPIOTE_CHAN];
    NRF_PPI->CH[RPM_LEFT_PPI_CHAN].TEP  = (uint32_t)&RPM_LEFT_TIMER->TASKS_COUNT;
    NRF_PPI->CH[RPM_RIGHT_PPI_CHAN].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[RPM_RIGHT_GPIOTE_CHAN];
    NRF_PPI->CH[RPM_RIGHT_PPI_CHAN].TEP = (uint32_t)&RPM_RIGHT_TIMER->TASKS_COUNT;

    // Enable PPI channels
    NRF_PPI->CHENSET = (1 << RPM_RIGHT_PPI_CHAN) | (1 << RPM_LEFT_PPI_CHAN);

    // Configure RTC timer period used to update counters
    db_timer_init();
    db_timer_set_periodic_ms(0, RPM_UPDATE_PERIOD_MS, &_update_counters);

    // Start timers used as counters
    RPM_LEFT_TIMER->TASKS_START  = 1;
    RPM_RIGHT_TIMER->TASKS_START = 1;
}

void db_rpm_get_values(rpm_values_t *values) {
    uint32_t left  = _db_rpm_left_cycles();
    uint32_t right = _db_rpm_right_cycles();

    values->left.rpm   = RPM_CYCLES_TO_RPM(left);
    values->left.rps   = RPM_CYCLES_TO_RPS(left);
    values->left.speed = RPM_CYCLES_TO_SPEED(left);

    values->right.rpm   = RPM_CYCLES_TO_RPM(right);
    values->right.rps   = RPM_CYCLES_TO_RPS(right);
    values->right.speed = RPM_CYCLES_TO_SPEED(right);
}

//=========================== private ==========================================

static void _update_counters(void) {
    // Copy the last counts from previous time frame in the corresponding variables
    _rpm_vars.previous_left_counts  = _rpm_vars.last_left_counts;
    _rpm_vars.previous_right_counts = _rpm_vars.last_right_counts;

    // Move current timers count to CC registers
    RPM_LEFT_TIMER->TASKS_CAPTURE[0]  = 1;
    RPM_RIGHT_TIMER->TASKS_CAPTURE[0] = 1;

    // Update last counts variables with the value in CC
    _rpm_vars.last_left_counts  = RPM_LEFT_TIMER->CC[0];
    _rpm_vars.last_right_counts = RPM_RIGHT_TIMER->CC[0];
}

static uint32_t _db_rpm_left_cycles(void) {
    if (_rpm_vars.last_left_counts < _rpm_vars.previous_left_counts) {
        return UINT32_MAX - _rpm_vars.previous_left_counts + _rpm_vars.last_left_counts;
    }
    return _rpm_vars.last_left_counts - _rpm_vars.previous_left_counts;
}

static uint32_t _db_rpm_right_cycles(void) {
    if (_rpm_vars.last_right_counts < _rpm_vars.previous_right_counts) {
        return UINT32_MAX - _rpm_vars.previous_right_counts + _rpm_vars.last_right_counts;
    }
    return _rpm_vars.last_right_counts - _rpm_vars.previous_right_counts;
}