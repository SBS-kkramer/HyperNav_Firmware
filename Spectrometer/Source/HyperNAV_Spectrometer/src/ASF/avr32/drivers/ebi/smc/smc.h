/*****************************************************************************
 *
 * \file
 *
 * \brief SMC on EBI driver for AVR32 UC3.
 *
 * Copyright (c) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 ******************************************************************************/
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */


#ifndef _SMC_H_
#define _SMC_H_

/**
 * \defgroup group_avr32_drivers_ebi_smc MEMORY - EBI Static Memory Controller
 *
 * EBI (External Bus Interface) SMC (Static Memory Controller) allows to connect a Static Memory to the microcontroller.
 *
 * \{
 */

#include <avr32/io.h>

#include "compiler.h"
#include "conf_ebi.h"

/*! \brief Initializes the AVR32 SMC module and the connected SRAM(s).
 * \param hsb_hz HSB frequency in Hz (the HSB frequency is applied to the SMC).
 * \note Each access to the SMC address space validates the mode of the SMC
 *       and generates an operation corresponding to this mode.
 */
extern void smc_init(unsigned long hsb_hz);

/*! \brief Return the size of the peripheral connected  .
 *  \param cs The chip select value
 */
extern unsigned char smc_get_cs_size(unsigned char cs);

#endif  // _SMC_H_
