/*
 * TestBenchMaster.c
 *
 *  Created on: Oct 19, 2018
 *      Author: dave
 */

#include "TestBenchMaster.h"
#include "AS1.h"
#include "XF1.h" // xsprintf
#include "Shell.h"
#include "SpiHandler.h"

/* global variables, only used in this file */
static uint8_t uartReceiveBufferHW[NOF_BYTES_UART_RECEIVE_BUFFER];
static uint8_t uartNewTestDataBytes[NOF_BYTES_UART_RECEIVE_BUFFER];
static uint32_t time_ms = 0;
static tTestBenchByteBufferEntry sendBuffer[NUMBER_OF_UARTS][NOF_BYTES_UART_SEND_BUFFER_TESTBENCH];

static uint32_t nofLostBytes[NUMBER_OF_UARTS];
static uint32_t receivedBytesCounter[NUMBER_OF_UARTS];
static float averageLatency[NUMBER_OF_UARTS];

/* prototypes of local functions */
static void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer);
static void updateTime(void);
static void putByteIntoTestBenchSendBuffer(uint8_t databyte, uint8_t uartNr);
static bool getByteFromTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr,uint64_t* byteLatency);
static void updateAverageLatency(uint64_t latencyCurrentByte,uint8_t uartNr);
static void updateTestResults(void);
static uint16_t getLostPackages(uint8_t uartNr);

/*!
* \fn testBenchMaster_TaskEntry(void* p)
* \brief TestBench Master to test the different Routing Algorithms
* The UAV Switches are connected like the Picture below
* 		 ___________________________________________________
* 		| UAV Switch TestBench Master						|
* 		|  DeviceSide						ModemSide		|
* 		|___________________________________________________|
* 			  | | | |						  | | | |
* 		 _____|_|_|_|_______			 _____|_|_|_|_______
* 		|	DUT UAV Switch	|-----------|	 UAV Switch 	|
* 		|					|-----------|	 Modem Simulator|
* 		|			Modem	|-----------|	Modem			|
* 		|			Side	|-----------|	Side			|
* 		|___________________|			|___________________|
*
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
		uint16_t incommingBytes = 0, outgoningBytes = 0;

		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		updateTime();

		//Receive the UART input to use for the Routing Algorthm TestBench (Test Data)
		receivedUARTbyteForTesting(&nofBytesInUARTbuffer);


		if(uartNewTestDataBytes[0] || numberOfBytesInRxByteQueue(MAX_14830_WIRELESS_SIDE, 1))
		{
			for(int i = 0; i<nofBytesInUARTbuffer; i++)
			{
				outgoningBytes++;
				pushToByteQueue(MAX_14830_DEVICE_SIDE,1,&uartNewTestDataBytes[i]);
				pushToByteQueue(MAX_14830_DEVICE_SIDE,0,&uartNewTestDataBytes[i]);
				putByteIntoTestBenchSendBuffer(uartNewTestDataBytes[i],1);
			}

			while(numberOfBytesInRxByteQueue(MAX_14830_WIRELESS_SIDE, 1))
			//for(int i = 0; i<numberOfBytesInRxByteQueue(MAX_14830_WIRELESS_SIDE, 1); i++)
			{
				uint64_t latency;
				uint8_t dataByte;
				incommingBytes++;

				if(popFromByteQueue(MAX_14830_WIRELESS_SIDE,1,&dataByte) == pdTRUE)
				{
					pushToByteQueue(MAX_14830_DEVICE_SIDE,3,&dataByte);

					if(getByteFromTestBenchSendBuffer(&dataByte,1,&latency))
					{
						updateAverageLatency(latency,1);
					}
				}
			}

			XF1_xsprintf(infoBuf, "\nOutgoingBytes %u   IncommingBytes %u ",outgoningBytes,incommingBytes);
			pushMsgToShellQueue(infoBuf);
			updateTestResults();

		}

	}
}


/*!
* \fn  testBenchMaster_TaskInit(void)
*/
void testBenchMaster_TaskInit(void)
{
	testBenchMaster_setUARTreceiveBuffer();
	for(int i = 0; i<NOF_BYTES_UART_SEND_BUFFER_TESTBENCH; i++)
	{
		for(int j = 0 ; j<NUMBER_OF_UARTS ; j++)
		{
			sendBuffer[j][i].isEmpty = true;
			sendBuffer[j][i].byteData = 0x00;
			sendBuffer[j][i].sendingTimestamp = 0xFFFFFFFF;
		}
	}
}


