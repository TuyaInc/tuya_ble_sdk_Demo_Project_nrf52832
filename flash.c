
#include "flash.h"
#include "nrf_dfu_types.h"

#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"
#include "nrf_fstorage_nvmc.h"

#include "ota.h"
#include "tuya_ble_port.h"

#define NRF_LOG_MODULE_NAME nrf_app_flash
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();



#define APP_FLASH_SETTING_BASE_ADDR     APP_NEW_FW_END_ADR

#define APP_FLASH_APP_DATA_END_ADDR     0x78000


void app_fstorage_evt_handler(nrf_fstorage_evt_t * p_evt);


NRF_FSTORAGE_DEF(nrf_fstorage_t app_fs) =
{
    .evt_handler = app_fstorage_evt_handler,
    .start_addr  = APP_FLASH_SETTING_BASE_ADDR,
    .end_addr    = APP_FLASH_APP_DATA_END_ADDR,
};

static uint32_t m_flash_operations_pending;

void app_fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
{
    
    tuya_ble_status_t result = nrf_err_code_convert(p_evt->result);
    
    if (NRF_LOG_ENABLED && (m_flash_operations_pending > 0))
    {
        m_flash_operations_pending--;
    }

    if (p_evt->result == NRF_SUCCESS)
    {
        NRF_LOG_DEBUG("Flash %s success: addr=%p, pending %d",
                      (p_evt->id == NRF_FSTORAGE_EVT_WRITE_RESULT) ? "write" : "erase",
                      p_evt->addr, m_flash_operations_pending);
    }
    else
    {
        NRF_LOG_DEBUG("Flash %s failed (0x%x): addr=%p, len=0x%x bytes, pending %d",
                      (p_evt->id == NRF_FSTORAGE_EVT_WRITE_RESULT) ? "write" : "erase",
                      p_evt->result, p_evt->addr, p_evt->len, m_flash_operations_pending);
    }

    if (p_evt->p_param)
    {

    }
}


ret_code_t nrf_app_flash_init(bool sd_irq_initialized)
{
    nrf_fstorage_api_t * p_api_impl;

    /* Setup the desired API implementation. */
#ifdef BLE_STACK_SUPPORT_REQD
    if (sd_irq_initialized)
    {
        NRF_LOG_DEBUG("Initializing nrf_fstorage_sd backend.");
        p_api_impl = &nrf_fstorage_sd;
    }
    else
#endif
    {
        NRF_LOG_DEBUG("Initializing nrf_fstorage_nvmc backend.");
        p_api_impl = &nrf_fstorage_nvmc;
    }

    return nrf_fstorage_init(&app_fs, p_api_impl, NULL);
}


ret_code_t nrf_app_flash_store(uint32_t                   dest,
                               void               const * p_src,
                               uint32_t                   len,
                               void *   p_param)
{
    ret_code_t rc;

    NRF_LOG_DEBUG("nrf_fstorage_write(addr=%p, src=%p, len=%d bytes), queue usage: %d",
                  dest, p_src, len, m_flash_operations_pending);

    //lint -save -e611 (Suspicious cast)
    rc = nrf_fstorage_write(&app_fs, dest, p_src, len, (void *)p_param);
    //lint -restore

    if ((NRF_LOG_ENABLED) && (rc == NRF_SUCCESS))
    {
        m_flash_operations_pending++;
    }
    else
    {
        NRF_LOG_WARNING("nrf_fstorage_write() failed with error 0x%x.", rc);
    }

    return rc;
}


ret_code_t nrf_app_flash_erase(uint32_t                 page_addr,
                               uint32_t                 num_pages,
                               void *   p_param)
{
    ret_code_t rc;

    NRF_LOG_DEBUG("nrf_fstorage_erase(addr=0x%p, len=%d pages), queue usage: %d",
                  page_addr, num_pages, m_flash_operations_pending);

    //lint -save -e611 (Suspicious cast)
    rc = nrf_fstorage_erase(&app_fs, page_addr, num_pages, (void *)p_param);
    //lint -restore

    if ((NRF_LOG_ENABLED) && (rc == NRF_SUCCESS))
    {
        m_flash_operations_pending++;
    }
    else
    {
        NRF_LOG_WARNING("nrf_fstorage_erase() failed with error 0x%x.", rc);
    }

    return rc;
}


ret_code_t nrf_app_flash_read(uint32_t                   dest,
                               void                    * p_dest,
                               uint32_t                   len)
{
    ret_code_t rc;

    NRF_LOG_DEBUG("nrf_fstorage_read(addr=%p, dest=%p, len=%d bytes), queue usage: %d",
                  dest, p_dest, len, m_flash_operations_pending);

    //lint -save -e611 (Suspicious cast)
    rc = nrf_fstorage_read(&app_fs, dest, p_dest, len);
    //lint -restore
    
    return rc;
}

