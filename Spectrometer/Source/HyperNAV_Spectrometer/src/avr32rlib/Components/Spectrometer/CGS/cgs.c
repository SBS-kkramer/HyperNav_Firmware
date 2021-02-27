/*
 * cgs.c
 *
 * Created: 7/4/2016 4:19:56 PM
 *  Author: sfeener
 */ 

# include "cgs.h"
# include "cgs_cfg.h"


# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <sys/time.h>

# include "compiler.h"
# include "board.h"

# include "FreeRTOS.h"
# include "semphr.h"
# include "queue.h"
# include "task.h"

#include <avr32/io.h>
#include "power_clocks_lib.h"
#include "gpio.h"
#include "tc.h"
#include "delay.h"
#include <eic.h>
#include <ast.h>

#include "intc.h"
#include "io_funcs.spectrometer.h"
#include "systemtime.spectrometer.h"

//#include "gplp_trace.spectrometer.h"

# include "components.h"
# include "AD7677.h"
# include "sn74v283.h"

# include "tasks.spectrometer.h"

//#include "smc_adc.h"
//#include "smc_sram.h"  
//#include "sram_memory_map.spectrometer.h"

#define CGS_EIC_INTERRUPTS  2  // number of external interrupts

static bool specClock_initialized = false;

static bool timerAinit = false;
static bool timerBinit = false;

typedef enum {
  CGS_Undefined, CGS_Clearing, CGS_Acquiring
} CGS_Activity;

static bool cgs_autoClearing = false;

static uint16_t request_clear_A = 0;
static uint16_t request_acqui_A = 0;
static uint16_t received_EOS_A  = 0;
static char     which_EOS_A[16];
static CGS_Activity previous_action_A = CGS_Undefined; 

static uint16_t request_clear_B = 0;
static uint16_t request_acqui_B = 0;
static uint16_t received_EOS_B  = 0;
static char     which_EOS_B[16];
static CGS_Activity previous_action_B = CGS_Undefined; 

static bool spec_A_enabled = false;
static bool spec_B_enabled = false;

//  The following variables track times:
//  When clearing was started,
//  when integration was started,
//  when integration was complete.
//
static volatile uint32_t begunClearing_ast_A;
static volatile uint32_t begunIntegration_ast_A = 0;
static volatile uint32_t endedIntegration_ast_A = 0;

uint32_t CGS_ClearoutBegun_A   () { return begunClearing_ast_A; }
uint32_t CGS_IntegrationBegun_A() { return begunIntegration_ast_A; }
uint32_t CGS_IntegrationEnded_A() { return endedIntegration_ast_A; }
uint32_t CGS_IntegrationTime_A () { return endedIntegration_ast_A-begunIntegration_ast_A; }

static volatile uint32_t begunClearing_ast_B;
static volatile uint32_t begunIntegration_ast_B = 0;
static volatile uint32_t endedIntegration_ast_B = 0;

uint32_t CGS_ClearoutBegun_B   () { return begunClearing_ast_B; }
uint32_t CGS_IntegrationBegun_B() { return begunIntegration_ast_B; }
uint32_t CGS_IntegrationEnded_B() { return endedIntegration_ast_B; }
uint32_t CGS_IntegrationTime_B () { return endedIntegration_ast_B-begunIntegration_ast_B; }

//  The start pulse is sent to the spectrometer.
//
//  The pulse triggers the 2058 (or 2059?) analog-to-digital conversions.
//  Those conversions in turn trigger hardware signals,
//  that prompt the digital values to be written to the FIFO (A or B).
//
//  At the very end, the spectrometer generates the EOS (End of Scan),
//  which triggers an interrup in firmware.
//  Handlers: EOS_SPEC_A_irq() and EOS_SPEC_B_irq()
//
__always_inline static void cgs_StartPulse_A( CGS_Activity activity )
{
	// start pulse must be at least one spectrometer clock period - UNKNOWN MAX
	U32 const specclk = sysclk_get_pbc_hz()/(4*CGS_CLK_RC_VALUE);
	U32 const delay   = sysclk_get_cpu_hz()/specclk  +  50; // add a bit extra to guarantee width

	eic_enable_interrupt_line(&AVR32_EIC, CGS_SPEC_A_EOS_INT_LINE);
	
  taskENTER_CRITICAL();

	gpio_set_pin_high(SPEC_START_A);
	cpu_delay_cy(delay);
	gpio_set_pin_low(SPEC_START_A);

  taskEXIT_CRITICAL();

    previous_action_A = activity;
}

