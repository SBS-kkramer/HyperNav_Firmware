/*
 * pressure.c
 *
 * Created: 10/08/2016 4:13:30 PM
 *  Author: rvandommelen / Scott Feener
 *
 * History:
	2017-04-21, SKF: Updates for rev B PCB
 */

#include "stdio.h"
#include "gpio.h"
#include "intc.h"
#include "tc.h"
#include "sysclk.h"
#include "qdec.h"
#include "power_clocks_lib.h"
#include "pevc.h"
#include <math.h>
#include <compiler.h>
# include "FreeRTOS.h"
# include "task.h"
# include "queue.h"
#include "SatlanticHardware.h"
#include "errorcodes.h"
#include "io_funcs.spectrometer.h"

#include "pressure.h"


//sysclk_get_pba_hz()
#define QDEC_CLK_FREQ_HZ	3686400		// would like it to be FOSC0
#define QDEC_CLK_FREQ_MHZ	3.6864		// in MHz


volatile avr32_pevc_t         *ppevc  = &AVR32_PEVC;

// Instance of QDEC0 and QDEC1
volatile avr32_qdec_t* qdec0 = &AVR32_QDEC0;
volatile avr32_qdec_t* qdec1 = &AVR32_QDEC1;

static Bool		QDEC0captured = false;
static U32		QDEC0capturetime = 0;
static Bool		QDEC1captured = false;
static U32		QDEC1capturetime = 0;

static U32 Tstart = 0;
static U32 Pstart = 0;
static int PcountsCapture = 0;
static int TcountsCapture = 0;

static digiquartz_t T_currently_sampling = DIGIQUARTZ_Nothing;
static digiquartz_t P_currently_sampling = DIGIQUARTZ_Nothing;

// Options for QDEC timer Mode
static const qdec_timer_opt_t QUADRATURE_TIMER_OPT =
{
	.upd      = false,				// Up/Down Mode Timer Disabled.
	.tsdir    = QDEC_TSIR_UP,       // Count up Timer.
	.filten   = false,              // Disable filtering.
	.evtrge   = false,              // Enable event triggering.
	.rcce     = false,              // Disable Position Compare.
	.pcce     = false               // Disable Revolution Compare.
};

// Options for QDEC Interrupt Management
static const qdec_interrupt_t QDEC_INTERRUPT =
{
	.cmp    = 0,                      // Enable Compare Interrupt.
	.cap    = 1,                      // Enable Capture Interrupt.
	.pcro   = 0,                      // Disable Position Roll-over Interrupt.
	.rcro   = 0                       // Disable Counter Roll-over Interrupt.
};



__attribute__((__interrupt__))
static void QDEC0_capture_int_handler(void)
{
	// clear QDEC0 interrupt flag
	qdec0->scr = 0x000003FF;		// clear all

	//PEVC_CHANNELS_DISABLE_TRIGGER_INTERRUPT(ppevc, 1<<AVR32_PEVC_ID_USER_QDEC0_CAPT );
	//PEVC_CHANNELS_DISABLE_TRIGGER_INTERRUPT(ppevc, 1<<AVR32_PEVC_ID_GEN_PAD_12 );

	// disable the interrupt so it doesn't fire on next edge
	pevc_channels_disable(ppevc, 1<<AVR32_PEVC_ID_USER_QDEC0_CAPT);

	// clear the interrupt to prevent re-firing
	pevc_channel_clear_trigger_interrupt(ppevc, AVR32_PEVC_ID_USER_QDEC0_CAPT);

	QDEC0captured = true;

	// read the QDEC0 capture time
	QDEC0capturetime = qdec0->cap;

	// read current counter value
	PcountsCapture = tc_read_tc(P_T_TC, P_T_FREQ_COUNTER_CHANNEL_ID);
}

