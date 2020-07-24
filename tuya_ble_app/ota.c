
#include "nrf_balloc.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "nrf_dfu_types.h"
#include "nrf_dfu_settings.h"
#include "crc32.h"
#include "nrf_fstorage.h"
#include "ota.h"
#include "tuya_ble_app_demo.h"
#include "tuya_ble_type.h"
#include "tuya_ble_data_handler.h"
#include "tuya_ble_api.h"
#include "tuya_ble_log.h"

static volatile tuya_ota_status_t tuya_ota_status;

ANON_UNIONS_ENABLE;
typedef union
{
    struct
    {
        uint32_t firmware_file_version;
        uint32_t firmware_file_length;
        uint32_t firmware_file_crc;
        uint8_t  firmware_file_md5[16];
    };
    uint8_t init_command[INIT_COMMAND_MAX_SIZE];

} ota_firmware_file_info_t;
ANON_UNIONS_DISABLE;

ota_firmware_file_info_t *current_firmware_file_info;

#define MAX_DFU_DATA_LEN  512

#define MAX_DFU_BUFFERS   ((CODE_PAGE_SIZE / MAX_DFU_DATA_LEN) + 1)

NRF_BALLOC_DEF(m_buffer_pool, MAX_DFU_DATA_LEN, MAX_DFU_BUFFERS);

static uint32_t m_firmware_start_addr;          /**< Start address of the current firmware image. */
static uint32_t m_firmware_size_req;


static uint16_t current_package = 0;
static uint16_t last_package = 0;


static void tuya_ota_start_req(uint8_t*recv_data,uint32_t recv_len)
{
    uint8_t p_buf[70];
    uint8_t payload_len = 0;
    uint8_t encry_mode = 0;
    uint32_t current_version = TY_APP_VER_NUM;
    tuya_ble_ota_response_t res_data;

    if(tuya_ota_status!=TUYA_OTA_STATUS_NONE)
    {
        TUYA_APP_LOG_ERROR("current ota status is not TUYA_OTA_STATUS_NONE  and is : %d !",tuya_ota_status);
        return;
    }

    p_buf[0] = 0;
    p_buf[1] = TUYA_OTA_VERSION;
    p_buf[2] = TUYA_OTA_TYPE;
    p_buf[3] = current_version>>24;
    p_buf[4] = current_version>>16;
    p_buf[5] = current_version>>8;
    p_buf[6] = current_version;
    p_buf[7] = MAX_DFU_DATA_LEN>>8;
    p_buf[8] = MAX_DFU_DATA_LEN;
    tuya_ota_status = TUYA_OTA_STATUS_START;
    payload_len = 9;

    res_data.type =  TUYA_BLE_OTA_REQ;
    res_data.data_len = payload_len;
    res_data.p_data = p_buf;

    if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
    {
        TUYA_APP_LOG_ERROR("tuya_ota_start_response failed.");
    }

}


static uint8_t file_crc_check_in_flash(uint32_t len,uint32_t *crc)
{

    static uint8_t buf[256];
    if(len == 0)
    {
        return 1;
    }
    uint32_t crc_temp = 0;
    uint32_t read_addr = APP_NEW_FW_START_ADR;
    uint32_t cnt = len/256;
    uint32_t remainder = len % 256;
    for(uint32_t i = 0; i<cnt; i++)
    {
        memcpy(buf,(uint32_t *)read_addr,256);
        crc_temp = crc32_compute(buf, 256, &crc_temp);
        read_addr += 256;
    }

    if(remainder>0)
    {
        memcpy(buf,(uint32_t *)read_addr,remainder);
        crc_temp = crc32_compute(buf, remainder, &crc_temp);
        read_addr += remainder;
    }

    *crc = crc_temp;

    return 0;
}




