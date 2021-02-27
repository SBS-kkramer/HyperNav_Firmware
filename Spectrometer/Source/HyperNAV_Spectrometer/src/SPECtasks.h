/*!	\file SPECtasks.h
 *
 * Created: 7/26/2015 9:45:22 PM
 *  Author: Scott
 */ 


#ifndef SPECTASKS_H_
#define SPECTASKS_H_

/* Main Task */

#define configTSK_SPEC_TSK_NAME        ((const signed portCHAR *)"SPEC")
#define configTSK_SPEC_TSK_STACK_SIZE  1024				//was (4096/sizeof(portSTACK_TYPE))	See portmacro.h for "#define portSTACK_TYPE  unsigned portLONG"
#define configTSK_SPEC_TSK_PRIORITY    (tskIDLE_PRIORITY + 1)
extern xTaskHandle gMainSPECTaskHandler;



#endif /* SPECTASKS_H_ */