__always_inline static void cgs_StartPulse_B( CGS_Activity activity )
{
	// start pulse must be at least one spectrometer clock period - UNKNOWN MAX
	U32 const specclk = sysclk_get_pbc_hz()/(4*CGS_CLK_RC_VALUE);
	U32 const delay   = sysclk_get_cpu_hz()/specclk  +  50; // add a bit extra to guarantee width

	eic_enable_interrupt_line(&AVR32_EIC, CGS_SPEC_B_EOS_INT_LINE);
	
  taskENTER_CRITICAL();

	gpio_set_pin_high(SPEC_START_B);
	cpu_delay_cy(delay);
	gpio_set_pin_low(SPEC_START_B);

  taskEXIT_CRITICAL();

    previous_action_B = activity;
}

////////////////////////////////////////////////////////////////////////////////
//
//  The timers (one for each spectrometer) are started at the end of a clearout.
//  After the specified (integration) time, the timers trigger an interrupt.
//  The handler (below) then generates the spectrometer start pulse,
//  initiating the readout.
//  The point of time when the start pulse is generated is recorded,
//  in order to obtain an accurate value for the integration time.
//

/**
 * \brief TC interrupt.
 *
 * The ISR handles RC compare interrupt on SPEC A
 */
__attribute__((__interrupt__))
static void SPEC_A_TC_irq(void)
{
    // Clear the interrupt flag. This is a side effect of reading the TC SR.
    tc_read_sr(CGS_TC, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);
    tc_stop(CGS_TC, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);

//  { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= 0x00040000; pcl_write_gplp ( 0, gplp0 ); }
    cgs_StartPulse_A(CGS_Acquiring);
    endedIntegration_ast_A = ast_get_counter_value(&AVR32_AST);
}

/**
 * \brief TC interrupt.
 *
 * The ISR handles RC compare interrupt on SPEC B
 */
__attribute__((__interrupt__))
static void SPEC_B_TC_irq(void)
{
    // Clear the interrupt flag. This is a side effect of reading the TC SR.
    tc_read_sr(CGS_TC, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);
    tc_stop(CGS_TC, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);

//  { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= 0x00400000; pcl_write_gplp ( 0, gplp0 ); }
    cgs_StartPulse_B(CGS_Acquiring);
    endedIntegration_ast_B = ast_get_counter_value(&AVR32_AST);
}

////////////////////////////////////////////////////////////////////////////////
//
//  The spectrometers generate each an EOS (End of Scan) signal when all
//  data have been passed to the ADC.
//  This signal generates an interrupt that is handled (below):
//
//  This CGS module is keeping track of what the purpose of the
//  most recent spectrometer readout was (CGS_Acquiring or CGS_Clearing).
//

__attribute__((__interrupt__))
static void EOS_SPEC_A_irq(void)
{
    eic_clear_interrupt_line(&AVR32_EIC, CGS_SPEC_A_EOS_INT_LINE);

	if ( previous_action_A == CGS_Acquiring ) {
        if ( received_EOS_A<16 ) which_EOS_A[received_EOS_A] = 'A';
        SN74V283_AddedSpectrum ( component_A, (Spec_Aux_Data_t*) 0 ); 
// FIXME
// STRT_A = begunIntegration_ast_A
// ENDD_A = endedIntegration_ast_A
	} else if ( previous_action_A == CGS_Clearing ) {
        if ( received_EOS_A<16 ) which_EOS_A[received_EOS_A] = 'c';
        SN74V283_ReportNewClearout ( component_A ); 
	} else {
        if ( received_EOS_A<16 ) which_EOS_A[received_EOS_A] = 'u';
    }

    received_EOS_A ++;
	
	if ( request_clear_A > 0
      || request_acqui_A == 0 ) {
      //  Start a clearing readout when no data acquisition is done,
	  //  or when clearouts requested (those are normally requested before acquisitions)
	  //
	  //  Disable writing to FIFO --- ALERT --- Not in hardware (yet)
	  //
      // SN74V283_WriteDisable(component_A);
      begunClearing_ast_A = ast_get_counter_value(&AVR32_AST);
	  if ( request_clear_A ) {
        request_clear_A--;
	    cgs_StartPulse_A( CGS_Clearing );
      } else {
        if ( cgs_autoClearing ) cgs_StartPulse_A( CGS_Clearing );
        else                    previous_action_A = CGS_Undefined;
      }
	} else {
      //  Start a acquisition readout when requested.
	  //  This else is only reached for request_acqui_A > 0.
	  //  The integration timer was set up in the CGS_StartSampling() function.
      //
	  //  Enable writing to the FIFO --- ALERT --- Not in hardware (yet)
	  //
      // SN74V283_WriteEnable(component_A);
      tc_start(CGS_TC, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);
      begunIntegration_ast_A = ast_get_counter_value(&AVR32_AST);
	  request_acqui_A--;
	}
}

