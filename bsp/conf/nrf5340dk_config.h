#ifndef __BOARD_NRF5340DK_CONFIG_H
#define __BOARD_NRF5340DK_CONFIG_H

/**
 * @file nrf5340dk_config.h
 * @addtogroup BSP
 *
 * @brief  nRF5340DK board specific definitions.
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @copyright Inria, 2023
 */

/**
 * @name    Debug pins definitions
 * @{
 */
#define DB_DEBUG1_PORT 0
#define DB_DEBUG1_PIN  28
#define DB_DEBUG2_PORT 0
#define DB_DEBUG2_PIN  29
#define DB_DEBUG3_PORT 0
#define DB_DEBUG3_PIN  30
/** @} */

/**
 * @name    LEDs pins definitions
 * @{
 */
#define DB_LED1_PORT DB_DEBUG1_PORT
#define DB_LED1_PIN  DB_DEBUG1_PIN
#define DB_LED2_PORT DB_DEBUG2_PORT
#define DB_LED2_PIN  DB_DEBUG2_PIN
#define DB_LED3_PORT DB_DEBUG3_PORT
#define DB_LED3_PIN  DB_DEBUG3_PIN
#define DB_LED4_PORT 0
#define DB_LED4_PIN  31
/** @} */

/**
 * @name    Buttons pins definitions
 * @{
 */
#define DB_BTN1_PORT 0
#define DB_BTN1_PIN  23
#define DB_BTN2_PORT 0
#define DB_BTN2_PIN  24
#define DB_BTN3_PORT 0
#define DB_BTN3_PIN  8
#define DB_BTN4_PORT 0
#define DB_BTN4_PIN  9
/** @} */

/**
 * @name    I2C pins definitions
 * @{
 */
#define DB_I2C_SCL_PORT 1
#define DB_I2C_SCL_PIN  3
#define DB_I2C_SDA_PORT 1
#define DB_I2C_SDA_PIN  2
/** @} */

/**
 * @name    UART pins definitions
 * @{
 */
#define DB_UART_RX_PORT 0
#define DB_UART_RX_PIN  1
#define DB_UART_TX_PORT 1
#define DB_UART_TX_PIN  1
/** @} */

/**
 * @name    LH2 event and data pins definitions
 * @{
 */
#define DB_LH2_E_PORT 0
#define DB_LH2_E_PIN  26
#define DB_LH2_D_PORT 0
#define DB_LH2_D_PIN  25
/** @} */

/**
 * @name    Motor driver pins definitions
 * @{
 */
#define DB_MOTOR_AIN1_PORT DB_LED1_PORT
#define DB_MOTOR_AIN1_PIN  DB_LED1_PIN
#define DB_MOTOR_AIN2_PORT DB_LED2_PORT
#define DB_MOTOR_AIN2_PIN  DB_LED2_PIN
#define DB_MOTOR_BIN1_PORT DB_LED3_PORT
#define DB_MOTOR_BIN1_PIN  DB_LED3_PIN
#define DB_MOTOR_BIN2_PORT DB_LED4_PORT
#define DB_MOTOR_BIN2_PIN  DB_LED4_PIN
/** @} */

/**
 * @name    Magnetic encoders pins definitions
 * @{
 */
#define DB_RPM_LEFT_PORT  DB_BTN1_PORT
#define DB_RPM_LEFT_PIN   DB_BTN1_PIN
#define DB_RPM_RIGHT_PORT DB_BTN2_PORT
#define DB_RPM_RIGHT_PIN  DB_BTN2_PIN
/** @} */

/**
 * @name    LSM6DS pin definitions
 * @{
 */
#define DB_LSM6DS_INT_PORT DB_DEBUG3_PORT
#define DB_LSM6DS_INT_PIN  DB_DEBUG3_PIN
/** @} */

/**
 * @name    LIS2MDL pin definitions
 * @{
 */
#define DB_LIS2MDL_INT_PORT DB_DEBUG3_PORT
#define DB_LIS2MDL_INT_PIN  DB_DEBUG3_PIN
/** @} */

#endif