static void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer)
{
	static uint8_t uartReceiveBufferHWOld[NOF_BYTES_UART_RECEIVE_BUFFER];
	uint16_t newDataIndex = 0;

	uint16_t currentIndexInUARTrecFIFO = AS1_GetReceivedDataNum(AS1_DeviceData);
	static uint16_t oldIndexInUARTrecFIFO = 0;

	if(oldIndexInUARTrecFIFO<currentIndexInUARTrecFIFO)
	{
		for(int i = oldIndexInUARTrecFIFO  ; i<currentIndexInUARTrecFIFO; i++)
		{
			uartNewTestDataBytes[newDataIndex]=uartReceiveBufferHW[i];
			newDataIndex ++;
		}
	}
	else if(oldIndexInUARTrecFIFO>currentIndexInUARTrecFIFO)
	{
		for(int i = oldIndexInUARTrecFIFO  ; i<NOF_BYTES_UART_RECEIVE_BUFFER; i++)
		{
			uartNewTestDataBytes[newDataIndex]=uartReceiveBufferHW[i];
			newDataIndex ++;
		}
		for(int i = 0 ; i<currentIndexInUARTrecFIFO; i++)
		{
			uartNewTestDataBytes[newDataIndex]=uartReceiveBufferHW[i];
			newDataIndex ++;
		}
	}


	for(int i = newDataIndex; i< NOF_BYTES_UART_RECEIVE_BUFFER; i++)
	{
		uartNewTestDataBytes[i]=0;
	}

	oldIndexInUARTrecFIFO = currentIndexInUARTrecFIFO;
	*nofBytesInBuffer = newDataIndex;
//-------------
//	for(int i = 0 ; i<NOF_BYTES_UART_RECEIVE_BUFFER; i++)
//	{
//		if(uartReceiveBufferHWOld[i] != uartReceiveBufferHW[i])
//		{
//			uartNewTestDataBytes[newDataIndex]=uartReceiveBufferHW[i];
//			newDataIndex ++;
//		}
//		uartReceiveBufferHWOld[i] = uartReceiveBufferHW[i];
//		//uartReceiveBufferHW[i] = 0;
//	}
//	for(int i = newDataIndex; i< NOF_BYTES_UART_RECEIVE_BUFFER; i++)
//	{
//		uartNewTestDataBytes[i]=0;
//	}
//	*nofBytesInBuffer = newDataIndex;
}

/*!
* \fn  testBenchMaster_TaskInit(void)
* \brief set the HW Buffer for the incomming UART Bytes
*/
void testBenchMaster_setUARTreceiveBuffer(void)
{
	AS1_ReceiveBlock(AS1_DeviceData,uartReceiveBufferHW,NOF_BYTES_UART_RECEIVE_BUFFER);
}


/*!
* \fn  void putByteIntoTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr)
* \brief buts a byte which was sent into the testbench into the sendBuffer
* 		 later, it the byte gets received again, the byte gets deleted out of the buffer
*/
static void putByteIntoTestBenchSendBuffer(uint8_t databyte, uint8_t uartNr)
{
	bool foundEmptySpace = false;
	for(int i = 0; i<NOF_BYTES_UART_SEND_BUFFER_TESTBENCH; i++)
	{
		if(sendBuffer[uartNr][i].isEmpty)
		{
			sendBuffer[uartNr][i].isEmpty = false;
			sendBuffer[uartNr][i].byteData = databyte;
			sendBuffer[uartNr][i].sendingTimestamp = time_ms;
			foundEmptySpace = true;
			break;
		}
	}
	if(foundEmptySpace == false)
	{
		while(true){};
	}
}