__attribute__((__interrupt__))
static void EOS_SPEC_B_irq(void)
{
    eic_clear_interrupt_line(&AVR32_EIC, CGS_SPEC_B_EOS_INT_LINE);

	if ( previous_action_B == CGS_Acquiring ) {
        if ( received_EOS_B<16 ) which_EOS_B[received_EOS_B] = 'A';
        SN74V283_AddedSpectrum ( component_B, (Spec_Aux_Data_t*) 0 ); 
// FIXME
// STRT_B = begunIntegration_ast_B
// ENDD_B = endedIntegration_ast_B
	} else if ( previous_action_B == CGS_Clearing ) {
        if ( received_EOS_B<16 ) which_EOS_B[received_EOS_B] = 'c';
        SN74V283_ReportNewClearout ( component_B ); 
	} else {
        if ( received_EOS_B<16 ) which_EOS_B[received_EOS_B] = 'u';
	}

    received_EOS_B ++;
	
	if ( request_clear_B > 0
      || request_acqui_B == 0 ) {
      //  Start a clearing readout when no data acquisition is done,
	  //  or when clearouts requested (those are normally requested before acquisitions)
	  //
	  //  Disable writing to FIFO --- ALERT --- Not in hardware (yet)
	  //
      // SN74V283_WriteDisable(component_B);
      begunClearing_ast_B = ast_get_counter_value(&AVR32_AST);
	  if ( request_clear_B ) {
        request_clear_B--;
	    cgs_StartPulse_B( CGS_Clearing );
      } else {
        if ( cgs_autoClearing ) cgs_StartPulse_B( CGS_Clearing );
        else                    previous_action_B = CGS_Undefined;
      }
	} else {
      //  Start a acquisition readout when requested.
	  //  This else is only reached for request_acqui_B > 0.
	  //  The integration timer was set up in the CGS_StartSampling() function.
	  //
	  //  Enable writing to the FIFO --- ALERT --- Not in hardware (yet)
	  //
      // SN74V283_WriteEnable(component_B);
      tc_start(CGS_TC, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);
      begunIntegration_ast_B = ast_get_counter_value(&AVR32_AST);
	  request_acqui_B--;
	}
}

uint16_t CGS_Get_A_EOS() { return received_EOS_A; }
char*    CGS_Get_A_EOScode() { return which_EOS_A; }
uint16_t CGS_Get_B_EOS() { return received_EOS_B; }
char*    CGS_Get_B_EOScode() { return which_EOS_B; }

