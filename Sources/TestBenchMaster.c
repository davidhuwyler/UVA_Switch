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
static bool getByteFromTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr,uint32_t* byteLatency);
static void updateAverageLatencyAndByteCouter(uint64_t latencyCurrentByte,uint8_t uartNr);
static void updateTestResults(void);
static uint16_t getLostPackages(uint8_t uartNr);

/*!
* \fn testBenchMaster_TaskEntry(void* p)
* \brief TestBench Master to test the different Routing Algorithms
* The UAV Switches are connected like the Picture below
*
* 				 UART TestBench Data (Input) 56700 Baud
* 								  |
* 		 _________________________|__________________________
* 		| UAV Switch TestBench Master						|
* 		|													|
* 		|													|
* 		|  DeviceSide						ModemSide		|
* 		|___________________________________________________|
* 			  | | | |						  | | | |
* 			  | | | | 4xUART				  | | | | 4xUART
* 		 _____|_|_|_|_______			 _____|_|_|_|_______
* 		|	DeviceSide		|			|	DeviceSide		|
* 		|					|   4xUART	|					|
* 		|	DUT UAV Switch	|___________|	 UAV Switch		|
* 	 	|					|___________|	Modem Simulator	|
* 		|					|___________|	 				|
* 		|			   Modem|___________|Modem				|
* 		|				Side|			|Side				|
* 		|___________________|			|___________________|
*
*/
void testBenchMaster_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.TestBenchMasterTaskInterval);
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */
	char infoBuf[100];
	uint16_t nofBytesInUARTbuffer;
	uint8_t dataByte;

	static bool updatePrintoutsFlag;
	static uint16_t incommingBytes = 0, outgoningBytes = 0;

	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		updateTime();

		//Receive the UART input to use for the Routing Algorthm TestBench (Test Data)
		receivedUARTbyteForTesting(&nofBytesInUARTbuffer);

		for(int i = 0 ; i < NUMBER_OF_UARTS ; i++)
		{
			//Send Bytes into the Testbench
			if(config.testBenchMasterUsedChannels[i])
			{
				for(int i = 0; i<nofBytesInUARTbuffer; i++)
				{
					if(pushToByteQueue(MAX_14830_DEVICE_SIDE,i,&uartNewTestDataBytes[i]) == pdPASS)
					{
						outgoningBytes++;
						putByteIntoTestBenchSendBuffer(uartNewTestDataBytes[i],1);
					}
					else
					{
						XF1_xsprintf(infoBuf, "Lost Byte! Cannot insert in ByteQueue %u for sending",i);
						pushMsgToShellQueue(infoBuf);
					}

					//Debug: Send Byte also to Device 0
					if(pushToByteQueue(MAX_14830_DEVICE_SIDE,0,&uartNewTestDataBytes[i]) != pdPASS)
					{
						XF1_xsprintf(infoBuf, "Lost Byte! Cannot insert in ByteQueue0 for sending");
						pushMsgToShellQueue(infoBuf);
					}
				}
			}

			//Get Bytes back from Testbench
			while (popFromByteQueue(MAX_14830_WIRELESS_SIDE, i, &dataByte) == pdTRUE)
			{
				uint32_t latency;
				incommingBytes++;

				//Debug: Send byte out again on device 3
				pushToByteQueue(MAX_14830_DEVICE_SIDE,3,&dataByte);

				if(getByteFromTestBenchSendBuffer(&dataByte,i,&latency))
				{
					updateAverageLatencyAndByteCouter(latency,1);
				}

				updatePrintoutsFlag = true;
			}

			//Update Lost bytes
			nofLostBytes[i] = nofLostBytes[i] + getLostPackages(i);
		}

		//Print out results
		if(updatePrintoutsFlag)
		{
			updatePrintoutsFlag = false;
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

/*!
* \fn  void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer)
* \brief get data from the UART Port. This is the Data to be inserted into the Testbench
* 		 either from a external PC or from a Serial Recorder
*/
static void receivedUARTbyteForTesting(uint16_t* nofBytesInBuffer)
{
	uint16_t newDataSize = 0;
	uint16_t currentIndexInUARTrecFIFO = AS1_GetReceivedDataNum(AS1_DeviceData);
	static uint16_t oldIndexInUARTrecFIFO = 0;

	//if(oldIndexInUARTrecFIFO==currentIndexInUARTrecFIFO) -> Do Nothing, No new Data

	//Copy The UART data out of the Buffer
	//No buffer overflow
	if(oldIndexInUARTrecFIFO<currentIndexInUARTrecFIFO)
	{
		for(int i = oldIndexInUARTrecFIFO  ; i<currentIndexInUARTrecFIFO; i++)
		{
			uartNewTestDataBytes[newDataSize]=uartReceiveBufferHW[i];
			newDataSize ++;
		}
	}

	//Buffer overflow...
	else if(oldIndexInUARTrecFIFO>currentIndexInUARTrecFIFO)
	{
		for(int i = oldIndexInUARTrecFIFO  ; i<NOF_BYTES_UART_RECEIVE_BUFFER; i++)
		{
			uartNewTestDataBytes[newDataSize]=uartReceiveBufferHW[i];
			newDataSize ++;
		}
		for(int i = 0 ; i<currentIndexInUARTrecFIFO; i++)
		{
			uartNewTestDataBytes[newDataSize]=uartReceiveBufferHW[i];
			newDataSize ++;
		}
	}

	//Clear unused Part of the Buffer
	for(int i = newDataSize; i< NOF_BYTES_UART_RECEIVE_BUFFER; i++)
	{
		uartNewTestDataBytes[i]=0;
	}

	oldIndexInUARTrecFIFO = currentIndexInUARTrecFIFO;
	*nofBytesInBuffer = newDataSize;
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
static bool getByteFromTestBenchSendBuffer(uint8_t* databyte, uint8_t uartNr,uint32_t* byteLatency)
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

/*!
* \fn  uint16_t getLostPackages(uint8_t uartNr)
* \brief checks the sendBuffer[] if there are older bytes than the Timeout (TIMEOUT_FOR_BYTES_TO_GO_THROUGH_TESTBENCH)
* 		 older Bytes are counted and deleted. The number deleted (== lost) Bytes is returned.
*/
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

/*!
* \fn  void updateTime(void)
* \brief updated the time_ms variable to hold the time since boot
*/
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

/*!
* \fn  void updateAverageLatencyAndByteCouter(uint64_t latencyCurrentByte,uint8_t uartNr)
* \brief calculates the average latency per channel (uartNR)
*/
static void updateAverageLatencyAndByteCouter(uint64_t latencyCurrentByte,uint8_t uartNr)
{
	receivedBytesCounter[uartNr]++;
	if(receivedBytesCounter[uartNr] == 1)
		averageLatency[uartNr] = (float)latencyCurrentByte;
	else if(averageLatency[uartNr]<latencyCurrentByte)
	{
		averageLatency[uartNr] += ((float)latencyCurrentByte/((float)receivedBytesCounter[uartNr]));
	}
	else
	{
		averageLatency[uartNr] -= ((float)latencyCurrentByte/((float)receivedBytesCounter[uartNr]));
	}

}

/*!
* \fn  void updateTestResults(void)
* \brief prints out the current results
*/
static void updateTestResults(void)
{
	char infoBuf[200];
	XF1_xsprintf(infoBuf, "\nRec.Bytes: %u %u %u %u   LostBytes: %u %u %u %u   Av.Latency: %f %f %f %f",receivedBytesCounter[0],receivedBytesCounter[1],receivedBytesCounter[2],receivedBytesCounter[3],nofLostBytes[0],nofLostBytes[1],nofLostBytes[2],nofLostBytes[3],averageLatency[0],averageLatency[1],averageLatency[2],averageLatency[3]);
	pushMsgToShellQueue(infoBuf);
}