static void tuya_ota_file_info_req(uint8_t*recv_data,uint32_t recv_len)
{
    uint8_t p_buf[30];
    uint8_t payload_len = 0;
    uint8_t encry_mode = 0;
    uint32_t file_version;
    uint32_t file_length;
    uint32_t file_crc;
    bool file_md5;
    uint8_t state;
    tuya_ble_ota_response_t res_data;


    if(tuya_ota_status!=TUYA_OTA_STATUS_START)
    {
        TUYA_APP_LOG_ERROR("current ota status is not TUYA_OTA_STATUS_START  and is : %d !",tuya_ota_status);
        return;
    }

    if(recv_data[0]!=TUYA_OTA_TYPE)
    {
        TUYA_APP_LOG_ERROR("current ota fm type is not 0!");
        return;
    }

    file_version = recv_data[9]<<24;
    file_version += recv_data[10]<<16;
    file_version += recv_data[11]<<8;
    file_version += recv_data[12];

    if(memcmp(current_firmware_file_info->firmware_file_md5,&recv_data[17+TUYA_BLE_PRODUCT_ID_DEFAULT_LEN],16)==0)
    {
        file_md5 = true;
    }
    else
    {
        file_md5 = false;
    }

    file_length = recv_data[29]<<24;
    file_length += recv_data[30]<<16;
    file_length += recv_data[31]<<8;
    file_length += recv_data[32];

    file_crc = recv_data[33]<<24;
    file_crc += recv_data[34]<<16;
    file_crc += recv_data[35]<<8;
    file_crc += recv_data[36];

    if((file_version > TY_APP_VER_NUM)&&(file_length <= APP_NEW_FW_MAX_SIZE))
    {

        if(file_md5&&(current_firmware_file_info->firmware_file_version==file_version)&&(current_firmware_file_info->firmware_file_length==file_length)
                &&(current_firmware_file_info->firmware_file_crc==file_crc))
        {
            state = 0;
        }
        else
        {
            memset(&s_dfu_settings.progress, 0, sizeof(dfu_progress_t));
            s_dfu_settings.progress.firmware_image_crc_last = 0;
            current_firmware_file_info->firmware_file_version = file_version;
            current_firmware_file_info->firmware_file_length = file_length;
            current_firmware_file_info->firmware_file_crc = file_crc;
            memcpy(current_firmware_file_info->firmware_file_md5,&recv_data[17+TUYA_BLE_PRODUCT_ID_DEFAULT_LEN],16);
            s_dfu_settings.write_offset = s_dfu_settings.progress.firmware_image_offset_last;
            state = 0;
            nrf_dfu_settings_write_and_backup(NULL);
        }

        m_firmware_start_addr = APP_NEW_FW_START_ADR;
        m_firmware_size_req = current_firmware_file_info->firmware_file_length;

    }
    else
    {
        if(file_version <= TY_APP_VER_NUM)
        {
            TUYA_APP_LOG_ERROR("ota file version error !");
            state = 2;
        }
        else
        {
            TUYA_APP_LOG_ERROR("ota file length is bigger than rev space !");
            state = 3;
        }
    }



    memset(p_buf,0,sizeof(p_buf));
    p_buf[0] = TUYA_OTA_TYPE;
    p_buf[1] = state;
    if(state==0)
    {
        uint32_t crc_temp  = 0;
        if(file_crc_check_in_flash(s_dfu_settings.progress.firmware_image_offset_last,&crc_temp)==0)
        {
            if(crc_temp != s_dfu_settings.progress.firmware_image_crc_last)
            {
                s_dfu_settings.progress.firmware_image_offset_last = 0;
                s_dfu_settings.progress.firmware_image_crc_last = 0;
                s_dfu_settings.write_offset = s_dfu_settings.progress.firmware_image_offset_last;
                nrf_dfu_settings_write_and_backup(NULL);
            }
        }

        p_buf[2] = s_dfu_settings.progress.firmware_image_offset_last>>24;
        p_buf[3] = s_dfu_settings.progress.firmware_image_offset_last>>16;
        p_buf[4] = s_dfu_settings.progress.firmware_image_offset_last>>8;
        p_buf[5] = (uint8_t)s_dfu_settings.progress.firmware_image_offset_last;
        p_buf[6] = s_dfu_settings.progress.firmware_image_crc_last>>24;
        p_buf[7] = s_dfu_settings.progress.firmware_image_crc_last>>16;
        p_buf[8] = s_dfu_settings.progress.firmware_image_crc_last>>8;
        p_buf[9] = (uint8_t)s_dfu_settings.progress.firmware_image_crc_last;
        tuya_ota_status = TUYA_OTA_STATUS_FILE_INFO;
        current_package = 0;
        last_package = 0;

        TUYA_APP_LOG_DEBUG("ota file length  : 0x%04x",current_firmware_file_info->firmware_file_length);
        TUYA_APP_LOG_DEBUG("ota file  crc    : 0x%04x",current_firmware_file_info->firmware_file_crc);
        TUYA_APP_LOG_DEBUG("ota file version : 0x%04x",current_firmware_file_info->firmware_file_version);
        TUYA_APP_LOG_DEBUG("ota firmware_image_offset_last : 0x%04x",s_dfu_settings.progress.firmware_image_offset_last);
        TUYA_APP_LOG_DEBUG("ota firmware_image_crc_last    : 0x%04x",s_dfu_settings.progress.firmware_image_crc_last);
        TUYA_APP_LOG_DEBUG("ota firmware   write offset    : 0x%04x",s_dfu_settings.write_offset);

    }
    payload_len = 26;

    res_data.type =  TUYA_BLE_OTA_FILE_INFO;
    res_data.data_len = payload_len;
    res_data.p_data = p_buf;

    if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
    {
        TUYA_APP_LOG_ERROR("tuya_ota_file_info_response failed.");
    }

}