__attribute__((__interrupt__))
static void QDEC1_capture_int_handler(void)
{
	// clear QDEC1 interrupt flag
	qdec1->scr = 0x000003FF;		// clear all

	// disable the interrupt so it doesn't fire on next edge
	pevc_channels_disable(ppevc, 1<<AVR32_PEVC_ID_USER_QDEC1_CAPT);

	// clear the interrupt to prevent re-firing
	pevc_channel_clear_trigger_interrupt(ppevc, AVR32_PEVC_ID_USER_QDEC1_CAPT);

	QDEC1captured = true;

	// read the QDEC1 capture time
	QDEC1capturetime = qdec1->cap;

	// read current counter value
	TcountsCapture = tc_read_tc(P_T_TC, T_FREQ_COUNTER_CHANNEL_ID);
}

void PRES_InitGpio() {

  gpio_configure_pin(PRES_ON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);			// ensure pressure sensor is OFF
  gpio_configure_pin(PEN_N, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH );			// disable pressure passthrough buffer
  gpio_configure_pin(TEN_N, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH );			// disable temperature passthrough buffer
  gpio_configure_pin(P_T_INPUT_PIN, GPIO_DIR_INPUT);					//
  gpio_configure_pin(T_FREQ, GPIO_DIR_INPUT);
  //test
  //gpio_configure_pin(TWIS0_TWCK, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
  //while (1) gpio_toggle_pin(TWIS0_TWCK);
}


void PRES_power(bool on)
{
	if (on) {
		gpio_set_gpio_pin( PRES_ON );
	} else {
		gpio_clr_gpio_pin( PRES_ON );
	}

	// disable buffer passthrough for both P and T reading by default
	gpio_set_gpio_pin( PEN_N );
	gpio_set_gpio_pin( TEN_N );
}


digiquartz_t PRES_GetCurrentMeasurement(digiquartz_t sensor) {

	if (sensor == DIGIQUARTZ_TEMPERATURE)
		return T_currently_sampling;
	if (sensor == DIGIQUARTZ_PRESSURE)
		return P_currently_sampling;

	return DIGIQUARTZ_Nothing;
}


F64 PRES_Calc_Pressure_dBar(F64 t_period, F64 p_period, DigiQuartz_Calibration_Coefficients_t const* coeffs, F64 *TdegC, F64 *Ppsia) {
    if ( !coeffs ) {
        if ( TdegC ) *TdegC = 0;
        if ( Ppsia ) *Ppsia = 0;
        return 0;
    }

    // Calculate temperature:
    F64 const  U = t_period - coeffs->U0;

    F64 const  Usq = U*U;
    F64 const  Ucu = U*Usq;
    F64 const  Uqd = Usq*Usq;

    F64 const  Temperature = coeffs->Y1*U + coeffs->Y2*Usq + coeffs->Y3*Ucu;

    F64 const  C  = coeffs->C1 + coeffs->C2*U + coeffs->C3*Usq;
    F64 const  D  = coeffs->D1 + coeffs->D2*U;
    F64 const  T0 = coeffs->T1 + coeffs->T2*U + coeffs->T3*Usq + coeffs->T4*Ucu + coeffs->T5*Uqd;

    // Calculate pressure in psia
    F64 const  Ratio = ( T0 / p_period );
    F64 const  RatioSq = Ratio*Ratio;
    F64 const  Pressure = C * (1-RatioSq) * (1 - D*(1-RatioSq) );

    /*io_out_F64("T: %.3lf C\r\n", *TdegC);
    io_out_F64("P: %.3lf psia\r\n", *Ppsia);
    io_out_F64("P: %.3lf decibar\r\n", *Ppsia * 0.6894757);*/

    if ( TdegC ) *TdegC = Temperature;
    if ( Ppsia ) *Ppsia = Pressure;

    return Pressure*0.6894757;    // convert psia to dBar
}


/*! \brief	Initializes the Digiquartz pressure sensor driver resources
 *  \return     0 for success, 1 for failure
 */
