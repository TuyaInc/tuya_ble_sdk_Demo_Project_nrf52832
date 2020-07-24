
#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

//#define   TUYA_DFU_SETTINGS_BOOTLOADER   0


void ble_device_disconnected(void);

uint32_t ble_nus_send_mtu(const uint8_t * data, uint16_t length);

void uart_init(void);

void update_adv_data(uint8_t const* p_ad_data, uint8_t ad_len);

void update_scan_rsp_data(uint8_t const *p_sr_data, uint8_t sr_len);


#ifdef __cplusplus
}
#endif

#endif // 


