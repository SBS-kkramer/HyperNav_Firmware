/*! \file tasks.spectrometer.c*************************************************
 *
 * \brief spectrometer tasks handlers
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-07
 *
  ***************************************************************************/

# include "tasks.spectrometer.h"

# include <stdint.h>
# include <time.h>
# include <sys/time.h>

# include "wdt.h"
//# include "io_funcs.spectrometer.h"
# include "power_clocks_lib.h"

# ifdef DEBUG
#  define DEBUG_TSKS
# endif

//*****************************************************************************
//  Global variables:
//
//  Handle: Passed to xTaskCreate(...) for initialization, but never used.
//  Status: periodically updated by each task to prove that that task is not stuck
//
//*****************************************************************************

xTaskHandle gHNV_SetupCmdSpecTask_Handler = NULL;
taskStatus_t gHNV_SetupCmdSpecTask_Status = TASK_UNKNOWN;

static xTaskHandle gHNV_MonitorTask_Handler;

//	Data Exchange on Spectrometer
xTaskHandle  gHNV_DataXSpectrmTask_Handler = NULL;
taskStatus_t gHNV_DataXSpectrmTask_Status  = TASK_UNKNOWN;

//	Data Acquisition Task
xTaskHandle  gHNV_DataAcquisitionTask_Handler = NULL;
taskStatus_t gHNV_DataAcquisitionTask_Status  = TASK_UNKNOWN;

//xTaskHandle  gHNV_ReadADCTask_Handler = NULL;


static int8_t monitorTask_isRunning = 0;


//*****************************************************************************
// Watchdog kicking (monitor) task.
static void gHNV_MonitorTask ( void* pvParameters_notUsed ) {

    // Tasks to monitor
    taskStatus_t* monitoredTasks[] = {
            &gHNV_SetupCmdSpecTask_Status,
            &gHNV_DataXSpectrmTask_Status,
            &gHNV_DataAcquisitionTask_Status,
            NULL };

# ifdef DEBUG_TSKS
    const signed portCHAR* taskNames[] = {
            HNV_SetupCmdSpecTask_NAME,
            HNV_DataXSpectrmTask_NAME,
            HNV_DataAcquisitionTask_NAME,
            NULL };
# endif

    int16_t const N_MON_TASKS = ( sizeof(monitoredTasks) / sizeof(taskStatus_t*) ) - 1;

  # ifdef DEBUG_TSKS
    U8 nFailedTest[N_MON_TASKS];                  // Failure to meet deadline counter
    // Initialize failure counter to 0
    bzero(nFailedTest, N_MON_TASKS);
  # endif

    for(;;) {

        // Run only periodically
        vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_MonitorTask_PERIOD_MS ) );

        if( monitorTask_isRunning ) {

            // Check status for each task.
            // If any one task state is unknown,
            // do not kick the WDT, thus trigger a reset
            int8_t allTasksReportingOk = true;

//          struct timeval now;
//          gettimeofday( &now, (void*)0 );
//          io_out_S32( "Time: %ld    ", now.tv_sec );
//          io_out_string("TMT");

			uint32_t gplp = pcl_read_gplp(1);

            int16_t i;
            for(i=0; i< N_MON_TASKS; i++) {

				uint32_t task_bit = 0x0;

                if( *(monitoredTasks[i]) == TASK_UNKNOWN ) {

                  # ifndef DEBUG_TSKS

                    allTasksReportingOk = false;

					task_bit = 0x01;
                    //io_out_string(" --");
                    vTaskDelay( (portTickType)TASK_DELAY_MS( 200 ) );

                  # else
# error "Do not want to compile this way..."
                    nFailedTest[i]++;
                    // During debugging, always kick the WDT.
                    // If a task hangs, it will be reported in a different way.
                    if( nFailedTest[i] >= 6 ) { // A task is unresponsive. Abort execution

                    //  io_out_string("\r\nTask '");
                    //  io_out_string((char*) taskNames[i]);
                    //  io_out_string("' is not responsive.\r\nAborting execution");
                        while(true) {
                          wdt_clear();
                        }
                    }

                  # endif

                } else if( *(monitoredTasks[i]) == TASK_IN_SETUP ) {

					task_bit = 0x02;
                    //io_out_string(" xx");
                    //  This is used in the setup & control task
                    //  to permit user input without 'status unknown' interruptions
                    //  or a watchdog reset

                } else if( *(monitoredTasks[i]) == TASK_RUNNING ) {

					task_bit = 0x04;
                    //io_out_string(" ++");
                  # ifdef DEBUG_TSKS
                    nFailedTest[i] = 0;                     // Reset failure to meet deadline counter
                  # endif

                    *(monitoredTasks[i]) = TASK_UNKNOWN;

                } else if( *(monitoredTasks[i]) == TASK_SLEEPING ) {

					task_bit = 0x08;
                    //io_out_string(" zz");
                    //  A sleeping task is fine, no concern
                }
				//
				//  Write the bits of this task to GPLP register.
				//  The steps are (for the example of i=3):
				//  [ 0000 0000 0000 0000 0000 0000 0000 tttt ]
				//  Left shift by 4*i:
				//  [ 0000 0000 0000 0000 tttt 0000 0000 0000 ]
				//
				//  ones are all 1 at the task location:
				//  [ 0000 0000 0000 0000 0000 0000 0000 1111 ]
				//  Left shift by 4*i:
				//  [ 0000 0000 0000 0000 1111 0000 0000 0000 ]
				//
				//  OR task with all ones except at task location:
				//  [ 1111 1111 1111 1111 tttt 1111 1111 1111 ]
				//
				//  In gplp, set all task bits to 1:
				//  [ gplp gplp gplp gplp gplp gplp gplp gplp ]
				//  gplp |= ones
				//  [ gplp gplp gplp gplp 1111 gplp gplp gplp ]
				//  then logical AND with the following
				//  [ 1111 1111 1111 1111 tttt 1111 1111 1111 ]
				//  to get to the value to be written to register:
				//  [ gplp gplp gplp gplp tttt gplp gplp gplp ]
				
				task_bit <<= 4*i;
				uint32_t ones = 0xF<<(4*i);
				task_bit  |= ~ones;
				
				gplp  |= ones;
				gplp  &= task_bit;
            }

            // Kick the WDT if all tasks passed the health check
            if( allTasksReportingOk ) {
                wdt_clear();
                //io_out_string(" clear\r\n");
            } else {
                //io_out_string(" ALERT\r\n");
            }

			pcl_write_gplp(1,gplp);
        } //  End of if ( monitorTask_isRunning )

    }   /*  End of infinite loop  */
}


//! \brief		Start spec board task monitor
void taskMonitor_spectrometer_startTask(void)
{
    monitorTask_isRunning = 1;   // Start task monitor - this happens only once at the beginning.
}


//! \brief		Create spec board task monitor
//! @return     true:Success false:Fail
Bool taskMonitor_spectrometer_createTask(void)
{
    if (pdPASS == xTaskCreate( gHNV_MonitorTask,
                    HNV_MonitorTask_NAME, HNV_MonitorTask_STACK_SIZE, NULL,
                    HNV_MonitorTask_PRIORITY, &gHNV_MonitorTask_Handler) ) {
            return true;
    } else {
            return false;
    }
}