static void tuya_ota_offset_req(uint8_t*recv_data,uint32_t recv_len)
{
    uint8_t p_buf[5];
    uint8_t payload_len = 0;
    uint8_t encry_mode = 0;
    uint32_t offset;
    tuya_ble_ota_response_t res_data;

    if(tuya_ota_status!=TUYA_OTA_STATUS_FILE_INFO)
    {
        TUYA_APP_LOG_ERROR("current ota status is not TUYA_OTA_STATUS_FILE_INFO  and is : %d !",tuya_ota_status);
        return;
    }

    offset  = recv_data[1]<<24;
    offset += recv_data[2]<<16;
    offset += recv_data[3]<<8;
    offset += recv_data[4];

    if((offset==0)&&(s_dfu_settings.progress.firmware_image_offset_last!=0))
    {
        s_dfu_settings.progress.firmware_image_crc_last = 0;
        s_dfu_settings.progress.firmware_image_offset_last = 0;
        s_dfu_settings.write_offset = s_dfu_settings.progress.firmware_image_offset_last;
        nrf_dfu_settings_write_and_backup(NULL);
    }

    p_buf[0] = TUYA_OTA_TYPE;
    p_buf[1] = s_dfu_settings.progress.firmware_image_offset_last>>24;
    p_buf[2] = s_dfu_settings.progress.firmware_image_offset_last>>16;
    p_buf[3] = s_dfu_settings.progress.firmware_image_offset_last>>8;
    p_buf[4] = (uint8_t)s_dfu_settings.progress.firmware_image_offset_last;

    tuya_ota_status = TUYA_OTA_STATUS_FILE_OFFSET;

    payload_len = 5;

    res_data.type =  TUYA_BLE_OTA_FILE_OFFSET_REQ;
    res_data.data_len = payload_len;
    res_data.p_data = p_buf;

    if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
    {
        TUYA_APP_LOG_ERROR("tuya_ota_offset_response failed.");
    }

}


static void on_flash_write(void * p_buf)
{
    TUYA_APP_LOG_DEBUG("Freeing buffer %p", p_buf);
    nrf_balloc_free(&m_buffer_pool, p_buf);
}