S16 CGS_StartSampling( spec_num_t spec, F64 inttime_ms, int num_clearouts, int num_readouts, U32 *spec_timeOfCompletion_ast ) {

    //  These are compile time constants
    //
    F64 const inttime_min_ms = ceil (1000.0*CGS_INTERVAL_MIN_RC_VALUE/CGS_INTERVAL_CLOCK_FREQUENCY_HZ);
    F64 const inttime_max_ms = floor(1000.0*CGS_INTERVAL_MAX_RC_VALUE/CGS_INTERVAL_CLOCK_FREQUENCY_HZ);

    //  Spectrometer clock: Runs at PCB (14.7456 MHz / (4*CGS_CLK_RC_VALUE) )
    //  Readout occurs at 1/8th of that clock speed.
    //  Hence:  Readout time in seconds = #pixels (plus little overhead) / Readout_Frequency
    //  Then convert to AST counts
    //
    uint32_t const clearout_duration_ast = Second_to_AST_Counter ( (10+2069) / (14745600.0/(4*CGS_CLK_RC_VALUE)/8 ) );

    //  Initialize return value
    //
    if ( spec_timeOfCompletion_ast ) *spec_timeOfCompletion_ast = 0;

    //  Make sure integration time is in acceptable interval
    //
    if ( ! ( inttime_min_ms < inttime_ms && inttime_ms < inttime_max_ms ) )  return CGS_FAIL;

    if ( SPEC_A==spec && !spec_A_enabled ) return CGS_FAIL;
    if ( SPEC_B==spec && !spec_B_enabled ) return CGS_FAIL;

    //  Convert ms to timer ticks - timer counter is 16 bit, hence the MIN/MAX restrictions
    F64 const inttime_ticks = CGS_INTERVAL_CLOCK_FREQUENCY_HZ * inttime_ms / 1000.0;

    //  Some arbitrary limits on number of clearouts and number of readouts
    //
    if      ( num_clearouts <  1 ) num_clearouts =  1;
    else if ( num_clearouts > 15 ) num_clearouts = 15;

    if      ( num_readouts <  0 ) num_readouts =  0;
    else if ( num_readouts > 15-num_clearouts ) num_readouts = 15-num_clearouts;

    //  Prepare integration time register: set RC for integration time interrupt
    //
    //  FIXME -- Do we need to make the next two assignments a critical section?
	//  What happens if right now an EOS interrupt occurs?
    //  Record the requested number of clearouts before integration.
    //  Then collect one spectrum. TODO - Allow collection of multiple spectra. - Alert: Saturation.
    //
    switch(spec) {
    case SPEC_A:
                 tc_stop    (CGS_TC, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);
                 tc_write_rc(CGS_TC, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID, (unsigned short)inttime_ticks);
			     request_clear_A = num_clearouts-1;
			     request_acqui_A = num_readouts;
                 received_EOS_A  = 0;
                 memset ( which_EOS_A, 0, 16 );
                 // SN74V283_WriteDisable(component_A);
                 eic_clear_interrupt_line(&AVR32_EIC, CGS_SPEC_A_EOS_INT_LINE);
                 if ( !cgs_autoClearing ) cgs_StartPulse_A( CGS_Clearing );
                 break;
    case SPEC_B: 
                 tc_stop    (CGS_TC, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);
                 tc_write_rc(CGS_TC, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID, (unsigned short)inttime_ticks);
			     request_clear_B = num_clearouts-1;
			     request_acqui_B = num_readouts;
                 received_EOS_B  = 0;
                 memset ( which_EOS_B, 0, 16 );
                 // SN74V283_WriteDisable(component_B);
                 eic_clear_interrupt_line(&AVR32_EIC, CGS_SPEC_B_EOS_INT_LINE);
                 if ( !cgs_autoClearing ) cgs_StartPulse_B( CGS_Clearing );
                 break;
    }

    //  Convert integration time to AST counts
    //
    uint32_t const inttime_ast = Second_to_AST_Counter ( 1000*inttime_ms );
    if ( spec_timeOfCompletion_ast )
        *spec_timeOfCompletion_ast = ast_get_counter_value(&AVR32_AST)
                                   + inttime_ast * num_readouts
                                   + clearout_duration_ast * (num_clearouts+1);
			
	return CGS_OK;
}

    
/*! \brief    Stop spectrometer clock
 */
static int16_t cgs_StopSpecClock(void)
{
    volatile avr32_tc_t *tc = CGS_TC;

    if (tc_stop(tc, CGS_CLK_TC_CHANNEL_ID) != 0)
        return CGS_FAIL;

    return CGS_OK;
}

/*! \brief  Start previously initialized  spectrometer clock
 */
static int16_t cgs_StartSpecClock(void)
{
    volatile avr32_tc_t *tc = CGS_TC;

    if (!specClock_initialized)
        return CGS_FAIL;

    if (tc_start(tc, CGS_CLK_TC_CHANNEL_ID) != 0)
        return CGS_FAIL;

    return CGS_OK;
}


