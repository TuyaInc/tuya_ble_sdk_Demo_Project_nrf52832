
#ifndef FLASH_H__
#define FLASH_H__

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"


#ifdef __cplusplus
extern "C" {
#endif



/**@brief Function for initializing the flash module.
 *
 * Depending on whether or not the SoftDevice is present and its IRQ have been initialized,
 * this function initializes the correct @ref nrf_fstorage backend.
 *
 * @param[in]   sd_irq_initialized  Whether or not the SoftDevice IRQ have been initialized.
 *
 * @retval NRF_SUCCESS  If the operation was successful.
  */
ret_code_t nrf_app_flash_init(bool sd_irq_initialized);


/**@brief Function for storing data to flash.
 *
 * This functions is asynchronous when the SoftDevice is enabled and synchronous when
 * the SoftDevice is not present or disabled. In both cases, if a callback function is provided,
 * it will be called when the operation has completed.
 *
 * @note The content of @p p_src should be kept in memory until the operation has completed.
 *
 * @param[in]  dest      The address where the data should be stored.
 * @param[in]  p_src     Pointer to the address where the data should be copied from.
 *                       This address can be in flash or RAM.
 * @param[in]  len       The number of bytes to be copied from @p p_src to @p dest.
 * @param[in]  p_param user-defined parameter passed to the event handler.
 *
 * @retval  NRF_SUCCESS                 If the operation was successful.
 * @retval  NRF_ERROR_INVALID_STATE     If nrf_dfu_flash is not initialized.
 * @retval  NRF_ERROR_INVALID_ADDR      If @p p_src or @p dest is not word-aligned.
 * @retval  NRF_ERROR_INVALID_LENGTH    If @p len is zero.
 * @retval  NRF_ERROR_NULL              If @p p_src is NULL.
 * @retval  NRF_ERROR_NO_MEM            If nrf_fstorage is out of memory.
 */
ret_code_t nrf_app_flash_store(uint32_t                     dest,
                               void                 const * p_src,
                               uint32_t                     len,
                               void *   p_param);


/**@brief Function for erasing data from flash.
 *
 * This functions is asynchronous when the SoftDevice is enabled and synchronous when
 * the SoftDevice is not present or disabled. In both cases, if a callback function is provided,
 * it will be called when the operation has completed.
 *
 * @param[in]  page_addr    The address of the first flash page to be deleted.
 * @param[in]  num_pages    The number of flash pages to be deleted.
 * @param[in]  p_param user-defined parameter passed to the event handler.
 *
 * @retval  NRF_SUCCESS                 If the operation was successful.
 * @retval  NRF_ERROR_INVALID_STATE     If nrf_dfu_flash is not initialized.
 * @retval  NRF_ERROR_INVALID_ADDR      If @p page_addr is not aligned to a page boundary or the
 *                                      operation would go beyond the flash memory boundaries.
 * @retval  NRF_ERROR_INVALID_LENGTH    If @p num_pages is zero.
 * @retval  NRF_ERROR_NULL              If @p page_addr is NULL.
 * @retval  NRF_ERROR_NO_MEM            If the queue of nrf_fstorage is full.
 */
ret_code_t nrf_app_flash_erase(uint32_t page_addr, uint32_t num_pages, void *   p_param);



ret_code_t nrf_app_flash_read(uint32_t dest,void * p_dest,uint32_t len);



#ifdef __cplusplus
}
#endif


#endif // FLASH_H__
/** @} */