S16 PRES_Initialize(void)
{
	volatile avr32_tc_t *tc = P_T_TC;

	// use QDEC modules as 32 bit timer/counters
	// enable the QDEC clock(s)
  	sysclk_enable_peripheral_clock(&AVR32_QDEC0);
	sysclk_enable_peripheral_clock(&AVR32_QDEC1);

	// The generic clock must be used in timing mode and less than half the CLK_QDEC; from datasheet:
	//
	//    GCLK_QDEC is used for filtering in QDEC mode and is the timer clock in Timer Mode. It is a
	//    generic clock generated by the Power Manager. The programmer must configure the Power
	//    Manager to enable GCLK_QDEC. The GCLK_QDEC frequency must less than half the
	//    CLK_QDEC clock frequency.
	//
	// CLK_QDEC = fPBA = 1/4 main = (1/4)*(4*FoSC0) = FOSC0 = 14.7456 MHz

	// Setup the generic clock for QDEC0 and QDEC1 - use OCS0, which should be 14.7456 MHz; divide by 4 to ensure we are < 0.5*CLK_QDEC
	//
	scif_gc_setup(AVR32_SCIF_GCLK_QDEC0, SCIF_GCCTRL_OSC0, AVR32_SCIF_GC_DIV_CLOCK, 4);
	scif_gc_enable(AVR32_SCIF_GCLK_QDEC0);      // Now enable the generic clock

	if (0 != scif_gc_setup(AVR32_SCIF_GCLK_QDEC1, SCIF_GCCTRL_OSC0, AVR32_SCIF_GC_DIV_CLOCK, 4)) {
		//portDBG_TRACE("error configuring QDEC1 clock");
		return 1;
	}
	if (0 != scif_gc_enable(AVR32_SCIF_GCLK_QDEC1)) {    // Now enable the generic clock
		//portDBG_TRACE("error enabling QDEC1 clock");
		return 1;
	}

	// setup GPIOs
    //
	static const gpio_map_t TC_GPIO_MAP =
	{
		{ AVR32_PEVC_PAD_EVT_12_1_PIN, AVR32_PEVC_PAD_EVT_12_1_FUNCTION },  // assign GPIO for pressure event trigger
		{ AVR32_PEVC_PAD_EVT_11_PIN,   AVR32_PEVC_PAD_EVT_11_FUNCTION   },  // assign GPIO for temperature event trigger
		{ P_T_FREQ_CLK_PIN, P_T_FREQ_CLK_FUNCTION },                        // assign I/O for pressure frequency input counting
		{   T_FREQ_CLK_PIN,   T_FREQ_CLK_FUNCTION }                         // assign I/O for temperature frequency input counting
	};

	// Assign I/Os to timer/counter.
        //
	gpio_enable_module(TC_GPIO_MAP, sizeof(TC_GPIO_MAP) / sizeof(TC_GPIO_MAP[0]));

	tc_capture_opt_t counter_opt = {
		.channel  = P_T_FREQ_COUNTER_CHANNEL_ID,  // Channel selection.

		.ldrb     = TC_SEL_NO_EDGE,               // RB loading selection.
		.ldra     = TC_SEL_NO_EDGE,               // RA loading selection.

		.cpctrg   = TC_NO_TRIGGER_COMPARE_RC,     // RC compare trigger enable.
		.abetrg   = TC_EXT_TRIG_SEL_TIOA,         // TIOA or TIOB external trigger selection.
		.etrgedg  = TC_SEL_NO_EDGE,               // External trigger edge selection.

		.ldbdis   = false,                        // Counter clock disable with RB loading.
		.ldbstop  = false,                        // Counter clock stopped with RB loading. - so we can detect overflow

		.burst    = TC_BURST_NOT_GATED,           // Burst signal selection.
		.clki     = TC_CLOCK_RISING_EDGE,         // Clock inversion.
		.tcclks   = TC_CLOCK_SOURCE_XC0           // external freq used as clock
	};

	// Initialize the frequency counter.
        //
	tc_init_capture(tc, &counter_opt);
	tc_select_external_clock(tc, P_T_FREQ_COUNTER_CHANNEL_ID, TC_CH0_EXT_CLK0_SRC_TCLK0);

	counter_opt.channel = T_FREQ_COUNTER_CHANNEL_ID;    // switch to temperature channel
	counter_opt.tcclks = TC_CLOCK_SOURCE_XC1;           // external freq used as clock
	tc_init_capture(tc, &counter_opt);
	tc_select_external_clock(tc, T_FREQ_COUNTER_CHANNEL_ID, TC_CH1_EXT_CLK1_SRC_TCLK1);

	// enable the PEVC clock
    //
	sysclk_enable_peripheral_clock(&AVR32_PEVC);

	// setup PEVC so that QDEC counter time can be captured
	// PEVC Event Shaper options.
    //
	static const pevc_evs_opt_t PEVC_EVS_OPTIONS =
	{
		.igfdr = 0x0A,            // Set the IGF clock (don't care here).
		.igf = PEVC_EVS_IGF_OFF,  // Input Glitch Filter off
		.evf = PEVC_EVS_EVF_OFF,   // Enable Event on falling edge
		.evr = PEVC_EVS_EVR_ON    // Enable Event on rising edge
	};

	// Configuring the PEVC path:
	//  Change on pevc input pin12 event -> QDEC0 capture
        //
	if(FAIL == pevc_channel_configure(ppevc, AVR32_PEVC_ID_USER_QDEC0_CAPT, AVR32_PEVC_ID_GEN_PAD_12, &PEVC_EVS_OPTIONS))
	{
		return 1;
	}

	// add peripheral event for temperature signal
	if(FAIL == pevc_channel_configure(ppevc, AVR32_PEVC_ID_USER_QDEC1_CAPT, AVR32_PEVC_ID_GEN_PAD_11, &PEVC_EVS_OPTIONS))
	{
		return 1;
	}

	// PEVC channel(s) now configured but not enabled


	Disable_global_interrupt();
	// Register the QDEC0 capture interrupt to the interrupt controller.
	INTC_register_interrupt(&QDEC0_capture_int_handler, AVR32_QDEC0_IRQ, AVR32_INTC_IPR_INTLEVEL_INT3);

	// Register the QDEC1 capture interrupt to the interrupt controller.
	INTC_register_interrupt(&QDEC1_capture_int_handler, AVR32_QDEC1_IRQ, AVR32_INTC_IPR_INTLEVEL_INT3);

	Enable_global_interrupt();

	T_currently_sampling = DIGIQUARTZ_Nothing;
	P_currently_sampling = DIGIQUARTZ_Nothing;

    return 0;
}



