/*! \file tasks.controller.h*************************************************
 *
 * \brief controller tasks configuration
 *
 * @author      BP, Satlantic
 * @date        2015-08-20
 *
  ***************************************************************************/

#ifndef _CONTROLLER_TASKS_H_
#define _CONTROLLER_TASKS_H_

# include <compiler.h>

# include "FreeRTOS.h"
# include "task.h"

// WATCHDOG TIMEOUT INTERVAL
// #define WDT_TIMEOUT_INTERVAL_MS   9000

// TASK STATUS
typedef enum { TASK_UNKNOWN, TASK_RUNNING, TASK_IN_SETUP, TASK_SLEEPING } taskStatus_t;
#define THIS_TASK_IS_RUNNING(taskStatus)  {taskStatus =  TASK_RUNNING;}
#define THIS_TASK_WILL_SLEEP(taskStatus)  {taskStatus = TASK_SLEEPING;}


//	HyperNav Setup and Command Handler - Controller Board
//HyperNav
# define HNV_SetupCmdCtrlTask_NAME        ((const signed portCHAR *)"Cmd Prompt")
# if defined(OPERATION_NAVIS)
# define HNV_SetupCmdCtrlTask_STACK_SIZE  (512+512)
# else
# define HNV_SetupCmdCtrlTask_STACK_SIZE  (1024+1024+512+512+128)
# endif
# define HNV_SetupCmdCtrlTask_PRIORITY    (tskIDLE_PRIORITY + 1)
# define HNV_SetupCmdCtrlTask_PERIOD_MS   250
extern xTaskHandle  gHNV_SetupCmdCtrlTask_Handler;
extern taskStatus_t gHNV_SetupCmdCtrlTask_Status;


//	HyperNav Task Monitor
//
# define HNV_MonitorTask_NAME        ((const signed portCHAR *)"Task Monitor")
# define HNV_MonitorTask_STACK_SIZE  (256)
# define HNV_MonitorTask_PRIORITY    (tskIDLE_PRIORITY + 4)
# define HNV_MonitorTask_PERIOD_MS   3000
// extern xTaskHandle gHNV_MonitorTask_Handler;
// TODO Increment monitor task period after development is finished. Now using a low value to catch slow tasks.
# warning "Monitor Task period was set to a low value for development. For final release, adjust!"

//	HyperNav Auxiliary Data Acquisition
//
# define HNV_AuxDatAcqTask_NAME        ((const signed portCHAR *)"Aux Data Acq")
# define HNV_AuxDatAcqTask_STACK_SIZE  (768)
# define HNV_AuxDatAcqTask_PRIORITY    (tskIDLE_PRIORITY + 1)
# define HNV_AuxDatAcqTask_PERIOD_MS   500
extern xTaskHandle  gHNV_AuxDatAcqTask_Handler;
extern taskStatus_t gHNV_AuxDatAcqTask_Status;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)

//	HyperNav Streaming Output
//
# define HNV_StreamLogTask_NAME        ((const signed portCHAR *)"Stream Output")
# define HNV_StreamLogTask_STACK_SIZE  (512+1024)
# define HNV_StreamLogTask_PRIORITY    (tskIDLE_PRIORITY + 3)
# define HNV_StreamLogTask_PERIOD_MS   100
extern xTaskHandle  gHNV_StreamLogTask_Handler;
extern taskStatus_t gHNV_StreamLogTask_Status;

//	HyperNav Profile Manager
//
# define HNV_ProfileManagerTask_NAME        ((const signed portCHAR *)"Profile Manager")
# define HNV_ProfileManagerTask_STACK_SIZE  (1024)
# define HNV_ProfileManagerTask_PRIORITY    (tskIDLE_PRIORITY + 3)
# define HNV_ProfileManagerTask_PERIOD_MS   100
extern xTaskHandle  gHNV_ProfileManagerTask_Handler;
extern taskStatus_t gHNV_ProfileManagerTask_Status;

# elif defined(OPERATION_AUTONOMOUS)
//	HyperNav Streaming Output
//
# define HNV_StreamLogTask_NAME        ((const signed portCHAR *)"Stream Output")
# define HNV_StreamLogTask_STACK_SIZE  (1024+1024+512)
# define HNV_StreamLogTask_PRIORITY    (tskIDLE_PRIORITY + 3)
# define HNV_StreamLogTask_PERIOD_MS   100
extern xTaskHandle  gHNV_StreamLogTask_Handler;
extern taskStatus_t gHNV_StreamLogTask_Status;

# elif defined(OPERATION_NAVIS)

