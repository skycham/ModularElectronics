#include "em_device.h"
#include "em_system.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_emu.h"

#define DEBUG

#include "ucPeripheralDrivers\uart_connection.h"
#include "ucPeripheralDrivers\spi_connection.h"
#include "ucPeripheralDrivers\RTC_.h"

#include "sensorsDrivers\AS3953.h"

#include <stdio.h>

#define SPI_PORTC     gpioPortC // USART1 (location #0) MISO and MOSI are on PORTD
#define SPI_PORTB     gpioPortB // USART1 (location #0) SS and SCLK are on PORTC
#define SPI_MISO_PIN  1  // PC1
#define SPI_MOSI_PIN  0  // PC0
#define SPI_CS_PIN    8  // PB8
#define SPI_SCLK_PIN  7  // PB7

void clockTest() {
	long int i=0;
	for(;i<120001L;++i) {
	  if(i==120000L)
		  GPIO_PortOutSet(gpioPortF, 0x4);
	  if(i==60000L)
		  GPIO_PortOutClear(gpioPortF, 0x4);
	}
}

void clockTest_short() {
	long int i=0;
	for(;i<12001L;++i) {
	  if(i==12000L)
		  GPIO_PortOutSet(gpioPortF, 0x4);
	  if(i==6000L)
		  GPIO_PortOutClear(gpioPortF, 0x4);
	}
}

void initOscillators(void)
{
	  CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_CORELEDIV2);  // select HFCORECLK/2 as clock source to LFB
	  CMU_ClockEnable(cmuClock_CORELE, true);                  // enable the Low Energy Peripheral Interface clock

	  CMU_ClockEnable(cmuClock_HFPER, true);
	  CMU_ClockEnable(cmuClock_GPIO, true);                    // enable GPIO peripheral clock
}

void init_uart_interface(void){
	/* UART interface initialization */
	struct UART_Settings uartSettings;
	uartSettings.uart_com_port=gpioPortD;
	uartSettings.uart_tx_pin=7;
	uartSettings.uart_rx_pin=6;
	uartSettings.uart_port_location=2;
	uartSettings.uart_speed=115200;

	uart_init(uartSettings);
}

void init_spi_interface(void){
	/* SPI interface initialization */
	struct SPI_Settings spiSettings;
	spiSettings.spi_miso_port=gpioPortC;
	spiSettings.spi_miso_pin= 1;
	spiSettings.spi_mosi_port=gpioPortC;
	spiSettings.spi_mosi_pin= 0;
	spiSettings.spi_sclk_port=gpioPortB;
	spiSettings.spi_sclk_pin= 7;
	spiSettings.spi_location=0;

	spi_init(spiSettings);
}

