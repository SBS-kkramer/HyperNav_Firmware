/* This source file is part of the ATMEL AVR-UC3-SoftwareFramework-1.7.0 Release */

/*This file is prepared for Doxygen automatic documentation generation.*/
/*! \file ******************************************************************
 *
 * \brief Processing of USB device specific enumeration requests.
 *
 * This file contains the specific request decoding for enumeration process.
 *
 * - Compiler:           IAR EWAVR32 and GNU GCC for AVR32
 * - Supported devices:  All AVR32 devices with a USB module can be used.
 * - AppNote:
 *
 * \author               Atmel Corporation: http://www.atmel.com \n
 *                       Support and FAQ: http://support.atmel.no/
 *
 ***************************************************************************/

/* Copyright (c) 2009 Atmel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an Atmel
 * AVR product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 */

//_____ I N C L U D E S ____________________________________________________

#include "conf_usb.h"


#if USB_DEVICE_FEATURE == ENABLED

#include "usb_drv.h"
#include "usb_descriptors.h"
#include "usb_standard_request.h"
#include "usb_specific_request.h"
#include "ctrl_access.h"
#include "compiler.h"

# include "config.controller.h"   //  FIXME -- Somehow, the Serial Number should be passed to this module, not retrieved vie a CFG_...() call.

// ____ T Y P E  D E F I N I T I O N _______________________________________

typedef struct
{
   U32 dwDTERate;
   U8 bCharFormat;
   U8 bParityType;
   U8 bDataBits;
}S_line_coding;


//_____ D E F I N I T I O N S ______________________________________________

volatile Bool ms_multiple_drive;

S_line_coding   line_coding;

//_____ P R I V A T E   D E C L A R A T I O N S ____________________________

extern const  void *pbuffer;
extern        U16   data_to_transfer;

//! @brief This function manages reception of line coding parameters (baudrate...).
//!
void  cdc_get_line_coding(void);

//! @brief This function manages reception of line coding parameters (baudrate...).
//!
void  cdc_set_line_coding(void);

//! @brief This function manages the SET_CONTROL_LINE_LINE_STATE CDC request.
//!
//! @todo Manages here hardware flow control...
//!
void 	cdc_set_control_line_state (void);


//! @brief Update serial number in product name descriptor
//!
static void  updateProdDescriptor(void);

//! @brief Update serial number in USB serial number descriptor
//!
static void  updateSNDescriptor(void);


//_____ D E C L A R A T I O N S ____________________________________________

//! @brief This function configures the endpoints of the device application.
//! This function is called when the set configuration request has been received.
//!
void usb_user_endpoint_init(U8 conf_nb)
{
  ms_multiple_drive = FALSE;

#if (USB_HIGH_SPEED_SUPPORT==ENABLED)
   if( !Is_usb_full_speed_mode() )
   {
     (void)Usb_configure_endpoint(EP_MS_IN,
                                  EP_ATTRIBUTES_1,
                                  DIRECTION_IN,
                                  EP_SIZE_1_HS,
                                  DOUBLE_BANK);
   
     (void)Usb_configure_endpoint(EP_MS_OUT,
                                  EP_ATTRIBUTES_2,
                                  DIRECTION_OUT,
                                  EP_SIZE_2_HS,
                                  DOUBLE_BANK);
      return;
   }
#endif

   // CDC Endpoints
   //
   (void)Usb_configure_endpoint(TX_EP,
		   EP_ATTRIBUTES_1,
		   DIRECTION_IN,
		   EP_SIZE_1_FS,
		   DOUBLE_BANK);

   (void)Usb_configure_endpoint(RX_EP,
		   EP_ATTRIBUTES_2,
		   DIRECTION_OUT,
		   EP_SIZE_2_FS,
		   DOUBLE_BANK);

   (void)Usb_configure_endpoint(INT_EP,
   		   EP_ATTRIBUTES_3,
   		   DIRECTION_IN,
   		   EP_SIZE_3,
   		   SINGLE_BANK);


   // Mass Storage Endpoints
   //
  (void)Usb_configure_endpoint(EP_MS_IN,
		  EP_ATTRIBUTES_4,
		  DIRECTION_IN,
		  EP_SIZE_4_FS,
		  DOUBLE_BANK);

  (void)Usb_configure_endpoint(EP_MS_OUT,
		  EP_ATTRIBUTES_5,
		  DIRECTION_OUT,
		  EP_SIZE_5_FS,
		  DOUBLE_BANK);
}


