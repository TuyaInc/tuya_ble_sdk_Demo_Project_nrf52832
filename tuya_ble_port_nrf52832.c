#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tuya_ble_port.h"
#include "tuya_ble_type.h"
#include "aes.h"
#include "md5.h"
#include "hmac.h"
#include "main.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "ble_nus.h"
#include "ble_advertising.h"
#include "app_timer.h"
#include "nrf_drv_pwm.h"
#include "tuya_ble_internal_config.h"
#include "app_uart.h"
#include "flash.h"
#include "app_util_platform.h"
#include "elog.h"
#include <stdarg.h>


tuya_ble_status_t nrf_err_code_convert(uint32_t errno)
{
    tuya_ble_status_t stat;
    switch (errno) {
    case NRF_SUCCESS:
        stat = TUYA_BLE_SUCCESS;
        break;
    case NRF_ERROR_INTERNAL:
        stat = TUYA_BLE_ERR_INTERNAL;
        break;
    case NRF_ERROR_NOT_FOUND:
        stat = TUYA_BLE_ERR_NOT_FOUND;
        break;
    case NRF_ERROR_NO_MEM:
        stat = TUYA_BLE_ERR_NO_MEM;
        break;
    case NRF_ERROR_INVALID_ADDR:
        stat = TUYA_BLE_ERR_INVALID_ADDR;
        break;
    case NRF_ERROR_INVALID_PARAM:
        stat = TUYA_BLE_ERR_INVALID_PARAM;
        break;
    case NRF_ERROR_INVALID_STATE:
        stat = TUYA_BLE_ERR_INVALID_STATE;
        break;
    case NRF_ERROR_INVALID_LENGTH:
        stat = TUYA_BLE_ERR_INVALID_LENGTH;
        break;
    case NRF_ERROR_DATA_SIZE:
        stat = TUYA_BLE_ERR_DATA_SIZE;
        break;
    case NRF_ERROR_BUSY:
        stat = TUYA_BLE_ERR_BUSY;
        break;
    case NRF_ERROR_TIMEOUT:
        stat = TUYA_BLE_ERR_TIMEOUT;
        break;

    }

    return stat;
}


tuya_ble_status_t tuya_ble_gap_advertising_adv_data_update(uint8_t const* p_ad_data, uint8_t ad_len)
{
    update_adv_data(p_ad_data,ad_len);
    return TUYA_BLE_SUCCESS;
}


tuya_ble_status_t tuya_ble_gap_advertising_scan_rsp_data_update(uint8_t const *p_sr_data, uint8_t sr_len)
{
    update_scan_rsp_data(p_sr_data,sr_len);
    return TUYA_BLE_SUCCESS;
}



tuya_ble_status_t tuya_ble_gap_disconnect(void)
{
     ble_device_disconnected();
     return TUYA_BLE_SUCCESS;
}


tuya_ble_status_t tuya_ble_gap_addr_get(tuya_ble_gap_addr_t *p_addr)
{
    uint32_t       err_code;
    ble_gap_addr_t addr;

    err_code = sd_ble_gap_addr_get(&addr);
    VERIFY_SUCCESS(err_code);
    
    if(addr.addr_type==BLE_GAP_ADDR_TYPE_RANDOM_STATIC)
    {
        p_addr->addr_type = TUYA_BLE_ADDRESS_TYPE_RANDOM;
    }
    else
    {
        p_addr->addr_type = TUYA_BLE_ADDRESS_TYPE_PUBLIC;
    }
    
    memcpy(p_addr->addr,addr.addr,6);
    
    return TUYA_BLE_SUCCESS;
}

tuya_ble_status_t tuya_ble_gap_addr_set(tuya_ble_gap_addr_t *p_addr)
{
    uint32_t       err_code;
    ble_gap_addr_t bt_addr;
    if(p_addr->addr_type == TUYA_BLE_ADDRESS_TYPE_RANDOM)
    {
        bt_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    }
    else
    {
        bt_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
    }

    memcpy(bt_addr.addr,p_addr->addr,6);
    err_code = sd_ble_gap_addr_set(&bt_addr);
    APP_ERROR_CHECK(err_code);
    return TUYA_BLE_SUCCESS;
}


