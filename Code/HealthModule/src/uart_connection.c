#include "uart_connection.h"
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"

void initUART(void){
	CMU_ClockEnable(cmuClock_USART1, true);   // enable USART1 peripheral clock

	USART_InitAsync_TypeDef uartInit =
	  {	//230400
	    .enable = usartDisable,     // wait to enable transmitter and receiver
	    .refFreq = 0,               // setting refFreq to 0 will invoke the CMU_ClockFreqGet() function and measure the HFPER clock
	    .baudrate = 115200,         // desired baud rate
	    .oversampling = usartOVS16, // set oversampling to x16
	    .databits = usartDatabits8, // 8 data bits
	    .parity = usartNoParity,    // no parity bits
	    .stopbits = usartStopbits1, // 1 stop bit
	    .mvdis = false,             // use majority voting
	    .prsRxEnable = false,       // not using PRS input
	    .prsRxCh = usartPrsRxCh0,   // doesn't matter what channel we select
	 };
	 USART_InitAsync(USART1, &uartInit);      // apply configuration to USART1
	 USART1->ROUTE = USART_ROUTE_RXPEN | USART_ROUTE_TXPEN | USART_ROUTE_LOCATION_LOC0; // clear buffers, enable transmitter and receiver pins

	 USART_IntClear(USART1, _USART_IF_MASK);  // clear all USART interrupt flags
	 NVIC_ClearPendingIRQ(USART1_RX_IRQn);    // clear pending RX interrupt flag in NVIC
	 NVIC_ClearPendingIRQ(USART1_TX_IRQn);    // clear pending TX interrupt flag in NVIC

	 USART_Enable(USART1, usartEnable);       // enable transmitter and receiver
}
void uart_sendChar(char c){
	while(!(USART1->STATUS & (1 << 6)));   // wait for TX buffer to empty
	USART1->TXDATA = c; 			       // send character
}
void uart_sendText(char * text){
	int i=0;
	uint8_t len=150;
	for (;i<len;++i){
		if(text[i]==0) break;
		while(!(USART1->STATUS & (1 << 6)));   // wait for TX buffer to empty
		USART1->TXDATA = text[i];       // send character
	}
}
