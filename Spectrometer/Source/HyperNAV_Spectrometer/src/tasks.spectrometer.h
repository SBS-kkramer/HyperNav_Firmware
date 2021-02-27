/*! \file tasks.spectrometer.h*************************************************
 *
 * \brief spectrometer tasks configuration
 *
 * @author      BP, Satlantic
 * @date        2015-08-20
 *
  ***************************************************************************/

#ifndef _SPECTROMETER_TASKS_H_
#define _SPECTROMETER_TASKS_H_

# include <compiler.h>

# include "FreeRTOS.h"
# include "task.h"

// WATCHDOG TIMEOUT INTERVAL
// #define WDT_TIMEOUT_INTERVAL_MS   9000

// TASK STATUS
typedef enum { TASK_UNKNOWN, TASK_RUNNING, TASK_IN_SETUP, TASK_SLEEPING } taskStatus_t;
#define THIS_TASK_IS_RUNNING(taskStatus)  {taskStatus =  TASK_RUNNING;}
#define THIS_TASK_WILL_SLEEP(taskStatus)  {taskStatus = TASK_SLEEPING;}


//	HyperNAV Setup and Command Handler - Spectrometer Board
//
# define HNV_SetupCmdSpecTask_NAME        ((const signed portCHAR *)"HyperNAV Spectrometer")
# define HNV_SetupCmdSpecTask_STACK_SIZE  512+256+256+128
# define HNV_SetupCmdSpecTask_PRIORITY    (tskIDLE_PRIORITY + 1)
# define HNV_SetupCmdSpecTask_PERIOD_MS   150
extern xTaskHandle  gHNV_SetupCmdSpecTask_Handler;
extern taskStatus_t gHNV_SetupCmdSpecTask_Status;


//	HyperNAV Task Monitor
//
# define HNV_MonitorTask_NAME        ((const signed portCHAR *)"HyperNAV Monitor")
# define HNV_MonitorTask_STACK_SIZE  512
# define HNV_MonitorTask_PRIORITY    (tskIDLE_PRIORITY + 4)
# define HNV_MonitorTask_PERIOD_MS   3000
// TODO Increment monitor task period after development is finished. Now using a low value to catch slow tasks.
# warning "Monitor Task period was set to a low value for development. For final release, adjust!"

//	HyperNAV Data Exchange Spectrometer
//
# define HNV_DataXSpectrmTask_NAME        ((const signed portCHAR *)"HyperNAV Data Exchange Spectrometer")
# define HNV_DataXSpectrmTask_STACK_SIZE  (1024+256+128)
# define HNV_DataXSpectrmTask_PRIORITY    (tskIDLE_PRIORITY + 2)
# define HNV_DataXSpectrmTask_PERIOD_MS   200
extern xTaskHandle  gHNV_DataXSpectrmTask_Handler;
extern taskStatus_t gHNV_DataXSpectrmTask_Status;

//	HyperNAV Data Acquisition
//
# define HNV_DataAcquisitionTask_NAME        ((const signed portCHAR *)"HyperNAV Stream Output")
# define HNV_DataAcquisitionTask_STACK_SIZE  (1024+512+128)
# define HNV_DataAcquisitionTask_PRIORITY    (tskIDLE_PRIORITY + 1)
# define HNV_DataAcquisitionTask_PERIOD_MS   250
extern xTaskHandle  gHNV_DataAcquisitionTask_Handler;
extern taskStatus_t gHNV_DataAcquisitionTask_Status;

void taskMonitor_spectrometer_startTask(void);
Bool taskMonitor_spectrometer_createTask(void);

#endif /* _SPECTROMETER_TASKS_H_ */