tuya_ble_status_t tuya_ble_gatt_send_data(const uint8_t *p_data,uint16_t len)
{
    uint8_t data_len = len;
    if(data_len>TUYA_BLE_DATA_MTU_MAX)
    {
        data_len = TUYA_BLE_DATA_MTU_MAX;
    }
    ble_nus_send_mtu(p_data,data_len);
    return TUYA_BLE_SUCCESS;
}

extern void  uart_init(void);
	
tuya_ble_status_t tuya_ble_common_uart_init(void)
{    
    
    return TUYA_BLE_SUCCESS;
}

tuya_ble_status_t tuya_ble_common_uart_send_data(const uint8_t *p_data,uint16_t len)
{
    for(uint16_t i = 0;i < len;i++)
	{
		app_uart_put(p_data[i]);
	}
    return TUYA_BLE_SUCCESS;
}



#define TIMER_MAX_NUM               4


typedef struct {
    uint8_t is_avail;
    app_timer_t data;
    uint32_t timeout_value_ms;
} tuya_ble_nrf_timer_item_t;

static tuya_ble_nrf_timer_item_t m_timer_pool[TIMER_MAX_NUM] = {
    [0] = { .is_avail = 1},
    [1] = { .is_avail = 1},
    [2] = { .is_avail = 1},
    [3] = { .is_avail = 1}
};

static app_timer_t* acquire_timer(uint32_t timeout_value_ms)
{
    uint8_t i;
    for (i = 0; i < TIMER_MAX_NUM; i++) 
    {
        if (m_timer_pool[i].is_avail) 
        {
            m_timer_pool[i].is_avail = 0;
            m_timer_pool[i].timeout_value_ms = timeout_value_ms;
            return (void *)&m_timer_pool[i].data ;
        }
    }
    return NULL;
}

static int32_t release_timer(void* timer_id)
{
    for (uint8_t i = 0; i < TIMER_MAX_NUM; i++) {
        if (timer_id == &m_timer_pool[i].data) {
            m_timer_pool[i].is_avail = 1;
            return i;
        }
    }
    return -1;
}

static int32_t find_timer_timeout_value(void* timer_id,uint32_t *value)
{
    for (uint8_t i = 0; i < TIMER_MAX_NUM; i++) {
        if (timer_id == &m_timer_pool[i].data) {
            *value = m_timer_pool[i].timeout_value_ms;
            return i;
        }
    }
    return -1;
}


tuya_ble_status_t tuya_ble_timer_create(void** p_timer_id,uint32_t timeout_value_ms, tuya_ble_timer_mode mode,tuya_ble_timer_handler_t timeout_handler)
{
    tuya_ble_status_t ret = TUYA_BLE_SUCCESS;
    uint32_t errno;
    static uint8_t is_init = 0;
    if (!is_init) 
    {
        errno = app_timer_init();
        is_init = errno == NRF_SUCCESS;
    }

    app_timer_id_t id  = acquire_timer(timeout_value_ms);
    if (id == NULL)
    {
        return TUYA_BLE_ERR_NO_MEM;
    }

    app_timer_mode_t m = mode == TUYA_BLE_TIMER_SINGLE_SHOT ? APP_TIMER_MODE_SINGLE_SHOT : APP_TIMER_MODE_REPEATED;
    
    app_timer_timeout_handler_t handler = timeout_handler;
    
    errno = app_timer_create(&id, m, handler);
    
    *p_timer_id = id;
    
    return nrf_err_code_convert(errno);

}


tuya_ble_status_t tuya_ble_timer_delete(void* timer_id)
{
    uint32_t errno;
    int id = release_timer(timer_id);
    if (id == -1)
    {
        return TUYA_BLE_ERR_INVALID_PARAM;
    }

    errno = app_timer_stop((app_timer_id_t)timer_id);
    return nrf_err_code_convert(errno);

}

tuya_ble_status_t tuya_ble_timer_start(void* timer_id)
{
    uint32_t errno;
    uint32_t timeout_value_ms;
    
    if(find_timer_timeout_value(timer_id,&timeout_value_ms)>=0)
    {
        errno = app_timer_start((app_timer_id_t)timer_id, APP_TIMER_TICKS(timeout_value_ms), NULL);
        
        return nrf_err_code_convert(errno);
    }
    else
    {
        return TUYA_BLE_ERR_NOT_FOUND;
    }
}