//! This function is called by the standard USB read request function when
//! the USB request is not supported. This function returns TRUE when the
//! request is processed. This function returns FALSE if the request is not
//! supported. In this case, a STALL handshake will be automatically
//! sent by the standard USB read request function.
//!
Bool usb_user_read_request(U8 type, U8 request)
{
   U16 wInterface;
   U8 wValue_msb;
   U8 wValue_lsb;

   wValue_lsb = Usb_read_endpoint_data(EP_CONTROL, 8);
   wValue_msb = Usb_read_endpoint_data(EP_CONTROL, 8);
   
   if( USB_SETUP_SET_CLASS_INTER == type )
   {
      switch( request )
      {
         case MASS_STORAGE_RESET:
         // wValue must be 0
         // wIndex = Interface
         if( (0!=wValue_lsb) || (0!=wValue_msb) )
            return FALSE;
         wInterface=Usb_read_endpoint_data(EP_CONTROL, 16);
         if( INTERFACE1_NB != wInterface )
        	 return FALSE;
         else
         {
        	 Usb_ack_setup_received_free();
        	 Usb_ack_control_in_ready_send();
        	 return TRUE;
         }

         // CDC requests --

         case GET_LINE_CODING:
        	 cdc_get_line_coding();
        	 return TRUE;

         case SET_LINE_CODING:
        	 cdc_set_line_coding();
        	 return TRUE;

         case SET_CONTROL_LINE_STATE:
        	 cdc_set_control_line_state();
        	 return TRUE;


         default: return FALSE;
      }
   }
   if( USB_SETUP_GET_CLASS_INTER == type )
   {
      switch( request )
      {
         case GET_MAX_LUN:
         // wValue must be 0
         // wIndex = Interface
         if( (0!=wValue_lsb) || (0!=wValue_msb) )
            return FALSE;
         wInterface=Usb_read_endpoint_data(EP_CONTROL, 16);
         if( INTERFACE1_NB != wInterface )
            return FALSE;
         Usb_ack_setup_received_free();
         Usb_reset_endpoint_fifo_access(EP_CONTROL);
         Usb_write_endpoint_data(EP_CONTROL, 8, get_nb_lun() - 1);
         Usb_ack_control_in_ready_send();
         while (!Is_usb_control_in_ready());
      
         while(!Is_usb_control_out_received());
         Usb_ack_control_out_received_free();

         ms_multiple_drive = TRUE;
         return TRUE;


         // CDC requests --
         case GET_LINE_CODING:
        	 cdc_get_line_coding();
        	 return TRUE;


         default: return FALSE;
      }
   }

   // If class type does not match any supported one return FALSE
   return FALSE;
}


//! This function returns the size and the pointer on a user information
//! structure
//!
Bool usb_user_get_descriptor(U8 type, U8 string)
{
  pbuffer = NULL;

  switch (type)
  {
  case STRING_DESCRIPTOR:
    switch (string)
    {
    case LANG_ID:
      data_to_transfer = sizeof(usb_user_language_id);
      pbuffer = &usb_user_language_id;
      break;

    case MAN_INDEX:
      data_to_transfer = sizeof(usb_user_manufacturer_string_descriptor);
      pbuffer = &usb_user_manufacturer_string_descriptor;
      break;

    case PROD_INDEX:
      updateProdDescriptor();
      data_to_transfer = sizeof(usb_user_product_string_descriptor);
      pbuffer = &usb_user_product_string_descriptor;
      break;

    case SN_INDEX:
      updateSNDescriptor();
      data_to_transfer = sizeof(usb_user_serial_number);
      pbuffer = &usb_user_serial_number;
      break;

    default:
      break;
    }
    break;

  default:
    break;
  }

  return pbuffer != NULL;
}


void cdc_get_line_coding(void)
{
   Usb_ack_setup_received_free();

   Usb_reset_endpoint_fifo_access(EP_CONTROL);
   Usb_write_endpoint_data(EP_CONTROL, 8, LSB0(line_coding.dwDTERate));
   Usb_write_endpoint_data(EP_CONTROL, 8, LSB1(line_coding.dwDTERate));
   Usb_write_endpoint_data(EP_CONTROL, 8, LSB2(line_coding.dwDTERate));
   Usb_write_endpoint_data(EP_CONTROL, 8, LSB3(line_coding.dwDTERate));
   Usb_write_endpoint_data(EP_CONTROL, 8, line_coding.bCharFormat);
   Usb_write_endpoint_data(EP_CONTROL, 8, line_coding.bParityType);
   Usb_write_endpoint_data(EP_CONTROL, 8, line_coding.bDataBits  );

   Usb_ack_control_in_ready_send();
   while (!Is_usb_control_in_ready());

   while(!Is_usb_control_out_received());
   Usb_ack_control_out_received_free();
}