S16 PRES_StartMeasurement(digiquartz_t sensor)
{
	if (sensor == DIGIQUARTZ_PRESSURE) {
		if (P_currently_sampling != DIGIQUARTZ_Nothing)		// Failed: Already sampling
			return PRES_Failed;

		gpio_clr_gpio_pin( PEN_N );							// allow pressure signal to reach MCU
		P_currently_sampling = DIGIQUARTZ_PRESSURE;			// set running flag

	} else if (sensor == DIGIQUARTZ_TEMPERATURE) {

		if (T_currently_sampling != DIGIQUARTZ_Nothing)		// Failed: Already sampling
			return PRES_Failed;

		gpio_clr_gpio_pin( TEN_N );							// allow temperature signal to reach MCU
		T_currently_sampling = DIGIQUARTZ_TEMPERATURE;		// set running flag

		//portDBG_TRACE("T measurement started");

	} else if ( sensor == DIGIQUARTZ_Nothing ) {
		//  Failed: Invalid sensor
		return PRES_Failed;
		//  TODO - Reset sampling
	}

	// stop the timer capture
	tc_stop(P_T_TC, (sensor == DIGIQUARTZ_PRESSURE) ? P_T_FREQ_COUNTER_CHANNEL_ID : T_FREQ_COUNTER_CHANNEL_ID);

	// stop the QDEC timer
	qdec_stop((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1);

	// Initialize 32-bit counter to 0
	qdec_write_rc_cnt((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, 0);
	qdec_write_pc_cnt((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, 0);

	// Set the top values at maximum
	qdec_write_rc_top((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, 0xffff);
	qdec_write_pc_top((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, 0xffff);

	// Initialize the QDEC in timer mode.
	qdec_init_timer_mode((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, &QUADRATURE_TIMER_OPT);

	// Configure the QDEC interrupts.
	qdec_configure_interrupts((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1, &QDEC_INTERRUPT);

	// reset the QDEC capture flag and capture time
	if (sensor == DIGIQUARTZ_PRESSURE) {
		QDEC0captured = false;
		QDEC0capturetime = 0;
	}
	else {
		QDEC1captured = false;
		QDEC1capturetime = 0;
	}

	// Start the QDEC (software trigger or peripheral event is required).
	qdec_software_trigger((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1);

// 	U32 count;
// 	while (1) {
// 		count = ((sensor == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1) -> cnt;
// 		io_print_U32("%lu\t", count);
// 		io_print_U32("%lu\r\n", count/3686400);
// 	}

	AVR32_ENTER_CRITICAL_REGION();

		// wait for a rising edge by polling the pin so we have time to set up peripheral event and counter
		//
		while (gpio_get_pin_value((sensor == DIGIQUARTZ_PRESSURE) ? P_T_INPUT_PIN : T_FREQ))
		//io_out_string("0");

		while (!gpio_get_pin_value((sensor == DIGIQUARTZ_PRESSURE) ? P_T_INPUT_PIN : T_FREQ));		// wait for a high pin state
		//io_out_string("1");

		// now we have a full period before the next rising edge, should be plenty of time to get things set up

		// Enable the appropriate PEVC channel - capture QDECx counter on rising edge
		PEVC_CHANNELS_ENABLE(ppevc, (sensor == DIGIQUARTZ_PRESSURE) ? (1<<AVR32_PEVC_ID_USER_QDEC0_CAPT) : (1<<AVR32_PEVC_ID_USER_QDEC1_CAPT));

		// enable the counter
		tc_start(P_T_TC, (sensor == DIGIQUARTZ_PRESSURE) ? P_T_FREQ_COUNTER_CHANNEL_ID : T_FREQ_COUNTER_CHANNEL_ID);		// enable counter

	AVR32_LEAVE_CRITICAL_REGION();

	// we are now prepared to count pulses

	// now wait for first edge...
	//U32 ticks = 0;
	while ( !((sensor == DIGIQUARTZ_PRESSURE) ? QDEC0captured : QDEC1captured) ) {
// 		ticks++;
// 		io_out_string("*");
// 		io_print_U32("wait count: %lu\r", (U32)tc_read_tc(P_T_TC, (sensor == DIGIQUARTZ_PRESSURE) ? P_T_FREQ_COUNTER_CHANNEL_ID : T_FREQ_COUNTER_CHANNEL_ID));
	}
	//io_print_U32("waited %lu \r\n", ticks);

	if (sensor == DIGIQUARTZ_PRESSURE) {
		Pstart = QDEC0capturetime;
// 		PstartCounts = PcountsCapture;
	}
	else {
		Tstart = QDEC1capturetime;
//		TstartCounts = TcountsCapture;
	}

	//io_print_U32("Start %lu \r\n", QDEC0capturetime);
	//io_print_U32("Start %lu \r\n", (sensor == DIGIQUARTZ_PRESSURE) ? QDEC0capturetime : QDEC1capturetime);
	//io_print_U32("Count %lu \r\n", (U32)countsCapture);
	//io_print_U32("Count %lu \r\n", (sensor == DIGIQUARTZ_PRESSURE) ? (U32)PcountsCapture : (U32)TcountsCapture);

	return PRES_Ok;
}

S16 PRES_GetPerioduSecs(digiquartz_t sensortype, digiquartz_t *sensor, F64 *period_usecs, int* counts, U32* duration, int16_t frequency_divisor )
{
//	int startcounts;
	U32 starttime;

	if (sensortype == DIGIQUARTZ_PRESSURE) {
		if (P_currently_sampling == DIGIQUARTZ_PRESSURE) {
//			startcounts = PstartCounts;
			starttime = Pstart;
			// reset the QDEC0 capture flag and capture time
			QDEC0captured = false;
			QDEC0capturetime = 0;
		}
		else
			return PRES_Failed;
	}

	else if (sensortype == DIGIQUARTZ_TEMPERATURE) {
		if (T_currently_sampling == DIGIQUARTZ_TEMPERATURE) {
//			startcounts = TstartCounts;
			starttime = Tstart;
			// reset the QDEC1 capture flag and capture time
			QDEC1captured = false;
			QDEC1capturetime = 0;
		}
		else
			return PRES_Failed;
	}

	else {
		return PRES_Failed;
	}

  AVR32_ENTER_CRITICAL_REGION();

	while ( gpio_get_pin_value( (sensortype == DIGIQUARTZ_PRESSURE) ? P_T_INPUT_PIN : T_FREQ ) );		// wait for a  low pin state
	while (!gpio_get_pin_value( (sensortype == DIGIQUARTZ_PRESSURE) ? P_T_INPUT_PIN : T_FREQ ) );		// wait for a high pin state

	// Enable the PEVC channel  - capture QDECx counter on rising edge
	PEVC_CHANNELS_ENABLE(ppevc, (sensortype == DIGIQUARTZ_PRESSURE) ? (1<<AVR32_PEVC_ID_USER_QDEC0_CAPT) : (1<<AVR32_PEVC_ID_USER_QDEC1_CAPT) );

  AVR32_LEAVE_CRITICAL_REGION();

	//U32 ticks = 0;
	while (!((sensortype == DIGIQUARTZ_PRESSURE)?QDEC0captured:QDEC1captured)) {
		//ticks++;
		//io_out_string("*");
		//io_print_U32("wait count: %lu\r", (U32)tc_read_tc(P_T_TC, (sensor == DIGIQUARTZ_PRESSURE) ? P_T_FREQ_COUNTER_CHANNEL_ID : T_FREQ_COUNTER_CHANNEL_ID)));
	}

	U32 endtime   = (sensortype == DIGIQUARTZ_PRESSURE) ? QDEC0capturetime : QDEC1capturetime;
	int endcounts = (sensortype == DIGIQUARTZ_PRESSURE) ? PcountsCapture   : TcountsCapture;
	//io_print_U32("waited %lu \r\n", ticks);

	// stop the QDEC timer
	qdec_stop((sensortype == DIGIQUARTZ_PRESSURE) ? qdec0 : qdec1);

//	int diffcounts = endcounts - startcounts;		// startcounts always 0?
	int diffcounts = endcounts;
	U32 difftime = endtime - starttime;

	if ( sensor )   *sensor   = ((sensortype == DIGIQUARTZ_PRESSURE) ? P_currently_sampling : T_currently_sampling);
	if ( counts )   *counts   = diffcounts;
	if ( duration ) *duration = difftime;

	//	io_print_U32("time %lu \t", difftime);
	//	io_print_U32("cnts %lu \r\n", (U32)diffcounts);

	if ( period_usecs ) {
		*period_usecs = ((F64)difftime/(F64)diffcounts)/QDEC_CLK_FREQ_MHZ;

		// Adjust for external hardware frequency divider
		*period_usecs /= frequency_divisor;
	}

	// 	char tmp[100] = {0};
	// 	snprintf(tmp, sizeof(tmp), "time: %lu to %lu (%lu ticks or %.6lf s)\r\n", starttime, endtime, difftime, (F64)difftime/(F64)QDEC_CLK_FREQ_HZ);
	// 	io_out_string(tmp);
	// 	snprintf(tmp, sizeof(tmp), "per: %.6lf usecs freq: %.6lf Hz\r\n", *period_usecs, 1000000.0/(*period_usecs));
	// 	io_out_string(tmp);
	if (sensortype == DIGIQUARTZ_PRESSURE)
		P_currently_sampling = DIGIQUARTZ_Nothing;
	else
		T_currently_sampling = DIGIQUARTZ_Nothing;

	return PRES_Ok;
}