static void tuya_ota_data_req(uint8_t*recv_data,uint32_t recv_len)
{
    uint8_t p_buf[2];
    uint8_t payload_len = 0;
    uint8_t state = 0;
    uint16_t len;
    uint8_t * p_balloc_buf;
    tuya_ble_ota_response_t res_data;


    if((tuya_ota_status!=TUYA_OTA_STATUS_FILE_OFFSET)&&(tuya_ota_status!=TUYA_OTA_STATUS_FILE_DATA))
    {
        TUYA_APP_LOG_ERROR("current ota status is not TUYA_OTA_STATUS_FILE_OFFSET  or TUYA_OTA_STATUS_FILE_DATA and is : %d !",tuya_ota_status);
        return;
    }

    state = 0;

    current_package = (recv_data[1]<<8)|recv_data[2];
    len = (recv_data[3]<<8)|recv_data[4];

    if((current_package!=(last_package+1))&&(current_package!=0))
    {
        TUYA_APP_LOG_ERROR("ota received package number error.received package number : %d",current_package);
        state = 1;
    }
    else  if(len>MAX_DFU_DATA_LEN)
    {
        TUYA_APP_LOG_ERROR("ota received package data length error : %d",len);
        state = 5;
    }
    else
    {
        uint32_t  write_addr = APP_NEW_FW_START_ADR +  s_dfu_settings.write_offset;//current_package*MAX_DFU_DATA_LEN;
        if(write_addr>=APP_NEW_FW_END_ADR)
        {
            TUYA_APP_LOG_ERROR("ota write addr error.");
            state = 1;
        }

        if(write_addr%CODE_PAGE_SIZE==0)
        {
            if (nrf_dfu_flash_erase(write_addr,1, NULL) != NRF_SUCCESS)
            {
                TUYA_APP_LOG_ERROR("ota Erase page operation failed");
                state = 4;
            }
        }

        if(state==0)
        {
            /* Allocate a buffer to receive data. */
            p_balloc_buf = nrf_balloc_alloc(&m_buffer_pool);
            if (p_balloc_buf == NULL)
            {
                /* Operations are retried by the host; do not give up here. */
                TUYA_APP_LOG_ERROR("cannot allocate memory buffer!");
                state = 4;
            }
            else
            {
                len = (recv_data[3]<<8)|recv_data[4];

                memcpy(p_balloc_buf, &recv_data[7], len);

                ret_code_t ret = NRF_SUCCESS;
                //on_flash_write((void*)p_balloc_buf);// free buffer
                ret = nrf_dfu_flash_store(write_addr, p_balloc_buf, len, on_flash_write);

                if (ret != NRF_SUCCESS)
                {
                    on_flash_write((void*)p_balloc_buf);// free buffer
                    state = 4;
                }
                else
                {
                    s_dfu_settings.progress.firmware_image_crc_last = crc32_compute(p_balloc_buf, len, &s_dfu_settings.progress.firmware_image_crc_last);
                    s_dfu_settings.write_offset    += len;
                    s_dfu_settings.progress.firmware_image_offset_last += len;

                    if((current_package+1)%32==0)
                    {
                        nrf_dfu_settings_write_and_backup(NULL); //由于flash异步存储，此处操作有风险，会出现setting数据和实际存储的固件数据不一致，可在偏移量请求中增加实际校验规避�
                    }


                }

            }

        }

    }

    p_buf[0] = TUYA_OTA_TYPE;
    p_buf[1] = state;

    tuya_ota_status = TUYA_OTA_STATUS_FILE_DATA;

    payload_len = 2;

    res_data.type =  TUYA_BLE_OTA_DATA;
    res_data.data_len = payload_len;
    res_data.p_data = p_buf;

    if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
    {
        TUYA_APP_LOG_ERROR("tuya_ota_data_response failed.");
    }

    if(state!=0)//出错，恢复初始状
    {
        TUYA_APP_LOG_ERROR("ota error so free!");
        tuya_ota_status = TUYA_OTA_STATUS_NONE;
        tuya_ota_init_disconnect();
        memset(&s_dfu_settings, 0, sizeof(nrf_dfu_settings_t));
        nrf_dfu_settings_write_and_backup(NULL);
    }
    else
    {
        last_package = current_package;
    }


}


static void reset_after_flash_write(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    tuya_ble_gap_disconnect();
    //tuya_bsp_delay_ms(1000);
    TUYA_APP_LOG_INFO("start reset~~~.");
    NVIC_SystemReset();
}


static void on_dfu_complete(nrf_fstorage_evt_t * p_evt)
{
    UNUSED_PARAMETER(p_evt);

    TUYA_APP_LOG_INFO("All flash operations have completed. DFU completed.");

    reset_after_flash_write(NULL);
}