/*!  \brief Prepare to generate spectrometer clock waveform.
     Clock is common to both spectrometers and must be always present when the spectrometer is powered.
     Max clock speed is 8 MHz
 */
static int16_t cgs_InitSpecClock(void)
{
    volatile avr32_tc_t *tc = CGS_TC;

    if (specClock_initialized)    // already initialized?
        return CGS_OK;

    cgs_StopSpecClock();    // ensure stopped before modifying

    // options for waveform generation
    tc_waveform_opt_t waveform_opt =
    {
        .channel  = CGS_CLK_TC_CHANNEL_ID,        // Channel selection.

        .bswtrg   = TC_EVT_EFFECT_NOOP,           // Software trigger effect on TIOB.   (NONE)
        .beevt    = TC_EVT_EFFECT_NOOP,           // External event effect on TIOB.     (NONE)
        .bcpc     = TC_EVT_EFFECT_NOOP,           // RC compare effect on TIOB.         (NONE)
        .bcpb     = TC_EVT_EFFECT_NOOP,           // RB compare effect on TIOB.         (NONE)

        .aswtrg   = TC_EVT_EFFECT_NOOP,           // Software trigger effect on TIOA.   (NONE)
        .aeevt    = TC_EVT_EFFECT_NOOP,           // External event effect on TIOA.     (NONE)
        .acpc     = TC_EVT_EFFECT_TOGGLE,         // RC compare effect on TIOA: toggle.
        .acpa     = TC_EVT_EFFECT_NOOP,           // RA compare effect on TIOA          (NONE)

        .wavsel   = TC_WAVEFORM_SEL_UP_MODE_RC_TRIGGER,      // Waveform selection: Up mode with automatic trigger on RC compare.
        .enetrg   = false,                        // External event trigger enable.
        .eevt     = TC_EXT_EVENT_SEL_TIOB_INPUT,  // External event selection.
        .eevtedg  = TC_SEL_NO_EDGE,               // External event edge selection.
        .cpcdis   = false,                        // Counter disable when RC compare.
        .cpcstop  = false,                        // Counter clock stopped with RC compare.

        .burst    = TC_BURST_NOT_GATED,           // Burst signal selection.
        .clki     = TC_CLOCK_RISING_EDGE,         // Clock inversion.
        .tcclks   = CGS_CLK_CLOCK_SOURCE
    };

    // Assign I/O to timer/counter channel pin & function.
    gpio_enable_module_pin(CGS_CLK_TC_CHANNEL_PIN, CGS_CLK_TC_CHANNEL_FUNCTION);

    // Initialize the timer/counter.
    tc_init_waveform(tc, &waveform_opt);  // Initialize the timer/counter waveform.

    // Set the compare triggers.
    tc_write_rc(tc, CGS_CLK_TC_CHANNEL_ID, CGS_CLK_RC_VALUE);    // Set RC value.

    specClock_initialized = true;    // set flag

    // Clock is now initialized but not running

    return CGS_OK;
}


