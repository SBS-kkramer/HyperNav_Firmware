/*! \file smc_sram.h*******************************************************
 *
 * \brief SRAM Static Memory Controller configuration file. See AVR32 'SMC
 * Example' demo & Appnote AVR32752 for more information on how to use this file.
 *
 *
 * @author      BP, Satlantic
 * @date        2015-08-20
 *
 *
 * History:
 *	2017-04-24, SF: Changes for rev B PCB - larger SRAM
  ********************************************************************************/


#ifndef _SMC_SRAM_H_
#define _SMC_SRAM_H_

# include <smc.h>

typedef uint16_t* sram_pointer2;
#define smc_write_16(b,o,d)	(((sram_pointer2)(b))[(o)] = (U16)(d))


//! Pointer to SRAM.
#define SRAM_U16		((sram_pointer2)AVR32_EBI_CS1_0_ADDRESS)	

//! SRAM size.
#define SRAM_SIZE         (1<<smc_get_cs_size(1)) // for CS1


//! SMC Peripheral Memory Size in log2(16-bit-words)
#define EXT_SM_SIZE            19				// SRAM is 512k x 16 bits, addressed using ADDR1 - 19	

//! SMC Data Bus Width
#define SMC_DBW                16				// 16-bit bus


//! Whether 8-bit SM chips are connected on the SMC
#define SMC_8_BIT_CHIPS        FALSE

// NWE setup length = (128* NWE_SETUP[5] + NWE_SETUP[4:0])
//! Unit: ns.
#define NWE_SETUP               0

// NCS setup length = (128* NCS_WR_SETUP[5] + NCS_WR_SETUP[4:0])
//! Unit: ns.
#define NCS_WR_SETUP            0

// NRD setup length = (128* NRD_SETUP[5] + NRD_SETUP[4:0])
//! Unit: ns.
#define NRD_SETUP               0

// NCS setup length = (128* NCS_RD_SETUP[5] + NCS_RD_SETUP[4:0])
//! Unit: ns.
#define NCS_RD_SETUP            0

// NCS pulse length = (256* NCS_WR_PULSE[6] + NCS_WR_PULSE[5:0])
//! Unit: ns.
#define NCS_WR_PULSE            82

// NWE pulse length = (256* NWE_PULSE[6] + NWE_PULSE[5:0])
//! Unit: ns.
#define NWE_PULSE               82

// NCS pulse length = (256* NCS_RD_PULSE[6] + NCS_RD_PULSE[5:0])
//! Unit: ns.
#define NCS_RD_PULSE            82

// NRD pulse length = (256* NRD_PULSE[6] + NRD_PULSE[5:0])
//! Unit: ns.
#define NRD_PULSE               82


// Write cycle length = (NWE_CYCLE[8:7]*256 + NWE_CYCLE[6:0])
//! Unit: ns.
#define NCS_WR_HOLD             82
#define NWE_HOLD                82
#define NWE_CYCLE               Max((NCS_WR_SETUP + NCS_WR_PULSE + NCS_WR_HOLD),(NWE_SETUP + NWE_PULSE + NWE_HOLD))

// Read cycle length = (NRD_CYCLE[8:7]*256 + NRD_CYCLE[6:0])
//! Unit: ns.
#define NCS_RD_HOLD             0
#define NRD_HOLD                0
#define NRD_CYCLE               Max((NCS_RD_SETUP + NCS_RD_PULSE + NCS_RD_HOLD),(NRD_SETUP + NRD_PULSE + NRD_HOLD))

// Data float time
#define TDF_CYCLES              0
#define TDF_OPTIM               DISABLED

// Page mode
#define PAGE_MODE               DISABLED
#define PAGE_SIZE               0

//! Whether read is controlled by NCS or by NRD
#define NCS_CONTROLLED_READ     FALSE

//! Whether write is controlled by NCS or by NWE
#define NCS_CONTROLLED_WRITE    FALSE

//! Whether to use the NWAIT pin
#define NWAIT_MODE              AVR32_SMC_EXNW_MODE_DISABLED

#endif //_SMC_SRAM_H_