/* Debug functions for AS3953 */
void Test_AS3953()
{
	uint8_t uid[4];
	uint8_t conf[4];
	uint8_t lock[4];

	for (int i=0;i<2;i++){
		AS3953_Read_UID(uid);
		AS3953_Read_Lock(lock);
		AS3953_Read_Conf(conf);

		init_uart_interface();

		char buff[30];
		sprintf(buff,"\nuid: %d %d %d %d\n",uid[0],uid[1],uid[2],uid[3]);
		uart_sendText(buff);
		sprintf(buff,"conf: %d %d %d %d\n",conf[0],conf[1],conf[2],conf[3]);
		uart_sendText(buff);
		sprintf(buff,"lock: %d %d %d %d\n",lock[0],lock[1],lock[2],lock[3]);
		uart_sendText(buff);
		for (int i=0;i<20;i++) { clockTest_short(); }	//wait for the UART transmition to finished

		init_spi_interface();
	}
}
void Print_AS3953_Registers()
{
	uint8_t reg[0x12];
	for (int i=0;i<0x12;i++)
	{
		reg[i]=AS3953_Read_Register(i);
		clockTest_short();
	}

	init_uart_interface();

	uart_sendChar('\n'); uart_sendChar('\n'); uart_sendChar('\n');

	char buff[30];
	for (int i=0;i<0x12;i++)
	{
		sprintf(buff,"[0x%02x]:0x%02x\t",i,reg[i]);
		uart_sendText(buff);
		//wait for the UART transmition to finished
	}
	for (int i=0;i<20;i++) { clockTest_short(); }

	init_spi_interface();
}
void AS3953_PrintStatus(AS3953_PICC_AFE_PowerStatus_t AS3953_PiccAfe_PowerStatus, AS3953_Status_t AS3953Status)
{
	uart_sendText("\nAFE state: \t");
	switch(AS3953_PiccAfe_PowerStatus)
	{
	case PICC_AFE_OFF:
		uart_sendText("PICC_AFE_OFF");
		break;
	case PICC_AFE_ON:
		uart_sendText("PICC_AFE_ON");
		break;
	}

	uart_sendText("\nPICC Logic state: \t");
	switch(AS3953Status)
	{
	case POWER_OFF:
		uart_sendText("POWER_OFF\n");
		break;
	case IDLE:
		uart_sendText("IDLE\n");
		break;
	case READY:
		uart_sendText("READY\n");
		break;
	case ACTIVE:
		uart_sendText("ACTIVE\n");
		break;
	case HALT:
		uart_sendText("HALLT\n");
		break;
	case READYX:
		uart_sendText("READYX\n");
		break;
	case ACTIVEX:
		uart_sendText("ACTIVEX\n");
		break;
	case L4:
		uart_sendText("L4\n");
		break;
	}
}
void AS3953_PrintFifoStatus(uint8_t RxNumber, uint8_t TxNumber, AS3953_FifoErrors_t AS3953_FifoError)
{
	char buff[30];
	sprintf(buff, "Rx: %d, Tx: %d\t", RxNumber, TxNumber);
	uart_sendText(buff);

	if (AS3953_FifoError.Fifo_Underflow == 1)
	{
		uart_sendText("Underflow\t");
	}
	if (AS3953_FifoError.Fifo_Overflow == 1)
	{
		uart_sendText("Overflow\t");
	}
	if (AS3953_FifoError.Last_Fifo_NotComplete == 1)
	{
		uart_sendText("Last_Fifo_NotComplete\t");
	}
	if (AS3953_FifoError.ParityBitMissing == 1)
	{
		uart_sendText("ParityBitMissing\t");
	}

	uart_sendChar('\n');
}
void AS3953_PrintRATS(uint8_t RATS_FSDI_BitNumber, uint8_t RATS_CID_BitNumber)
{
	char buff[30];
	sprintf(buff,"RATS: \tFSDI %d\tCID %d\n",RATS_FSDI_BitNumber,RATS_CID_BitNumber);
	uart_sendText(buff);
}

void RTC_IRQHandler(void)
{
#ifdef DEBUG
	/*Read and print registers values */
	Print_AS3953_Registers();
#endif

	/*--------------------------------------------------*/
	/* 				Read AS3953 information 			*/
	/*--------------------------------------------------*/
	AS3953_Status_t AS3953Status;
	AS3953_PICC_AFE_PowerStatus_t AS3953_PiccAfe_PowerStatus;
	AS3953_Status(&AS3953_PiccAfe_PowerStatus, &AS3953Status);

	uint8_t RxNumber = AS3953_FifoRxStatus();
	AS3953_FifoErrors_t AS3953_FifoError;
	uint8_t TxNumber = AS3953_FifoTxStatus(&AS3953_FifoError);

	uint8_t RATS_FSDI_BitNumber;
	uint8_t RATS_CID_BitNumber;
	AS3953_RATS(&RATS_FSDI_BitNumber, &RATS_CID_BitNumber);

	/*--------------------------------------------------*/
	/* 				Print AS3953 information 			*/
	/*--------------------------------------------------*/
	init_uart_interface();

	AS3953_PrintStatus(AS3953_PiccAfe_PowerStatus, AS3953Status);
	AS3953_PrintFifoStatus(RxNumber, TxNumber, AS3953_FifoError);
	AS3953_PrintRATS(RATS_FSDI_BitNumber, RATS_CID_BitNumber);

	init_spi_interface();

	/* Clear RTC interrupts */
	RTC_clearInt();
}

int main ()
{
	  CHIP_Init();
	  initOscillators();

	  /* UART test */
	  init_uart_interface();

	  /* Spi initialization */
	  init_spi_interface();

	  for (int i=0;i<20;i++) { clockTest_short(); }	//long wait

	  /* AS3953 initialization */
	  AS3953_Setting_t AS3953_Setting;
	  AS3953_Setting.spi_cs_port=  gpioPortB;
	  AS3953_Setting.spi_cs_pin =  8;
	  AS3953_Setting.conf_word[0]=0x26;		//	{ 0x20, 0x7E, 0xE7, 0x80 } 14443-4		??
	  AS3953_Setting.conf_word[1]=0x7E;
	  AS3953_Setting.conf_word[2]=0x8f;
	  AS3953_Setting.conf_word[3]=0x80;
	  AS3953_Init(AS3953_Setting);

	  for (int i=0;i<20;i++) { clockTest_short(); }

	  Test_AS3953();

	  //RTC initialization
	  RTC_init();
	  RTC_setTime(500);
	  RTC_enableInt();

	  while(1){
		  EMU_EnterEM2(false);
	  }
}