static int16_t cgs_InitIntervalTimers(void) {

    volatile avr32_tc_t *tc = CGS_TC;

    // allow reinit
# ifdef DEBUG
    io_out_string("timer init\r\n");
# endif

    // ensure timers are stopped before modifying
    tc_stop(tc, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);
    tc_stop(tc, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);

    // options for waveform generation
    tc_waveform_opt_t waveform_opt =
    {
        .channel  = CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID,   // Channel selection.

        .bswtrg   = TC_EVT_EFFECT_NOOP,           // Software trigger effect on TIOB.   (NONE)
        .beevt    = TC_EVT_EFFECT_NOOP,           // External event effect on TIOB.     (NONE)
        .bcpc     = TC_EVT_EFFECT_NOOP,           // RC compare effect on TIOB.         (NONE)
        .bcpb     = TC_EVT_EFFECT_NOOP,           // RB compare effect on TIOB.         (NONE)

        .aswtrg   = TC_EVT_EFFECT_NOOP,           // Software trigger effect on TIOA.   (NONE)
        .aeevt    = TC_EVT_EFFECT_NOOP,           // External event effect on TIOA.     (NONE)
        .acpc     = TC_EVT_EFFECT_CLEAR,          // RC compare effect on TIOA: toggle.
        .acpa     = TC_EVT_EFFECT_SET,            // RA compare effect on TIOA          (NONE)

        .wavsel   = TC_WAVEFORM_SEL_UP_MODE_RC_TRIGGER,      // Waveform selection: Up mode with automatic trigger on RC compare.
        .enetrg   = false,                        // External event trigger enable.
        .eevt     = TC_EXT_EVENT_SEL_TIOB_INPUT,  // External event selection.
        .eevtedg  = TC_SEL_NO_EDGE,               // External event edge selection.
        .cpcdis   = true,                        // Counter disable when RC compare.
        .cpcstop  = false,                        // Counter clock stopped with RC compare.

        .burst    = TC_BURST_NOT_GATED,           // Burst signal selection.
        .clki     = TC_CLOCK_RISING_EDGE,         // Clock inversion.
        .tcclks   = CGS_INTERVAL_CLOCK_SOURCE
    };

    // Options for enabling TC interrupts
    static const tc_interrupt_t tc_interrupt = {
        .etrgs = 0,
        .ldrbs = 0,
        .ldras = 0,
        .cpcs  = 1, // Enable interrupt on RC compare alone
        .cpbs  = 0,
        .cpas  = 0,
        .lovrs = 0,
        .covfs = 0
    };


    // Assign I/O to timer/counter channel pin & function. 
    gpio_enable_module_pin(CGS_SPEC_A_INTERVAL_TC_CHANNEL_PIN, CGS_SPEC_A_INTERVAL_TC_CHANNEL_FUNCTION);
    gpio_enable_module_pin(CGS_SPEC_B_INTERVAL_TC_CHANNEL_PIN, CGS_SPEC_B_INTERVAL_TC_CHANNEL_FUNCTION);

    // Initialize the timer/counter.
    tc_init_waveform(tc, &waveform_opt);  // Initialize the timer/counter waveform SPEC A
    waveform_opt.channel = CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID;    // same options for spec B
    tc_init_waveform(tc, &waveform_opt);  // Initialize the timer/counter waveform for SPEC B


    // Set the compare triggers to minimums (fastest readout)
    tc_write_ra(tc, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID, 1);    // Set RA value to minimum
    tc_write_rc(tc, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID, CGS_INTERVAL_MIN_RC_VALUE);    // Set RC value to minimum
    tc_write_ra(tc, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID, 1);    // Set RA value to minimum
    tc_write_rc(tc, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID, CGS_INTERVAL_MIN_RC_VALUE);    // Set RC value to minimum

    // monitor timer on the output pins - goes high on first tick, low on compare match

    // configure the timer interrupts
    tc_configure_interrupts(tc, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID, &tc_interrupt);
    tc_configure_interrupts(tc, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID, &tc_interrupt);

    // Interval timers are now initialized but not running
    timerAinit = true;
    timerBinit = true;

    return CGS_OK;
}

static int16_t cgs_StopIntervalTimers(void) {

    volatile avr32_tc_t *tc = CGS_TC;

    tc_stop(tc, CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID);
    tc_stop(tc, CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID);

    return CGS_OK;
}


