/*
 * TestBenchMaster.c
 *
 *  Created on: Oct 19, 2018
 *      Author: dave
 */

#include "TestBenchMaster.h"
#include "Config.h"
#include "AS1.h"
#include "XF1.h" // xsprintf
#include "Shell.h"
#include "SpiHandler.h"

/* global variables, only used in this file */
static uint8_t uartReceiveBufferHW[NOF_BYTES_UART_RECEIVE_BUFFER];
static uint8_t uartNewReceivedBytes[NOF_BYTES_UART_RECEIVE_BUFFER];


/* prototypes of local functions */
static void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer);

/*!
* \fn testBenchMaster_TaskEntry(void* p)
* \brief TestBench Master to test the different Routing Algorithms
*/
void testBenchMaster_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.TestBenchMasterTaskInterval);
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */
	char infoBuf[100];
	uint16_t nofBytesInUARTbuffer;

	static bool toggle = false;
	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */

		receivedUARTbyteForTesting(&nofBytesInUARTbuffer);
		if(uartNewReceivedBytes[0])
		{
			for(int i = 0; i<nofBytesInUARTbuffer; i++)
			{
				pushToByteQueue(MAX_14830_DEVICE_SIDE,0,uartNewReceivedBytes[i]);
			}

			XF1_xsprintf(infoBuf, uartNewReceivedBytes);
			pushMsgToShellQueue(infoBuf);
			XF1_xsprintf(infoBuf, "\n");
			pushMsgToShellQueue(infoBuf);
		}

	}
}


/*!
* \fn  testBenchMaster_TaskInit(void)
*/
void testBenchMaster_TaskInit(void)
{
	testBenchMaster_setUARTreceiveBuffer();
}


static void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer)
{
	static uint8_t uartReceiveBufferHWOld[NOF_BYTES_UART_RECEIVE_BUFFER];
	uint16_t newDataIndex = 0;

	for(int i = 0 ; i<NOF_BYTES_UART_RECEIVE_BUFFER; i++)
	{
		if(uartReceiveBufferHWOld[i] != uartReceiveBufferHW[i])
		{
			uartNewReceivedBytes[newDataIndex]=uartReceiveBufferHW[i];
			newDataIndex ++;
		}
		uartReceiveBufferHWOld[i] = uartReceiveBufferHW[i];
		//uartReceiveBufferHW[i] = 0;
	}
	for(int i = newDataIndex; i< NOF_BYTES_UART_RECEIVE_BUFFER; i++)
	{
		uartNewReceivedBytes[i]=0;
	}
	*nofBytesInBuffer = newDataIndex;
}

/*!
* \fn  testBenchMaster_TaskInit(void)
* \brief set the HW Buffer for the incomming UART Bytes
*/
void testBenchMaster_setUARTreceiveBuffer(void)
{
	AS1_ReceiveBlock(AS1_DeviceData,uartReceiveBufferHW,NOF_BYTES_UART_RECEIVE_BUFFER);
}

