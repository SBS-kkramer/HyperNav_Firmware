/*! \file tasks.controller.c*************************************************
 *
 * \brief controller tasks handlers
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-07
 *
  ***************************************************************************/

# include "tasks.controller.h"

# include <stdint.h>
# include <time.h>
# include <sys/time.h>

# include "pm.h"
# include "telemetry.h"
# include "watchdog.h"
# include "io_funcs.controller.h"

//*****************************************************************************
// Exported handlers
//*****************************************************************************

//  Setup and Command Handling Task
xTaskHandle  gHNV_SetupCmdCtrlTask_Handler = NULL;
taskStatus_t gHNV_SetupCmdCtrlTask_Status  = TASK_UNKNOWN;

// Monitor task
static xTaskHandle gHNV_MonitorTask_Handler = NULL;

//  Auxiliary data acquisition task
xTaskHandle  gHNV_AuxDatAcqTask_Handler = NULL;
taskStatus_t gHNV_AuxDatAcqTask_Status  = TASK_UNKNOWN;

# if defined(OPERATION_AUTONOMOUS)
//  Stream log task
xTaskHandle  gHNV_StreamLogTask_Handler = NULL;
taskStatus_t gHNV_StreamLogTask_Status  = TASK_UNKNOWN;
# endif

# if defined(OPERATION_NAVIS)
//  Profile manager task
xTaskHandle  gHNV_ProfileManagerTask_Handler = NULL;
taskStatus_t gHNV_ProfileManagerTask_Status  = TASK_UNKNOWN;
# endif

//  Data Exchange on Controller
xTaskHandle  gHNV_DataXControlTask_Handler = NULL;
taskStatus_t gHNV_DataXControlTask_Status  = TASK_UNKNOWN;



static int8_t monitorTask_isRunning = 0;

//*****************************************************************************
// Watchdog kicking (monitor) task.
static void gHNV_MonitorTask ( __attribute__((unused)) void* pvParameters_notUsed ) {

    // Tasks to monitor
    taskStatus_t* monitoredTasks[] = {
            &gHNV_SetupCmdCtrlTask_Status,
# if defined(OPERATION_AUTONOMOUS)
            &gHNV_StreamLogTask_Status,
# endif
# if defined(OPERATION_NAVIS)
            &gHNV_ProfileManagerTask_Status,
# endif
            &gHNV_DataXControlTask_Status,
            &gHNV_AuxDatAcqTask_Status,
            NULL };

# ifdef DEBUG
    const signed portCHAR* taskNames[] = {
            HNV_SetupCmdCtrlTask_NAME,
# if defined(OPERATION_AUTONOMOUS)
            HNV_StreamLogTask_NAME,
# endif
# if defined(OPERATION_NAVIS)
            HNV_ProfileManagerTask_NAME,
# endif
            HNV_DataXControlTask_NAME,
            HNV_AuxDatAcqTask_NAME,
            NULL };
# endif

    const int N_MON_TASKS = ( sizeof(monitoredTasks) / sizeof(taskStatus_t*) ) - 1;

  # ifdef DEBUG
    U8 nFailedTest[N_MON_TASKS];                  // Failure to meet deadline counter
    // Initialize failure counter to 0
    bzero(nFailedTest, N_MON_TASKS);
  # endif

    for(;;) {

        // Run only periodically
        vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_MonitorTask_PERIOD_MS ) );

        if ( monitorTask_isRunning ) {

            // Check status for each task.
            // If any one task state is unknown,
            // do not kick the WDT, thus trigger a reset
            int8_t allTasksReportingOk = true;

         // truct timeval now;
         // gettimeofday( &now, (void*)0 );
         // io_out_S32( "Time: %ld    ", now.tv_sec );
         // io_out_string("TMT");

            uint32_t gplp = pm_read_gplp(&AVR32_PM,1);

            int16_t i;
            for(i=0; i< N_MON_TASKS; i++) {

                uint32_t task_bit = 0x0;

                if( *(monitoredTasks[i]) == TASK_UNKNOWN ) {


                  # ifndef DEBUG

                    allTasksReportingOk = false;

                    task_bit = 0x01;
                  //io_out_string(" --");
                    vTaskDelay( (portTickType)TASK_DELAY_MS( 200 ) );

                  # else
# error "Task Monitor Compiling Error"
                    nFailedTest[i]++;
                    // During debugging, always kick the WDT.
                    // If a task hangs, it will be reported in a different way.
                    if( nFailedTest[i] >= 6 ) { // A task is unresponsive. Abort execution

                        //tlm_msg("\r\nTask '");
                        //tlm_msg((char*) taskNames[i]);
                        //tlm_msg("' is not responsive.\r\nAborting execution");
                        while(true) {
                          watchdog_clear();
                        }
                    }
                  # endif

                } else if( *(monitoredTasks[i]) == TASK_IN_SETUP ) {

                    task_bit = 0x02;
                  //io_out_string(" xx");
                    //  This is used in the setup & controll thread
                    //  to permit user input without 'status unknown' interruptions
                    //  or a watchdog reset

                } else if( *(monitoredTasks[i]) == TASK_RUNNING ) {

                    task_bit = 0x04;
                  //io_out_string(" ++");
                  # ifdef DEBUG
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

                watchdog_clear();
                //io_out_string(" clear\r\n");
            } else {
                //io_out_string(" ALERT\r\n");
            }

            pm_write_gplp(&AVR32_PM,1,gplp);
        }
    }   /*  End of infinite loop  */
}

void taskMonitor_controller_startTask(void)
{
    monitorTask_isRunning = 1;   // Start task monitor - this happens only once at the beginning.
}

Bool taskMonitor_controller_createTask(void)
{
    if (pdPASS == xTaskCreate( gHNV_MonitorTask,
                    HNV_MonitorTask_NAME, HNV_MonitorTask_STACK_SIZE, NULL,
                    HNV_MonitorTask_PRIORITY, &gHNV_MonitorTask_Handler) ) {
            return true;
    } else {
            return false;
    }
}
