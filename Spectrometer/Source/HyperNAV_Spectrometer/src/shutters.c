/*
 * shutters.c
 *
 * Created: 05/08/2016 3:47:38 PM
 *  Author: rvandommelen
 */ 

# include "shutters.h"

#include "gpio.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"


#define shutterPulse 10	// Shutter switching pulse duration in milliseconds

void shutters_InitGpio() {

  // Shutter power supply
  #if (MCU_BOARD_REV==REV_A)
  gpio_configure_pin(SHTR_PWR_ON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );	// Shutter power is initialized OFF, turned on later
  // always on in Rev_B
  #endif

  // Shutter direction pins
  gpio_configure_pin(FORWARD_A, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );		// Shutter direction pins should be OFF
  gpio_configure_pin(REVERSE_A, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );		// Shutter direction pins should be OFF
  gpio_configure_pin(FORWARD_B, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );		// Shutter direction pins should be OFF
  gpio_configure_pin(REVERSE_B, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );		// Shutter direction pins should be OFF
  
  
	
}

// Turn on the shutter power supply
void shutters_power_on(void) {
#if (MCU_BOARD_REV==REV_A)		
	gpio_set_gpio_pin(SHTR_PWR_ON);
#endif	
	return;
}

// Turn off the shutter power supply
void shutters_power_off(void) {
#if (MCU_BOARD_REV==REV_A)		
	gpio_clr_gpio_pin(SHTR_PWR_ON);
#endif	
	return;
}

void shutters_open(void) {
	// Ensure that the reverse directions are OFF
	gpio_clr_gpio_pin(REVERSE_A);
	gpio_clr_gpio_pin(REVERSE_B);
	// Start the switching pulses
	gpio_set_gpio_pin(FORWARD_A);
	gpio_set_gpio_pin(FORWARD_B);
	// Duration of switching pulses
	vTaskDelay(shutterPulse);
	// End the switching pulse.
	gpio_clr_gpio_pin(FORWARD_A);
	gpio_clr_gpio_pin(FORWARD_B);
	return;
}

void shutters_close(void) {
	// Ensure that the forward directions are OFF
	gpio_clr_gpio_pin(FORWARD_A);
	gpio_clr_gpio_pin(FORWARD_B);
	// Start the switching pulses
	gpio_set_gpio_pin(REVERSE_A);
	gpio_set_gpio_pin(REVERSE_B);
	// Duration of switching pulse
	vTaskDelay(shutterPulse);
	// End the switching pulses
	gpio_clr_gpio_pin(REVERSE_A);
	gpio_clr_gpio_pin(REVERSE_B);
	return;
}

void shutter_A_open(void) {
	// Ensure that the reverse direction is OFF
	gpio_clr_gpio_pin(REVERSE_A);
	// Start the switching pulse
	gpio_set_gpio_pin(FORWARD_A);
	// Duration of switching pulse
	vTaskDelay(shutterPulse);
	// End the switching pulse
	gpio_clr_gpio_pin(FORWARD_A);
	return;
}

void shutter_A_close(void) {
	// Ensure that the forward direction is OFF
	gpio_clr_gpio_pin(FORWARD_A);
	// Start the switching pulse
	gpio_set_gpio_pin(REVERSE_A);
	// Duration of switching pulse
	vTaskDelay(shutterPulse);
	// End the switching pulse
	gpio_clr_gpio_pin(REVERSE_A);
	return;
}

void shutter_B_open(void) {
	// Ensure that the reverse direction is OFF
	gpio_clr_gpio_pin(REVERSE_B);
	// Start the switching pulse
	gpio_set_gpio_pin(FORWARD_B);
	// Duration of switching pulse
	vTaskDelay(shutterPulse);
	// End the switching pulse
	gpio_clr_gpio_pin(FORWARD_B);

	return;
}

void shutter_B_close(void) {
	// Ensure that the forward direction is OFF
	gpio_clr_gpio_pin(FORWARD_B);
	// Start the switching pulse
	gpio_set_gpio_pin(REVERSE_B);
	// Duration of switching pulse
	vTaskDelay(shutterPulse);
	// End the switching pulse
	gpio_clr_gpio_pin(REVERSE_B);
	return;
}
