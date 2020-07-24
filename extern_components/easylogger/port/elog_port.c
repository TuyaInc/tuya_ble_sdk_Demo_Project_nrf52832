/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */
 
#include "../inc/elog.h"
#include "SEGGER_RTT.h"
#include "app_util_platform.h"

#define  ENTER_CRITICAL()    CRITICAL_REGION_ENTER()//tuya_bsp_enter_critical()//vTaskSuspendAll();
#define  EXIT_CRITICAL()     CRITICAL_REGION_EXIT()//tuya_bsp_exit_critical()//xTaskResumeAll();

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    return result;
}



/**
 * output lock
 */
void elog_port_output_lock(void) {
    //__disable_irq();
//	ENTER_CRITICAL();
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    //__enable_irq();
//	EXIT_CRITICAL();
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    /* output to terminal */
	SEGGER_RTT_Write(0,log,size);
    //TODO output to flash
}
/**
 * get current time interface
 *
 * @return current time
 */

const char *elog_port_get_time(void) 
{
	static char time_buf[8] = {0};
//	extern unsigned int xTaskGetTickCount( void );
//	unsigned int ticks=xTaskGetTickCount();
//	
//    printf_output(time_buf,8,"%08s",ticks);
	
    return time_buf;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "pid:1008";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    return "tid:24";
}
