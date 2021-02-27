/*! \file smc_sram.h*******************************************************
 *
 * \brief SRAM Static Memory Controller configuration file. See AVR32 'SMC
 * Example' demo & Appnote AVR32752 for more information on how to use this file.
 *
 *
 * @author      BP, Satlantic
 * @date        2015-08-20
 *
  ********************************************************************************/


#ifndef _SMC_SRAM_H_
#define _SMC_SRAM_H_

# include <smc.h>

typedef U8* sram_pointer;
//! SRAM byte-write macro
#define smc_write_8(b,o,d)	(((sram_pointer)(b))[(o)] = (U8)(d))


//! Pointer to SRAM.
#define SRAM              ((sram_pointer)AVR32_EBI_CS0_ADDRESS)

//! SRAM size.
#define SRAM_SIZE         (1<<smc_get_cs_size(0))


//! SMC Peripheral Memory Size in log2(Bytes)
#define EXT_SM_SIZE            17				// SRAM is 128kB, addressed using ADDR0 to ADDR16

//! SMC Data Bus Width
#define SMC_DBW                8				// Use 8-bit bus as sram uses lower 8-bits of data bus

//! Whether 8-bit SM chips are connected on the SMC
#define SMC_8_BIT_CHIPS        TRUE


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