static void cgs_RegisterInterrupts(void) {

	static bool interrupts_registered = false;

	if (!interrupts_registered) {

		//! Structure holding the configuration parameters of the EIC (External Interrupt Controller) module.
		eic_options_t eic_options[CGS_EIC_INTERRUPTS];

		int i;

		// End-of-Scan (EOS) from spectrometers - falling edge triggered interrupts
		gpio_enable_pin_pull_up ( CGS_SPEC_A_EOS_PIN);				// not strictly needed, but safer
		eic_options[0].eic_line = CGS_SPEC_A_EOS_INT_LINE;			// Set the interrupt line number.
		eic_options[0].eic_mode = EIC_MODE_EDGE_TRIGGERED;			// Enable edge-triggered interrupt.
		eic_options[0].eic_edge = EIC_EDGE_FALLING_EDGE;			// Interrupt will trigger on falling edge
		eic_options[0].eic_filter = EIC_FILTER_ENABLED;				// Enable filtering to avoid spurious or multiple interrupts - can we live with the lag?
		eic_options[0].eic_async = EIC_SYNCH_MODE;					// Initialize in synchronous mode : interrupt is synchronized to the clock
		gpio_enable_module_pin(CGS_SPEC_A_EOS_PIN, CGS_SPEC_A_EOS_PIN_FUNCTION);  // Map the interrupt line to the GPIO pin
		
		gpio_enable_pin_pull_up ( CGS_SPEC_B_EOS_PIN);				// not strictly needed, but safer
		eic_options[1].eic_line = CGS_SPEC_B_EOS_INT_LINE;			// Set the interrupt line number.
		eic_options[1].eic_mode = EIC_MODE_EDGE_TRIGGERED;			// Enable edge-triggered interrupt.
		eic_options[1].eic_edge = EIC_EDGE_FALLING_EDGE;			// Interrupt will trigger on falling edge
		eic_options[1].eic_filter = EIC_FILTER_ENABLED;				// Enable filtering to avoid spurious or multiple interrupts - can we live with the lag?
		eic_options[1].eic_async = EIC_SYNCH_MODE;					// Initialize in synchronous mode : interrupt is synchronized to the clock
		gpio_enable_module_pin(CGS_SPEC_B_EOS_PIN, CGS_SPEC_B_EOS_PIN_FUNCTION);  // Map the interrupt line to the GPIO pin


		// Disable all interrupts.
		Disable_global_interrupt();

		INTC_register_interrupt((__int_handler)&SPEC_A_TC_irq, AVR32_TC0_IRQ1, AVR32_INTC_INT3);   // highest priority interrupt
		INTC_register_interrupt((__int_handler)&SPEC_B_TC_irq, AVR32_TC0_IRQ2, AVR32_INTC_INT3);   // highest priority interrupt

		INTC_register_interrupt((__int_handler)&EOS_SPEC_A_irq, CGS_SPEC_A_EOS_IRQ, CGS_SPEC_A_EOS_PRIORITY);
		INTC_register_interrupt((__int_handler)&EOS_SPEC_B_irq, CGS_SPEC_B_EOS_IRQ, CGS_SPEC_B_EOS_PRIORITY);

		// Init the EIC controller with the options
		eic_init ( &AVR32_EIC, eic_options, CGS_EIC_INTERRUPTS );

		// Disable EIC interrupts.
        // They will be enabled as soon as StartPulse gets sent to the spectrometer.
        //
		for (i = 0; i < CGS_EIC_INTERRUPTS; i++) {
			eic_enable_line           (&AVR32_EIC, eic_options[i].eic_line);
			eic_disable_interrupt_line(&AVR32_EIC, eic_options[i].eic_line);
		}

		// Enable all interrupts.
		Enable_global_interrupt();

		interrupts_registered = true;
	}
}

static void cgs_DisableEICInterrupts(void) {

    //  Turn off EIC interrupt handling
    //
    eic_disable_interrupt_line(&AVR32_EIC, CGS_SPEC_A_EOS_INT_LINE);
    eic_disable_interrupt_line(&AVR32_EIC, CGS_SPEC_B_EOS_INT_LINE);
}

void CGS_Initialize(bool runA, bool runB, bool autoClearing)
{
    //  Supply power to the ADCs
    //
    if(runA) ad7677_Start(component_A);
    if(runB) ad7677_Start(component_B);

    vTaskDelay((portTickType)TASK_DELAY_MS(500));
	
    // Register timer and EOS interrupts, but don't enable them
    //
    cgs_RegisterInterrupts();

    // Initialize spectrometer clock,
    // then start it
    //
    cgs_InitSpecClock();
    cgs_StartSpecClock();

    // Initialize the timers, don't start them.
    // Used to generate start pulses for specified integration times.
    // They are started when an integration is requested.
    // First, the ongoing clearout is completed, which ends with an EOS.
    // The EOS interrupt handler then starts the timer,
    // and when the timer generates the interrupt (after the integration interval),
    // the timer interrupt handler generates the start pulse.
    //
    cgs_InitIntervalTimers();

    if ( autoClearing ) {
	  if(runA) cgs_StartPulse_A( CGS_Clearing );  //  EOS interrupt enables here
	  if(runB) cgs_StartPulse_B( CGS_Clearing );  //  EOS interrupt enables here
    }

    cgs_autoClearing = autoClearing;

	//  Reset FIFOs
	//
	if( runA ) SN74V283_Start(component_A);
	if( runB ) SN74V283_Start(component_B);

    vTaskDelay((portTickType)TASK_DELAY_MS(500));

    return;
}

