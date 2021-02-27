/*! \file i2c.c *************************************************************
 *
 * \brief I2C Master GPIO bit-banging driver
 *
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-19
 *
 **********************************************************************************/

#include "compiler.h"
#include "cycle_counter.h"
#include "gpio.h"

#include "i2c.h"
#include "i2c_cfg.h"


// Driver parameters
static U32 fCPU;


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

/* \brief Initialize I2C driver.
 * @param fCPU	CPU clock frequency
 */
void I2C_init(U32 f_CPU)
{
	// Save CPU frequency
	fCPU = f_CPU;
}


/* \brief Generate a start condition
 */
void I2C_start(void)
{
	// Ensure SDA & SCL high
	gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);
	gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

	// Ensure bus free time
	cpu_delay_us(I2C_TBUF, fCPU);

	// SDA Falling edge
	gpio_clr_gpio_open_drain_pin(I2C_SDA_PIN);

	// Ensure start hold time
	cpu_delay_us(I2C_THDSTA, fCPU);

	// SCL Falling edge
	gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);
}


/* \brief Generate a repeated start condition
 */
void I2C_rep_start(void)
{
	// Ensure SDA & SCL high
	gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);
	gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

	// Ensure set-up time
	cpu_delay_us(I2C_TSUSTA, fCPU);

	// SDA Falling edge
	gpio_clr_gpio_open_drain_pin(I2C_SDA_PIN);

	// Ensure start hold time
	cpu_delay_us(I2C_THDSTA, fCPU);

	// SCL Falling edge
	gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);
}



/* \brief Put SCL line to Hi (Hi-Z)
 */
void I2C_SCLHi(void)
{
	gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);
}


/* \brief Return state of SCL line
 */
Bool I2C_SCL(void)
{
	return gpio_get_pin_value(I2C_SCL_PIN);
}


/* \brief Write a byte to I2C bus
 * @param data	Byte to be sent
 * @param expectack A expect ack flag. If true will generate an extra clock for slave ACK bit.
 * @return TRUE: Transmission was acknowledged FALSE: Transmission was not acknwoledged
 */
Bool I2C_write(U8 data, Bool expectack)
{
	U8 i;
	Bool ack;
	U8 mask = 0x80;

	// Send byte
	for(i=0; i<8; i++)
	{
		// Ensure previous bit hold time
		cpu_delay_us(I2C_THDDAT, fCPU);

		// Write bit
		if(data & mask)
			gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);
		else
			gpio_clr_gpio_open_drain_pin(I2C_SDA_PIN);

		// Ensure min clock low time
		#if I2C_TLOW > I2C_THDDAT
		cpu_delay_us(I2C_TLOW - I2C_THDDAT, fCPU);
		#endif

		// Clock rising edge (slave latches data)
		gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

		// Ensure clock high time
		cpu_delay_us(I2C_THIGH, fCPU);

		// Clock falling edge
		gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);

		// Shift masking bit
		mask >>= 1;
	}

	// Receive ACK
	if(expectack)
	{
		// Free data bus (in case last bit written was a 0)
		gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);

		// Ensure min clock low
		cpu_delay_us(I2C_TLOW, fCPU);

		// Clock rising edge
		gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

		// Read ACK
		ack = !gpio_get_pin_value(I2C_SDA_PIN);

		// Ensure clock high time
		cpu_delay_us(I2C_THIGH, fCPU);

		// Clock falling edge
		gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);

		return ack;
	}
	else
		return FALSE;
}


/* \brief Read a byte from I2C bus
 * @param ack If true an ACK pulse is sent after byte is read. If false a NACK is sent after byte is read.
 * @return Byte read
 */
U8 I2C_read(Bool ack)
{
	U8 i;
	U8 mask = 0x80;
	U8 data = 0;

	// Free bus
	gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);

	// Read byte
	for(i=0; i<8; i++)
	{
		// Clock rising edge
		gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

		// Read bit
		if(gpio_get_pin_value(I2C_SDA_PIN))
			data |= mask;

		// Ensure min  clock high time
		cpu_delay_us(I2C_THIGH, fCPU);

		// Clock falling edge (slave may write more data)
		gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);

		// Ensure clock low time
		cpu_delay_us(I2C_TLOW, fCPU);

		// Shift masking bit
		mask >>= 1;
	}

	// Send ACK/NACK
	if(ack)
		gpio_clr_gpio_open_drain_pin(I2C_SDA_PIN);

	// Clock rising edge
	gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

	// Ensure min clock high
	cpu_delay_us(I2C_THIGH, fCPU);

	// Clock falling edge
	gpio_clr_gpio_open_drain_pin(I2C_SCL_PIN);

	// Ensure clock low time
	cpu_delay_us(I2C_TLOW, fCPU);

	return  data;
}



/* \brief Generate a stop condition
 */
void I2C_stop(void)
{
	// Ensure SDA low
	gpio_clr_gpio_open_drain_pin(I2C_SDA_PIN);

	// SCL rising edge
	gpio_set_gpio_open_drain_pin(I2C_SCL_PIN);

	// Ensure stop set-up time
	cpu_delay_us(I2C_TSUSTO, fCPU);

	// SDA rising edge
	gpio_set_gpio_open_drain_pin(I2C_SDA_PIN);

}