tuya_ble_status_t tuya_ble_timer_restart(void* timer_id,uint32_t timeout_value_ms)
{
    uint32_t errno;
    uint32_t temp;
    
    if(find_timer_timeout_value(timer_id,&temp)>=0)
    {
        errno = app_timer_stop((app_timer_id_t)timer_id);
        
        errno = app_timer_start((app_timer_id_t)timer_id, APP_TIMER_TICKS(timeout_value_ms), NULL);
        
        return nrf_err_code_convert(errno);
    }
    else
    {
        return TUYA_BLE_ERR_NOT_FOUND;
    }
}


tuya_ble_status_t tuya_ble_timer_stop(void* timer_id)
{
    uint32_t errno;
    errno = app_timer_stop((app_timer_id_t)timer_id);
    return nrf_err_code_convert(errno); 
}


void tuya_ble_device_delay_ms(uint32_t ms)
{
    nrf_delay_ms(ms);
}


tuya_ble_status_t tuya_ble_rand_generator(uint8_t* p_buf, uint8_t len)
{
    uint32_t cnt=len/4;
    uint8_t  remain = len%4;
    int32_t temp;
    for(uint32_t i=0; i<cnt; i++)
    {
        temp = rand();
        memcpy(p_buf,(uint8_t *)&temp,4);
        p_buf += 4;
    }
    temp = rand();
    memcpy(p_buf,(uint8_t *)&temp,remain);

    return TUYA_BLE_SUCCESS;
}

/*
 *@brief
 *@param
 *
 *@note
 *
 * */
tuya_ble_status_t tuya_ble_device_reset(void)
{
    NVIC_SystemReset();
    return TUYA_BLE_SUCCESS;
}

static uint8_t __CR_NESTED = 0;    

void tuya_ble_device_enter_critical(void)
{
   __CR_NESTED = 0;                                                           
   app_util_critical_region_enter(&__CR_NESTED);
}

void tuya_ble_device_exit_critical(void)
{
   app_util_critical_region_exit(__CR_NESTED); 
}


tuya_ble_status_t tuya_ble_rtc_get_timestamp(uint32_t *timestamp,int32_t *timezone)
{
    *timestamp = 0;
    *timezone = 0;
    return TUYA_BLE_SUCCESS;
}

tuya_ble_status_t tuya_ble_rtc_set_timestamp(uint32_t timestamp,int32_t timezone)
{

    return TUYA_BLE_SUCCESS;
}


tuya_ble_status_t tuya_ble_nv_init(void)
{    
    nrf_app_flash_init(true);
    return TUYA_BLE_SUCCESS;
}

tuya_ble_status_t tuya_ble_nv_erase(uint32_t addr,uint32_t size)
{    
    tuya_ble_status_t result = TUYA_BLE_SUCCESS;

    uint32_t erase_pages;
    
    /* make sure the start address is a multiple of FLASH_ERASE_MIN_SIZE */
    if(addr % TUYA_NV_ERASE_MIN_SIZE != 0)
    {
        log_d("the start address is a not multiple of TUYA_NV_ERASE_MIN_SIZE");
        return TUYA_BLE_ERR_INVALID_ADDR;
    }
    
    /* calculate pages */
    erase_pages = size / TUYA_NV_ERASE_MIN_SIZE;
    if (size % TUYA_NV_ERASE_MIN_SIZE != 0) {
        erase_pages++;
    }

    result = nrf_err_code_convert(nrf_app_flash_erase(addr,erase_pages,NULL));
   
    return result;
}

tuya_ble_status_t tuya_ble_nv_write(uint32_t addr,const uint8_t *p_data, uint32_t size)
{  
    tuya_ble_status_t result = TUYA_BLE_SUCCESS;
    result = nrf_err_code_convert(nrf_app_flash_store(addr,p_data,size,NULL));
    return TUYA_BLE_SUCCESS;
}

tuya_ble_status_t tuya_ble_nv_read(uint32_t addr,uint8_t *p_data, uint32_t size)
{
    nrf_app_flash_read(addr,p_data,size);
    return TUYA_BLE_SUCCESS;

}


#if TUYA_BLE_USE_OS

bool tuya_ble_os_task_create(void **pp_handle, const char *p_name, void (*p_routine)(void *),void *p_param, uint16_t stack_size, uint16_t priority)
{
    return os_task_create(pp_handle, p_name, p_routine,p_param, stack_size, priority);
}