/*!
* \fn  bool getByteFromTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr)
* \brief serarches the sendbuffer for a byte which was received. If the byte was in the
* 		 buffer, it gets deleted and true is returned.
* 		 If the byte is multiple times in the buffer, the oldes instance is deleted.
*/
static bool getByteFromTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr,uint64_t* byteLatency)
{
	uint16_t usedIndex = 0;
	tTestBenchByteBufferEntry searchedEntry;
	searchedEntry.sendingTimestamp = 0xFFFFFFFF;

	for(int i = 0; i<NOF_BYTES_UART_SEND_BUFFER_TESTBENCH; i++)
	{
		if(!sendBuffer[uartNr][i].isEmpty && sendBuffer[uartNr][i].byteData == *databyte)
		{
			if(searchedEntry.sendingTimestamp > sendBuffer[uartNr][i].sendingTimestamp)
			{
				searchedEntry = sendBuffer[uartNr][i];
				usedIndex = i;
			}
		}
	}

	if(searchedEntry.sendingTimestamp != 0xFFFFFFFF)
	{
		sendBuffer[uartNr][usedIndex].isEmpty = true;
		sendBuffer[uartNr][usedIndex].byteData = 0x00;
		sendBuffer[uartNr][usedIndex].sendingTimestamp = 0xFFFFFFFF;

		*byteLatency = time_ms - searchedEntry.sendingTimestamp;
		return true;
	}
	else
	{
		return false;
	}

}

static uint16_t getLostPackages(uint8_t uartNr)
{
	uint16_t numberOfLostPacks = 0;
	for(int i = 0; i<NOF_BYTES_UART_SEND_BUFFER_TESTBENCH; i++)
	{
		if(!sendBuffer[uartNr][i].isEmpty && sendBuffer[uartNr][i].sendingTimestamp < (time_ms - TIMEOUT_FOR_BYTES_TO_GO_THROUGH_TESTBENCH))
		{
			numberOfLostPacks ++;
			sendBuffer[uartNr][i].isEmpty = true;
			sendBuffer[uartNr][i].byteData = 0x00;
			sendBuffer[uartNr][i].sendingTimestamp = 0xFFFFFFFF;
		}
	}
	return numberOfLostPacks;
}

static void updateTime(void)
{
	static uint16_t lastOsTick = 0, newOsTick;
	newOsTick = xTaskGetTickCount();

	if(newOsTick >= lastOsTick)
		time_ms += (newOsTick-lastOsTick);
	else
	{
		time_ms += (0xFFFF-lastOsTick);
		time_ms += newOsTick;
	}

	lastOsTick = newOsTick;
}


static void updateAverageLatency(uint64_t latencyCurrentByte,uint8_t uartNr)
{
	receivedBytesCounter[uartNr]++;
	if(receivedBytesCounter[uartNr] == 1)
		averageLatency[uartNr] = (float)latencyCurrentByte;
	else if(averageLatency[uartNr]<latencyCurrentByte)
	{
		averageLatency[uartNr] += ((float)latencyCurrentByte*(1/(float)receivedBytesCounter[uartNr]));
	}
	else
	{
		averageLatency[uartNr] -= ((float)latencyCurrentByte*(1/(float)receivedBytesCounter[uartNr]));
	}

}

static void updateTestResults(void)
{
	char infoBuf[200];
	for(int i = 0 ; i < NUMBER_OF_UARTS ; i++)
	{
		nofLostBytes[i] = nofLostBytes[i] + getLostPackages(i);
	}

	XF1_xsprintf(infoBuf, "\nRec.Bytes: %u %u %u %u   LostBytes: %u %u %u %u   Av.Latency: %f %f %f %f",receivedBytesCounter[0],receivedBytesCounter[1],receivedBytesCounter[2],receivedBytesCounter[3],nofLostBytes[0],nofLostBytes[1],nofLostBytes[2],nofLostBytes[3],averageLatency[0],averageLatency[1],averageLatency[2],averageLatency[3]);
	pushMsgToShellQueue(infoBuf);
}

