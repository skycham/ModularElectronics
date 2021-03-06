#include "i2c_connection.h"
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_emu.h"
#include "uart_connection.h"	//for i2c_Scan
#include <stdio.h>
#include "RTC_.h"



void initI2C_Master(void)
{
  CMU_ClockEnable(cmuClock_I2C0,true);

  I2C0->CTRL = 0;

  int i;

  /* Initialize I2C driver for the device on the DK, using standard rate. */
  /* Devices on DK itself supports fast mode, */
  /* but in case some slower devices are added on */
  /* prototype board, we use standard mode. */
  //I2C_Init_TypeDef i2cInit = {true, true, 0, I2C_FREQ_FAST_MAX, i2cClockHLRFast};
  I2C_Init_TypeDef i2cInit = {true, true, 0, I2C_FREQ_STANDARD_MAX, i2cClockHLRStandard};

  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(cmuClock_I2C0, true);

  /* Output value must be set to 1 to not drive lines low. Set */
    /* SCL first, to ensure it is high before changing SDA. */
  GPIO_PinModeSet(I2CDRV_SCL_PORT, I2CDRV_SCL_PIN, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(I2CDRV_SDA_PORT, I2CDRV_SDA_PIN, gpioModeWiredAndPullUp, 1);

  /* In some situations (after a reset during an I2C transfer), the slave */
  /* device may be left in an unknown state. Send 9 clock pulses just in case. */
  for (i = 0; i < 9; i++)
  {
      /*
       * TBD: Seems to be clocking at appr 80kHz-120kHz depending on compiler
       * optimization when running at 14MHz. A bit high for standard mode devices,
       * but DK only has fast mode devices. Need however to add some time
       * measurement in order to not be dependable on frequency and code executed.
       */
    GPIO_PinOutSet(I2CDRV_SCL_PORT, I2CDRV_SCL_PIN);
    GPIO_PinOutClear(I2CDRV_SCL_PORT, I2CDRV_SCL_PIN);
  }

    /* Enable pins at config location (3 is default which is the location used on the DK) */
  I2C0->ROUTE = I2C_ROUTE_SDAPEN |
                I2C_ROUTE_SCLPEN |
                (I2CDRV_PORT_LOCATION << _I2C_ROUTE_LOCATION_SHIFT);

  I2C_Init(I2C0, &i2cInit);

}

//---------------I2C SLAVE----------------------
void initI2C_Slave(void)
{
  // Using default settings
  I2C_Init_TypeDef i2cInit = {true, true, 0, I2C_FREQ_FAST_MAX, i2cClockHLRStandard};

  /* Using PD6 (SDA) and PD7 (SCL) */
  GPIO_PinModeSet(I2CDRV_SCL_PORT_SLAVE, I2CDRV_SCL_PIN_SLAVE, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(I2CDRV_SDA_PORT_SLAVE, I2CDRV_SDA_PIN_SLAVE, gpioModeWiredAndPullUp, 1);

  GPIO_PinOutSet(I2CDRV_SCL_PORT, I2CDRV_SCL_PIN);//only if this pin is set as input pulled and filtered

  /* Enable pins at location 1 */
  I2C0->ROUTE = I2C_ROUTE_SDAPEN |
                I2C_ROUTE_SCLPEN |
                (I2CDRV_PORT_LOCATION_SLAVE << _I2C_ROUTE_LOCATION_SHIFT);

  /* Initializing the I2C */
  I2C_Init(I2C0, &i2cInit);

  /* Setting the status flags and index */
  i2c_rxInProgress = false;
  i2c_startTx = false;
  i2c_rxBufferIndex = 0;

  /* Setting up to enable slave mode */
  I2C0->SADDR = I2C_SLAVE_ADDRESS;
  //I2C0->SADDRMASK = 0xFE;
  I2C0->CTRL |= I2C_CTRL_SLAVE | I2C_CTRL_AUTOACK | I2C_CTRL_AUTOSN;
  enableI2cSlaveInterrupts();
}

void enableI2cSlaveInterrupts(void){
  I2C_IntClear(I2C0, I2C_IEN_ADDR | I2C_IEN_RXDATAV | I2C_IEN_SSTOP);
  I2C_IntEnable(I2C0, I2C_IEN_ADDR | I2C_IEN_RXDATAV | I2C_IEN_SSTOP);
  NVIC_EnableIRQ(I2C0_IRQn);
}
void disableI2cSlaveInterrupts(void){
  NVIC_DisableIRQ(I2C0_IRQn);
  I2C_IntDisable(I2C0, I2C_IEN_ADDR | I2C_IEN_RXDATAV | I2C_IEN_SSTOP);
  I2C_IntClear(I2C0, I2C_IEN_ADDR | I2C_IEN_RXDATAV | I2C_IEN_SSTOP);
}

void I2C0_IRQHandler(void)
{
  disableRtcInterrupts();
  static bool slave_address_ok=false;
  static bool read_data_command=false;
  static int register_to_send = 0;

  int status;
  status = I2C0->IF;

  uint32_t data;

  if (status & I2C_IF_ADDR){
    /* Address Match */
    /* Indicating that reception is started */
	data = I2C0->RXDATA;

	uint8_t tab_addr = (data>>1);
	uint32_t tab_bit_addr = (1<<(uint32_t)(tab_addr&0x1F));
	if (data&0x01) {//read request indicate that the i2c slave exists.
		slavesList[tab_addr>>5]	|= tab_bit_addr;
	}
	/*else {
		slavesList[tab_addr>>5]	&= ~tab_bit_addr;
	}*/
	slavesListCheck[tab_addr>>5]|= tab_bit_addr;

    if(data==I2C_SLAVE_ADDRESS){
		i2c_rxInProgress = true;
		I2C0->RXDATA;
		slave_address_ok=true;
	}
    else if (data==I2C_SLAVE_ADDRESS+1){ //read request
    	i2c_rxInProgress = true;
    	I2C0->RXDATA;

    	if(i2c_rxBuffer[0]<I2C_REG_BUFFER_SIZE){
    		register_to_send=0;
    		I2C0->TXDATA = i2c_registers[i2c_rxBuffer[i2c_rxBufferIndex-1]+register_to_send]; //send data
    		read_data_command=true;
    		slave_address_ok=true;
    	}
    }
	else {
		i2c_rxInProgress = false;
		I2C0->RXDATA;
		slave_address_ok=false;
	}

    I2C_IntClear(I2C0, I2C_IFC_ADDR);

  } else if (status & I2C_IF_RXDATAV){
    /* Data received */
	if(slave_address_ok && !read_data_command){
		if(i2c_rxBufferIndex<I2C_RXBUFFER_SIZE){
			i2c_rxBuffer[i2c_rxBufferIndex] = I2C0->RXDATA;
			i2c_rxBufferIndex++;
		}
	}
  }

	if (read_data_command){
		register_to_send++;
		for (int i=0;i<100000;i++){
			if(true || i2c_rxBuffer[i2c_rxBufferIndex-1]+register_to_send < I2C_REG_BUFFER_SIZE){
				if((I2C0->IF)&I2C_IF_TXC){
					I2C0->TXDATA = i2c_registers[i2c_rxBuffer[i2c_rxBufferIndex-1]+register_to_send];
					register_to_send++;
					I2C_IntClear(I2C0, I2C_IF_TXC);
				}
				if ((I2C0->IF) & I2C_IEN_SSTOP){
					break;
				}
			}
		}
	}

  if(status & I2C_IEN_SSTOP){
    I2C_IntClear(I2C0, I2C_IEN_SSTOP);
    slave_address_ok=false;
    read_data_command=false;
    i2c_rxInProgress=false;
    register_to_send=0;
  }

  if (i2c_slave_address!=0x00)
	  enableRtcInterrupts();
}

//void I2C0_IRQHandler(void){
  /* Just run the I2C_Transfer function that checks interrupts flags and returns */
  /* the appropriate status */
//	I2C_Status = I2C_Transfer(I2C0);
//}
int i2c_RegisterGet(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t *val)
{
  I2C_TransferSeq_TypeDef seq;
  uint8_t i2c_write_data[1];

  seq.addr = addr;
  seq.flags = I2C_FLAG_WRITE_READ;
  /* Select register to be read */
  i2c_write_data[0] = reg;
  seq.buf[0].data = i2c_write_data;
  seq.buf[0].len = 1;

  /* Select location/length to place register */
   seq.buf[1].data = val;
   seq.buf[1].len = 1;

  /* Do a polled transfer */
  I2C_Status = I2C_TransferInit(i2c, &seq);
  uint32_t timeout = I2CDRV_TRANSFER_TIMEOUT;
  while (I2C_Status == i2cTransferInProgress && timeout--)
  {
    /* Enter EM1 while waiting for I2C interrupt */
    //EMU_EnterEM1();
    /* Could do a timeout function here. */
	I2C_Status = I2C_Transfer(I2C0);
  }
  if(timeout==(uint32_t)(-1)){
  	  uart_sendText("\nERROR: I2C_get_timeout\n");
  }

  if (I2C_Status != i2cTransferDone)
  {
    return((int)I2C_Status);
  }

  return ((int) 1);
}
int i2c_RegisterSet(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint16_t  val)
{
  I2C_TransferSeq_TypeDef seq;
  uint8_t data[3];

  seq.addr = addr;
  seq.flags = I2C_FLAG_WRITE;
  /* Select register to be written */
  data[0] = ((uint8_t)reg);
  seq.buf[0].data = data;
  if (true)		//this means to read only 1 byte
  {
    /* Only 1 byte reg */
    data[1] = (uint8_t)val;
    seq.buf[0].len = 2;
  }
  else
  {
    data[1] = (uint8_t)(val >> 8);
    data[2] = (uint8_t)val;
    seq.buf[0].len = 3;
  }

  uint32_t timeout = I2CDRV_TRANSFER_TIMEOUT;
  /* Do a polled transfer */
  I2C_Status = I2C_TransferInit(i2c, &seq);
  while (I2C_Status == i2cTransferInProgress && timeout--)
  {
    /* Enter EM1 while waiting for I2C interrupt */
	  I2C_Status = I2C_Transfer(I2C0);
    /* Could do a timeout function here. */
  }

  if(timeout==0)
	  uart_sendText("I2C_set_timeout\n");

  return(I2C_Status);
}

int i2c_Register_Write_Block (I2C_TypeDef *i2c,uint8_t addr, uint8_t reg, uint8_t length, uint8_t *data)
{
  I2C_TransferSeq_TypeDef seq;

  seq.addr = addr;
  seq.flags = I2C_FLAG_WRITE_WRITE;
  seq.buf[0].data = &reg;
  seq.buf[0].len = 1;
  seq.buf[1].data = data;
  seq.buf[1].len = length;

  uint32_t timeout = I2CDRV_TRANSFER_TIMEOUT;
  /* Do a polled transfer */
  I2C_Status = I2C_TransferInit(i2c, &seq);
  while (I2C_Status == i2cTransferInProgress && timeout--)
  {
    /* Enter EM1 while waiting for I2C interrupt */
	  I2C_Status = I2C_Transfer(I2C0);
    /* Could do a timeout function here. */
  }

  if(timeout==0)
	  uart_sendText("I2C_set_timeout\n");

  return(I2C_Status);
}

bool i2c_Detect    (I2C_TypeDef *i2c, uint8_t addr)
{
  I2C_TransferSeq_TypeDef    seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t                    i2c_read_data[8];
  uint8_t                    i2c_write_data[2];

  /* Unused parameter */
  (void) i2c;

  seq.addr  = addr;
  seq.flags = I2C_FLAG_WRITE_READ;
  /* Select command to issue */
  i2c_write_data[0] = 0;
  i2c_write_data[1] = 0;
  seq.buf[0].data   = i2c_write_data;
  seq.buf[0].len    = 1;
  /* Select location/length of data to be read */
  seq.buf[1].data = i2c_read_data;
  seq.buf[1].len  = 8;

  uint32_t timeout = I2CDRV_TRANSFER_TIMEOUT;
  /* Do a polled transfer */
  ret = I2C_TransferInit(I2C0, &seq);
  while (ret == i2cTransferInProgress && timeout--)
  {
      ret = I2C_Transfer(I2C0);
	  //UART_sendChar('e');
	  //EMU_EnterEM1();
  }

  if (ret != i2cTransferDone)
  {
    return(false);
  }

  return(true);
}

void i2c_Scan (I2C_TypeDef *i2c){
	int i=0;
	for (;i<128;i++){
		if (i2c_Detect(i2c,i*2)==1){
			char buff[30];
			sprintf(buff,"Detected I2C device: %d %02x\n",i*2,i);
			uart_sendText(buff);
		}
	}
}


int i2c_Register_Read_Block (I2C_TypeDef *i2c,uint8_t addr, uint8_t reg, uint8_t length, uint8_t *data)
{
  I2C_TransferSeq_TypeDef    seq;
  //I2C_TransferReturn_TypeDef ret;
  uint8_t i2c_write_data[1];
  /* Unused parameter */
  (void) i2c;

  seq.addr  = addr;
  seq.flags = I2C_FLAG_WRITE_READ;
  /* Select register to start reading from */
  i2c_write_data[0] = reg;
  seq.buf[0].data = i2c_write_data;
  seq.buf[0].len  = 1;
  /* Select length of data to be read */
  seq.buf[1].data = data;
  seq.buf[1].len  = length;

  uint32_t timeout = I2CDRV_TRANSFER_TIMEOUT;
  /* Do a polled transfer */
  I2C_Status = I2C_TransferInit(i2c, &seq);
  while (I2C_Status == i2cTransferInProgress && timeout--)
  {
  	  I2C_Status = I2C_Transfer(I2C0);
  }

  if(timeout==(uint32_t)(-1))
  	  uart_sendText("I2C_set_timeout\n");
  if (I2C_Status != i2cTransferDone)
  {
    return((int) I2C_Status);
  }

  return((int) length);
}