void cdc_set_line_coding (void)
{
   Usb_ack_setup_received_free();

   while(!Is_usb_control_out_received());
   Usb_reset_endpoint_fifo_access(EP_CONTROL);

   LSB0(line_coding.dwDTERate) = Usb_read_endpoint_data(EP_CONTROL, 8);
   LSB1(line_coding.dwDTERate) = Usb_read_endpoint_data(EP_CONTROL, 8);
   LSB2(line_coding.dwDTERate) = Usb_read_endpoint_data(EP_CONTROL, 8);
   LSB3(line_coding.dwDTERate) = Usb_read_endpoint_data(EP_CONTROL, 8);
   line_coding.bCharFormat = Usb_read_endpoint_data(EP_CONTROL, 8);
   line_coding.bParityType = Usb_read_endpoint_data(EP_CONTROL, 8);
   line_coding.bDataBits = Usb_read_endpoint_data(EP_CONTROL, 8);
   Usb_ack_control_out_received_free();

   Usb_ack_control_in_ready_send();
   while (!Is_usb_control_in_ready());

   // Set the baudrate of the USART
  /*{
      static usart_options_t dbg_usart_options;
      U32 stopbits, parity;

      if     ( line_coding.bCharFormat==0 )   stopbits = USART_1_STOPBIT;
      else if( line_coding.bCharFormat==1 )   stopbits = USART_1_5_STOPBITS;
      else                                    stopbits = USART_2_STOPBITS;

      if     ( line_coding.bParityType==0 )   parity = USART_NO_PARITY;
      else if( line_coding.bParityType==1 )   parity = USART_ODD_PARITY;
      else if( line_coding.bParityType==2 )   parity = USART_EVEN_PARITY;
      else if( line_coding.bParityType==3 )   parity = USART_MARK_PARITY;
      else                                    parity = USART_SPACE_PARITY;

      // Options for debug USART.
      dbg_usart_options.baudrate    = line_coding.dwDTERate;
      dbg_usart_options.charlength  = line_coding.bDataBits;
      dbg_usart_options.paritytype  = parity;
      dbg_usart_options.stopbits    = stopbits;
      dbg_usart_options.channelmode = USART_NORMAL_CHMODE;

      // Initialize it in RS232 mode.
      usart_init_rs232(DBG_USART, &dbg_usart_options, pcl_freq_param.pba_f);
   }*/
}


void cdc_set_control_line_state (void)
{
   Usb_ack_setup_received_free();
   Usb_ack_control_in_ready_send();
   while (!Is_usb_control_in_ready());
}


//! @brief Update serial number in product name descriptor
//!
static void  updateProdDescriptor(void)
{
	U16 serial = CFG_Get_Serial_Number(); 	// Read instrument serial number fron userpage

	// Modify the last 4 chars of the descriptor string to be the serial number
	usb_user_product_string_descriptor.wstring[USB_PN_LENGTH-1] = Swap16('0'+ serial%10);
	usb_user_product_string_descriptor.wstring[USB_PN_LENGTH-2] = Swap16('0'+ (serial%100)/10);
	usb_user_product_string_descriptor.wstring[USB_PN_LENGTH-3] = Swap16('0'+ (serial%1000)/100);
	usb_user_product_string_descriptor.wstring[USB_PN_LENGTH-4] = Swap16('0'+ (serial%10000)/1000);
}


//! @brief Update serial number in USB serial number descriptor
//!
static void  updateSNDescriptor(void)
{
	U16 serial = CFG_Get_Serial_Number(); 	// Read instrument serial number from userpage

	// Modify the last 4 chars of the descriptor string to be the serial number
	usb_user_serial_number.wstring[USB_SN_LENGTH-1] = Swap16('0' + serial%10);
	usb_user_serial_number.wstring[USB_SN_LENGTH-2] = Swap16('0' + (serial%100)/10);
	usb_user_serial_number.wstring[USB_SN_LENGTH-3] = Swap16('0' + (serial%1000)/100);
	usb_user_serial_number.wstring[USB_SN_LENGTH-4] = Swap16('0' + (serial%10000)/1000);
}



#endif  // USB_DEVICE_FEATURE == ENABLED