static void on_data_write_request_sched(void)
{
    ret_code_t          ret;
    tuya_ble_custom_evt_t event;
    uint8_t p_buf[2];
    uint8_t payload_len = 0;
    uint8_t state;
    tuya_ble_ota_response_t res_data;

    event.custom_event_handler = (void *)on_data_write_request_sched;


    /* Wait for all buffers to be written in flash. */
    if (nrf_fstorage_is_busy(NULL))
    {
        if(tuya_ble_custom_event_send(event)!=0)
        {
            TUYA_APP_LOG_ERROR("Failed to send custom evt");

            p_buf[0] = TUYA_OTA_TYPE;
            p_buf[1] = 3;

            tuya_ota_status = TUYA_OTA_STATUS_NONE;

            payload_len = 2;

            res_data.type =  TUYA_BLE_OTA_END;
            res_data.data_len = payload_len;
            res_data.p_data = p_buf;

            if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
            {
                TUYA_APP_LOG_ERROR("tuya_ota_end_response failed.");
            }

        }
        return;
    }

    if (s_dfu_settings.progress.firmware_image_offset_last == m_firmware_size_req)
    {
        TUYA_APP_LOG_DEBUG("Whole firmware image received. Postvalidating.");

        uint32_t crc_temp  = 0;
        if(file_crc_check_in_flash(s_dfu_settings.progress.firmware_image_offset_last,&crc_temp)==0)
        {
            if(s_dfu_settings.progress.firmware_image_crc_last != crc_temp)
            {
                TUYA_APP_LOG_WARNING("file crc check in flash diff from crc_last. crc_temp = 0x%04x,crc_last = 0x%04x",crc_temp,s_dfu_settings.progress.firmware_image_crc_last);
                s_dfu_settings.progress.firmware_image_crc_last = crc_temp;
            }

        }


        if(s_dfu_settings.progress.firmware_image_crc_last!=current_firmware_file_info->firmware_file_crc)
        {
            TUYA_APP_LOG_ERROR("ota file crc check error,last_crc = 0x%04x ,file_crc = 0x%04x",s_dfu_settings.progress.firmware_image_crc_last,current_firmware_file_info->firmware_file_crc);
            state = 2;
        }
        else
        {
            s_dfu_settings.bank_1.image_crc = s_dfu_settings.progress.firmware_image_crc_last;
            s_dfu_settings.bank_1.image_size = m_firmware_size_req;
            s_dfu_settings.bank_1.bank_code = NRF_DFU_BANK_VALID_APP;

            memset(&s_dfu_settings.progress, 0, sizeof(dfu_progress_t));

            s_dfu_settings.write_offset                  = 0;
            s_dfu_settings.progress.update_start_address = APP_NEW_FW_START_ADR;

            state = 0;


        }


    }
    else
    {

        state = 1;
    }

    p_buf[0] = TUYA_OTA_TYPE;
    p_buf[1] = state;
    tuya_ota_status = TUYA_OTA_STATUS_NONE;
    payload_len = 2;
    res_data.type =  TUYA_BLE_OTA_END;
    res_data.data_len = payload_len;
    res_data.p_data = p_buf;

    if(tuya_ble_ota_response(&res_data) != TUYA_BLE_SUCCESS)
    {
        TUYA_APP_LOG_ERROR("tuya_ota_end_response failed.");
    }

    if(state==0)
    {
        ret = nrf_dfu_settings_write_and_backup((nrf_dfu_flash_callback_t)on_dfu_complete);
        UNUSED_RETURN_VALUE(ret);
    }
    else
    {
        TUYA_APP_LOG_ERROR("ota crc error!");
        tuya_ota_status = TUYA_OTA_STATUS_NONE;
        tuya_ota_init_disconnect();
        memset(&s_dfu_settings, 0, sizeof(nrf_dfu_settings_t));
        nrf_dfu_settings_write_and_backup(NULL);
    }

}



static void tuya_ota_end_req(uint8_t*recv_data,uint32_t recv_len)
{
    uint8_t p_buf[2];
    uint8_t payload_len = 0;
    uint8_t encry_mode = 0;
    

    if(tuya_ota_status==TUYA_OTA_STATUS_NONE)
    {
        TUYA_APP_LOG_ERROR("current ota status is TUYA_OTA_STATUS_NONE!");
        return;
    }

    on_data_write_request_sched();

}


void tuya_ota_proc(uint16_t cmd,uint8_t*recv_data,uint32_t recv_len)
{
    TUYA_APP_LOG_DEBUG("ota cmd : 0x%04x , recv_len : %d",cmd,recv_len);
    switch(cmd)
    {
    case TUYA_BLE_OTA_REQ:
        tuya_ota_start_req(recv_data,recv_len);
        break;
    case TUYA_BLE_OTA_FILE_INFO:
        tuya_ota_file_info_req(recv_data,recv_len);
        break;
    case TUYA_BLE_OTA_FILE_OFFSET_REQ:
        tuya_ota_offset_req(recv_data,recv_len);
        break;
    case TUYA_BLE_OTA_DATA:
        tuya_ota_data_req(recv_data,recv_len);
        break;
    case TUYA_BLE_OTA_END:
        tuya_ota_end_req(recv_data,recv_len);
        break;
    default:
        break;
    }

}

void tuya_ota_status_set(tuya_ota_status_t status)
{
    tuya_ota_status = status;
}


tuya_ota_status_t tuya_ota_status_get(void)
{
    return tuya_ota_status;
}

uint8_t tuya_ota_init_disconnect(void)
{
    if(tuya_ota_status != TUYA_OTA_STATUS_NONE)
    {
        nrf_dfu_settings_write_and_backup(NULL);
        tuya_ota_status = TUYA_OTA_STATUS_NONE;
    }
    current_package = 0;
    last_package = 0;
}

uint32_t tuya_ota_init(void)
{
    ret_code_t      ret_val;
    tuya_ota_status = TUYA_OTA_STATUS_NONE;

    current_package = 0;
    last_package = 0;

    ret_val = nrf_balloc_init(&m_buffer_pool);
    UNUSED_RETURN_VALUE(ret_val);

    current_firmware_file_info = (ota_firmware_file_info_t *)(&s_dfu_settings.init_command);

    ret_val = nrf_dfu_settings_init(true);
    if (ret_val != NRF_SUCCESS)
    {
        return NRF_ERROR_INTERNAL;
    }

    return 0;
}