//	HyperNav Profile Manager
//
# define HNV_ProfileManagerTask_NAME        ((const signed portCHAR *)"Profile Manager")
# define HNV_ProfileManagerTask_STACK_SIZE  (2048+1024+1024+512+1024+512+256)
# define HNV_ProfileManagerTask_PRIORITY    (tskIDLE_PRIORITY + 3)
# define HNV_ProfileManagerTask_PERIOD_MS   100
extern xTaskHandle  gHNV_ProfileManagerTask_Handler;
extern taskStatus_t gHNV_ProfileManagerTask_Status;

# endif

//	HyperNav Data Exchange Controller
//
# define HNV_DataXControlTask_NAME        ((const signed portCHAR *)"Data X Ctrl")
# define HNV_DataXControlTask_STACK_SIZE  (768)
# define HNV_DataXControlTask_PRIORITY    (tskIDLE_PRIORITY + 2)
# define HNV_DataXControlTask_PERIOD_MS   50
extern xTaskHandle  gHNV_DataXControlTask_Handler;
extern taskStatus_t gHNV_DataXControlTask_Status;


# if 0   // FIXME (B.P.)  --   Move to tasks.controller.h

///////////////////////////////////////////////////////////////////////////////
// S Y S T E M     T A S K S     D E F I N I T I O N S
///////////////////////////////////////////////////////////////////////////////

#include "task.h"

#ifdef USE_FIRMWARE_MAIN

/* Main HyperNav Task */
#define configTSK_MNT_NAME        ((const signed portCHAR *)"Main HyperNav")
#define configTSK_MNT_STACK_SIZE  (8192/sizeof(portSTACK_TYPE))			//	See portmacro.h for "#define portSTACK_TYPE  unsigned portLONG"
#define configTSK_MNT_PRIORITY    (tskIDLE_PRIORITY + 1)
extern xTaskHandle MainNitrateTaskHandler;

#else

/* API Test Task */
#define configTSK_APITEST_NAME        ((const signed portCHAR *)"API Test")
#define configTSK_APITEST_STACK_SIZE  (8192/sizeof(portSTACK_TYPE))			//	See portmacro.h for "#define portSTACK_TYPE  unsigned portLONG"
#define configTSK_APITEST_PRIORITY    (tskIDLE_PRIORITY + 2)
extern xTaskHandle APITestTaskHandler;

#endif


/* Telemetry mode monitor task */
#define configTSK_TLM_MODE_NAME			((const signed portCHAR *)"TLM mode")
#define configTSK_TLM_MODE_STACK_SIZE	256
#define configTSK_TLM_MODE_PRIORITY		(tskIDLE_PRIORITY + 2)
#define configTSK_TLM_MODE_PERIOD_MS		50
extern xTaskHandle TLMModeTaskHandler;

# endif

/* USB task definitions. */
#define configTSK_USB_NAME            ((const signed portCHAR *)"USB")
#define configTSK_USB_STACK_SIZE      256
#define configTSK_USB_PRIORITY        (tskIDLE_PRIORITY + 4)
extern xTaskHandle USBTaskHandler;


/* USB device task definitions. */
#define configTSK_USB_DEV_NAME        ((const signed portCHAR *)"USB Dev")
#define configTSK_USB_DEV_STACK_SIZE  256
#define configTSK_USB_DEV_PRIORITY    (tskIDLE_PRIORITY + 3)
#define configTSK_USB_DEV_PERIOD      20
extern xTaskHandle usb_device_tsk;


/* USB device mass-storage task definitions. */
#define configTSK_USB_DMS_NAME        ((const signed portCHAR *)"USB MS")
#define configTSK_USB_DMS_STACK_SIZE  256
#define configTSK_USB_DMS_PRIORITY    (tskIDLE_PRIORITY + 2)
#define configTSK_USB_DMS_PERIOD      20
extern xTaskHandle USBMSTaskHandler;


/* USB device CDC task definitions. */
//!@ Note priority and latency have to be tuned accordingly  (Jira 2010-401-582)
#define configTSK_USB_DCDC_NAME               ((const signed portCHAR *)"USB CDC")
#define configTSK_USB_DCDC_STACK_SIZE         1536
#define configTSK_USB_DCDC_PRIORITY           (tskIDLE_PRIORITY + 2)
#define configTSK_USB_DCDC_PERIOD_MS          4
extern xTaskHandle USBCDCTaskHandler;


/* Power Loss Shutdown task */
#define configTSK_PLS_NAME			((const signed portCHAR *)"PLS")
#define configTSK_PLS_STACK_SIZE	256
#define configTSK_PLS_PRIORITY		(tskIDLE_PRIORITY + 4)			// TODO Diego: Revise this priority



void taskMonitor_controller_startTask(void);
Bool taskMonitor_controller_createTask(void);

#endif /* _CONTROLLER_TASKS_H_ */