void CGS_DeInitialize(bool runA, bool runB)
{
    //  Stop the timers from firing.
    //  This call may interfere with an already running timer,
    //  if spectrometers are stopped during integration.
    //  But: Better stop in the middle,
    //  than get an unexpected timer triggering an interrupt.
    //
    cgs_StopIntervalTimers();

    //  Stop the spectrometers.
    //
    cgs_StopSpecClock();

    //  Interrupts are NOT deregistered.
    //  Once they are available in firmware,
    //  they remain available.
    //  However, the EOS interrupt lines will be disabled at this point.
    //
    cgs_DisableEICInterrupts();

	//  Do NOT Reset FIFOs!

    //  Power down ADCs & bring into reset mode
    //
    ad7677_StopBoth();
}

//void CGS_StartPulse( bool A, bool B ) {
//  if(A) cgs_StartPulse( SPEC_A, CGS_Clearing );
//  if(B) cgs_StartPulse( SPEC_B, CGS_Clearing );
//}


static void cgs_Spec_A_Power_On ( void ) { gpio_set_pin_high (SPEC_A_ON); vTaskDelay((portTickType)TASK_DELAY_MS(500)); spec_A_enabled = true; }
static void cgs_Spec_A_Power_Off( void ) { gpio_set_pin_low  (SPEC_A_ON); spec_A_enabled = false; }
static bool cgs_Spec_A_Is_Power_On( void ) { return gpio_get_pin_value(SPEC_A_ON); }
static void cgs_Spec_B_Power_On ( void ) { gpio_set_pin_high (SPEC_B_ON); vTaskDelay((portTickType)TASK_DELAY_MS(500)); spec_B_enabled = true; }
static void cgs_Spec_B_Power_Off( void ) { gpio_set_pin_low  (SPEC_B_ON); spec_B_enabled = false; }
static bool cgs_Spec_B_Is_Power_On( void ) { return gpio_get_pin_value(SPEC_B_ON); }

static void cgs_16V_On ( void ) { gpio_set_pin_high (P16V_ON); vTaskDelay((portTickType)TASK_DELAY_MS(20)); }
static void cgs_16V_Off( void ) { gpio_set_pin_low  (P16V_ON); }
static bool cgs_Is_16V_On( void ) { return gpio_get_pin_value(P16V_ON); }


void CGS_Power_On( bool A, bool B ) {
  if ( !cgs_Is_16V_On() ) { cgs_16V_On(); }							// turn on 16V supply if necessary

  if ( A && !cgs_Spec_A_Is_Power_On() ) cgs_Spec_A_Power_On ();		// turn on spec A if requested
  if ( B && !cgs_Spec_B_Is_Power_On() ) cgs_Spec_B_Power_On ();		// turn on spec B if requested
}

void CGS_Power_Off() {

  cgs_Spec_A_Power_Off();
  cgs_Spec_B_Power_Off();

  cgs_16V_Off();
}

void CGS_InitGpio() {

  //  Power to spectrometer control

  //  Ensure 16V supply (spec power) is OFF
  //
  gpio_configure_pin( P16V_ON,   GPIO_DIR_OUTPUT | GPIO_INIT_LOW );

  //  Ensure spectrometers are OFF
  //
  gpio_configure_pin( SPEC_A_ON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );
  gpio_configure_pin( SPEC_B_ON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW );

  //  Spectrometer clock pin
  //
  gpio_configure_pin(SPEC_CLK, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);

#if (MCU_BOARD_REV==REV_A)   
  gpio_configure_pin(SPEC_START, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
#else
  gpio_configure_pin(SPEC_START_A, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
  gpio_configure_pin(SPEC_START_B, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
#endif  

  
}
