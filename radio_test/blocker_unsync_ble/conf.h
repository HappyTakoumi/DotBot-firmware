#include <nrf.h>

#define RADIO_MODE 1
#define FREQUENCY 8

#if defined(NRF5340_XXAA) && defined(NRF_APPLICATION)
#define RADIO_TXPOWER_TXPOWER_0dBm 0
#endif

#define POWER RADIO_TXPOWER_TXPOWER_Pos8dBm