bool tuya_ble_os_task_delete(void *p_handle)
{
    return os_task_delete(p_handle);
}

bool tuya_ble_os_task_suspend(void *p_handle)
{
    return os_task_suspend(p_handle);
}

bool tuya_ble_os_task_resume(void *p_handle)
{
    return os_task_resume(p_handle);
}

bool tuya_ble_os_msg_queue_create(void **pp_handle, uint32_t msg_num, uint32_t msg_size)
{
    return os_msg_queue_create(pp_handle, msg_num, msg_size);
}

bool tuya_ble_os_msg_queue_delete(void *p_handle)
{
    return os_msg_queue_delete(p_handle);
}

bool tuya_ble_os_msg_queue_peek(void *p_handle, uint32_t *p_msg_num)
{
    return os_msg_queue_peek(p_handle, p_msg_num);
}

bool tuya_ble_os_msg_queue_send(void *p_handle, void *p_msg, uint32_t wait_ms)
{
    return os_msg_send(p_handle, p_msg, wait_ms);
}

bool tuya_ble_os_msg_queue_recv(void *p_handle, void *p_msg, uint32_t wait_ms)
{
    return os_msg_recv(p_handle, p_msg, wait_ms);
}

#endif


bool tuya_ble_aes128_ecb_encrypt(uint8_t *key,uint8_t *input,uint16_t input_len,uint8_t *output)
{
    uint16_t length;
    mbedtls_aes_context aes_ctx;
    //
    if(input_len%16)
    {
        return false;
    }

    length = input_len;

    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_enc(&aes_ctx, key, 128);

    while( length > 0 )
    {
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, input, output );
        input  += 16;
        output += 16;
        length -= 16;
    }

    mbedtls_aes_free(&aes_ctx);

    return true;
}

bool tuya_ble_aes128_ecb_decrypt(uint8_t *key,uint8_t *input,uint16_t input_len,uint8_t *output)
{
    uint16_t length;
    mbedtls_aes_context aes_ctx;
    //
    if(input_len%16)
    {
        return false;
    }

    length = input_len;

    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_dec(&aes_ctx, key, 128);

    while( length > 0 )
    {
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, input, output );
        input  += 16;
        output += 16;
        length -= 16;
    }

    mbedtls_aes_free(&aes_ctx);

    return true;
}

bool tuya_ble_aes128_cbc_encrypt(uint8_t *key,uint8_t *iv,uint8_t *input,uint16_t input_len,uint8_t *output)
{
    mbedtls_aes_context aes_ctx;
    //
    if(input_len%16)
    {
        return false;
    }

    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_enc(&aes_ctx, key, 128);
    
    mbedtls_aes_crypt_cbc(&aes_ctx,MBEDTLS_AES_ENCRYPT,input_len,iv,input,output);
    //
    mbedtls_aes_free(&aes_ctx);

    return true;
}

bool tuya_ble_aes128_cbc_decrypt(uint8_t *key,uint8_t *iv,uint8_t *input,uint16_t input_len,uint8_t *output)
{
    mbedtls_aes_context aes_ctx;
    //
    if(input_len%16)
    {
        return false;
    }

    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_dec(&aes_ctx, key, 128);
    
    mbedtls_aes_crypt_cbc(&aes_ctx,MBEDTLS_AES_DECRYPT,input_len,iv,input,output);
    //
    mbedtls_aes_free(&aes_ctx);

    return true;
}


bool tuya_ble_md5_crypt(uint8_t *input,uint16_t input_len,uint8_t *output)
{
    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);
    mbedtls_md5_update(&md5_ctx, input, input_len);
    mbedtls_md5_finish(&md5_ctx, output);
    mbedtls_md5_free(&md5_ctx);    
    
    return true;
}

bool tuya_ble_hmac_sha1_crypt(const uint8_t *key, uint32_t key_len, const uint8_t *input, uint32_t input_len, uint8_t *output)
{    
    //hmac_sha1_crypt(key, key_len, input, input_len, output);
	return true;
}

bool tuya_ble_hmac_sha256_crypt(const uint8_t *key, uint32_t key_len, const uint8_t *input, uint32_t input_len, uint8_t *output)
{
   // hmac_sha256_crypt(key, key_len, input, input_len, output);
	return true;
}

