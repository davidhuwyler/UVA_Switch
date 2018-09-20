/*
 * ApplicationHandler.c
 *
 *  Created on: 02.05.2018
 *      Author: Stefanie
 */

#include "TransportHandler.h"
#include "NetworkHandler.h"
#include "NetworkMetrics.h"
#include "PackageHandler.h"
#include "SpiHandler.h"
#include "FRTOS.h"
#include "RNG.h"
#include "Config.h"
#include "ThroughputPrintout.h"
#include "Shell.h"
#include "LedRed.h"
#include "Platform.h"
#include "PackageBuffer.h"

/* --------------- prototypes ------------------- */
static bool processReceivedPayload(tWirelessPackage* pPackage);
static bool generateDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage);
static bool generateAckPackage(tWirelessPackage* pReceivedDataPack, tWirelessPackage* pAckPack);
static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage);
static bool generateTestDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage, bool returned);
static BaseType_t pushToGeneratedPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);
static void pushPayloadOut(tWirelessPackage* package);
static bool pushNextStoredPackOut(tUartNr wlConn);
static bool pushOldestPackOutIfTimeout(tUartNr wlConn, bool forced);
static void initTransportHandlerQueues(void);
static BaseType_t peekAtReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);
static BaseType_t nofReceivedPayloadPacksInQueue(tUartNr uartNr);
static BaseType_t popFromReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);


/* --------------- global variables -------------------- */
static xQueueHandle queueGeneratedPayloadPacks[NUMBER_OF_UARTS]; /* Outgoing data to wireless side */
static xQueueHandle queueReceivedPayloadPacks[NUMBER_OF_UARTS]; /* Outgoing data to wireless side */
static char* queueNameReadyToSendPacks[] = {"queueGeneratedPacksFromDev0", "queueGeneratedPacksFromDev1", "queueGeneratedPacksFromDev2", "queueGeneratedPacksFromDev3"};
static char* queueNameReceivedPayload[] = {"queueReceivedPacksFromDev0", "queueReceivedPacksFromDev1", "queueReceivedPacksFromDev2", "queueReceivedPacksFromDev3"};
static uint16_t payloadNumTracker[NUMBER_OF_UARTS];
static uint16_t sentAckNumTracker[NUMBER_OF_UARTS];

static tPackageBuffer sendBuffer[NUMBER_OF_UARTS];								/*Packets are stored which wait for the acknowledge */
static tPackageBuffer receiveBuffer[NUMBER_OF_UARTS];							/*Packets are stored which wait for reordering */

//static uint16_t sysTimeLastPushedOutPayload[NUMBER_OF_UARTS];  Which package was last sent out [payloadNR!!!!]
//static uint16_t minSysTimeOfStoredPackagesForReordering[NUMBER_OF_UARTS];
//static tWirelessPackage reorderingPacks[NUMBER_OF_UARTS][REORDERING_PACKAGES_BUFFER_SIZE];
//static bool reorderingPacksOccupiedAtIndex[NUMBER_OF_UARTS][REORDERING_PACKAGES_BUFFER_SIZE];
//static uint32_t nofReorderingPacksStored[NUMBER_OF_UARTS];
#if PL_HAS_PERCEPIO
traceString appHandlerUserEvent[10];
#endif



/*!
* \fn void transportHandler_TaskEntry(void)
* \brief Task generates packages from received bytes (received on device side) and sends those down to
* the NetworkHandler for transmission.
* Payload reordering is done here when configured
*/
void transportHandler_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.TransportHandlerTaskInterval);
	tWirelessPackage package,pAckPack;
	bool request = true;
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */


	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		/* generate data packages and put those into the package queue */
		for(int deviceNr = 0; deviceNr<NUMBER_OF_UARTS; deviceNr++)
		{

			/*------------------------ Generate TestPackets if requested ---------------------------*/
			if (popFromRequestNewTestPacketPairQueue(&request) == pdTRUE)
			{
				sendOutTestPackagePair(deviceNr, &package);
			}

			/*------------------ Generate Packages From Ray Data (Device Bytes)---------------------*/
			if (generateDataPackage(deviceNr, &package))
			{
				if (packageBuffer_put(&sendBuffer[deviceNr],&package) != true)//Put data-package into sendBuffer until Acknowledge gets received
				{
					tWirelessPackage oldestPackage;
					packageBuffer_getOldestPackage(&sendBuffer[deviceNr],&oldestPackage); //If buffer full, delete oldest Package
					vPortFree(oldestPackage.payload);
					oldestPackage.payload = NULL;

					if(packageBuffer_put(&sendBuffer[deviceNr],&package) != true) //Try again to put pack into sendbuffer
					{
						vPortFree(package.payload);
						package.payload = NULL;
					}
					else if(pushToGeneratedPacksQueue(deviceNr, &package) != pdTRUE) //Put data-package into Queues
					{
						vPortFree(package.payload);
						package.payload = NULL;
						if(packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr))
						{
							vPortFree(package.payload);
							package.payload = NULL;
						}
					}
				}
				else if (pushToGeneratedPacksQueue(deviceNr, &package) != pdTRUE) 		//Put data-package into Queues
				{
					vPortFree(package.payload);
					package.payload = NULL;
					if(packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr))
					{
						vPortFree(package.payload);
						package.payload = NULL;
					}
				}
			}

			/*-----------------------Handle Incoming Packages from the Modems-----------------------*/
			while (nofReceivedPayloadPacksInQueue(deviceNr) > 0)
			{
				peekAtReceivedPayloadPacksQueue(deviceNr, &package);

			/*--------------> Incoming Package == DataPackage <-----------------*/
				if(package.packType == PACK_TYPE_DATA_PACKAGE)
				{
					if(packageBuffer_put(&receiveBuffer[deviceNr],&package) != true)			//Put data-package into receiveBuffer
					{
						packageBuffer_getPackage(&receiveBuffer[deviceNr],&package,package.payloadNr);
						vPortFree(package.payload);
						package.payload = NULL;
					}
					else
					{
						//Send Acknowledge for the DataPack
						generateAckPackage(&package, &pAckPack);
						if (pushToGeneratedPacksQueue(deviceNr, &pAckPack) != pdTRUE)		//Put ack-package into Queues
						{
							vPortFree(pAckPack.payload);
							pAckPack.payload = NULL;
						}

						//Send out all packages from the buffer which are in order
						while(packageBuffer_getNextOrderedPackage(&receiveBuffer[deviceNr],&package))
						{
							if(freeSpaceInTxByteQueue(MAX_14830_DEVICE_SIDE, package.devNum) >= package.payloadSize)
							{
								popFromReceivedPayloadPacksQueue(deviceNr,&package);
								vPortFree(package.payload);
								package.payload = NULL;
							}
							else
							{	//If the TX Byte Queue hasnt enough space, the package gets reinserted into the Buffer
								packageBuffer_put(&receiveBuffer[deviceNr],&package);
							}
						}
					}
				}

			/*--------------> Incoming Package == Acknowledge <-----------------*/
				else if(package.packType == PACK_TYPE_REC_ACKNOWLEDGE)
				{
					uint16_t ackPayloadNr = package.payload[0];
					ackPayloadNr = ackPayloadNr | ((package.payload[1])<<8);

					//Delete Acknowledged package from all sendBuffer
					for(int i = 0 ; i<NUMBER_OF_UARTS ; i++)
					{
						if(packageBuffer_getPackage(&sendBuffer[i],&package,ackPayloadNr))
						{
							vPortFree(package.payload);
							package.payload = NULL;
						}
					}

				}

			/*--------------> Incoming Package == NetworkTestPackage <----------*/

				else if (package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE)
				{
					/* Copy payload out of testpackage */
					tTestPackagePayload payload;
					uint8_t *bytePtrPayload = (uint8_t*) &payload;
					for (int i = 0; i < sizeof(tTestPackagePayload); i++)
					{
						bytePtrPayload[i] = package.payload[i];
					}

					/* Return the Testpackage if this device is not the Sender */
					if (payload.returned && generateTestDataPackage(deviceNr, &package,	true))
					{
						if (pushToGeneratedPacksQueue(deviceNr,	&package) != pdTRUE)
						{
							vPortFree(package.payload);
							package.payload = NULL;
						}
					}
					/* Test-Packet returned from receiver */
					else
					{
						// TODO Send packet time and queue length to networksMetrics

						vPortFree(package.payload);
						package.payload = NULL;
					}
				}
			}


			/*------------------------ Resend Unacknowledged Wireless Packages ---------------------------*/
			uint16_t numberOfResendAttempts;
			while(packageBuffer_getNextPackageOlderThanTimeoutWithVar(&sendBuffer[deviceNr],&package,&numberOfResendAttempts,TIME_BETWEEN_RESEND_ATTEMPTS))
			{
				if(numberOfResendAttempts<MAX_NOF_RESEND_ATTEMPTS)  //Resend
				{
					packageBuffer_putWithVar(&sendBuffer[deviceNr],&package,(numberOfResendAttempts+1)); //Reinsert Package in the Buffer with new Timestamp
					if (pushToGeneratedPacksQueue(deviceNr, &package) != pdTRUE)		//Put data-package into Queues
					{
						vPortFree(package.payload);
						package.payload = NULL;
						packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr);
						vPortFree(package.payload);
						package.payload = NULL;
					}
				}
				else												//Max Number of resends reached... Delete Package
				{
					vPortFree(package.payload);
					package.payload = NULL;
				}
			}

			/*------------------------ Push Out received Wireless-Packets out of Order if timeOut has occurred ---------------------------*/
			while(packageBuffer_getNextPackageOlderThanTimeout(&receiveBuffer[deviceNr],&package,TIMEOUT_PUSH_OUT_PACKAGE_OUT_OF_ORDER))
			{
				if(freeSpaceInTxByteQueue(MAX_14830_DEVICE_SIDE, package.devNum) >= package.payloadSize)
				{
					popFromReceivedPayloadPacksQueue(deviceNr,&package);
					vPortFree(package.payload);
					package.payload = NULL;
				}
				else
				{	//If the TX Byte Queue hasnt enough space, the package gets reinserted into the Buffer
					packageBuffer_put(&receiveBuffer[deviceNr],&package);
				}
			}

//			if(*package.payload)
//			{
//				vPortFree(package.payload);
//				package.payload = NULL;
//			}
//			if(pAckPack.payload)
//			{
//				vPortFree(pAckPack.payload);
//				pAckPack.payload = NULL;
//			}



//#if 1
//			/* push payload of wireless package out on device side */
//			while(nofReceivedPayloadPacksInQueue(deviceNr) > 0)
//			{
//				peekAtReceivedPayloadPacksQueue(deviceNr, &package);
//
//				/* received Package is a Test-Package */
//				if(package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE)
//				{
//					/* Copy payload out of testpackage */
//					tTestPackagePayload payload;
//					uint8_t *bytePtrPayload = (uint8_t*)&payload;
//					for(int i = 0 ; i < sizeof(tTestPackagePayload) ; i++)
//					{
//						 bytePtrPayload[i]=package.payload[i];
//					}
//
//					/* Return the Testpackage if this device is not the Sender */
//					if(payload.returned && generateTestDataPackage(deviceNr, &package, true))
//					{
//						if(pushToGeneratedPacksQueue(deviceNr, &package) != pdTRUE)
//						{
//							vPortFree(package.payload);
//							package.payload = NULL;
//						}
//					}
//					/* Test-Packet returned from receiver */
//					else
//					{
//						// TODO Send packet time and queue length to networksMetrics
//
//
//						vPortFree(package.payload);
//						package.payload = NULL;
//					}
//				}
//
//				/* received Package is a Data-Package */
//				else if(freeSpaceInTxByteQueue(MAX_14830_DEVICE_SIDE, package.devNum) >= package.payloadSize) /* check if enough space */
//				{
//					processReceivedPayload(&package);
//					popFromReceivedPayloadPacksQueue(deviceNr, &package);
//					vPortFree(package.payload);
//					package.payload = NULL;
//				}
//			}

			/* check if there is a timeout on the package reordering */
//			if(config.PayloadNumberingProcessingMode[deviceNr] == PAYLOAD_REORDERING) /* package reordering configured for this device? */
//			{
//				while(pushOldestPackOutIfTimeout(deviceNr, false)) /* a timeout occured for oldest saved package? */
//				{
//					while(pushNextStoredPackOut(deviceNr)); /* push next packages out that are in order */
//				}
//			}

//#endif
		}

	}
}

/*!
* \fn void transportHandler_TaskInit(void)
* \brief Initializes all queues that are declared within application handler
*/
void transportHandler_TaskInit(void)
{
	initTransportHandlerQueues();

	for(int i = 0; i<NUMBER_OF_UARTS; i++)
	{
		packageBuffer_init(&sendBuffer[i]);
		packageBuffer_init(&receiveBuffer[i]);
	}

#if PL_HAS_PERCEPIO
	appHandlerUserEvent[0] = xTraceRegisterString("GenerateDataPackageEnter");
	appHandlerUserEvent[1] = xTraceRegisterString("GenerateDataPackageExit");
	appHandlerUserEvent[2] = xTraceRegisterString("readHwBufAndWriteToQueue");
	appHandlerUserEvent[3] = xTraceRegisterString("before for-loop");
	appHandlerUserEvent[4] = xTraceRegisterString("popping one byte");
	appHandlerUserEvent[5] = xTraceRegisterString("queue empty");
#endif
}


/*!
* \fn void initTransportHandlerQueues(void)
* \brief This function initializes the array of queues
*/
static void initTransportHandlerQueues(void)
{
#if configSUPPORT_STATIC_ALLOCATION
	static uint8_t xStaticQueuePacksToSend[NUMBER_OF_UARTS][ QUEUE_NUM_OF_READY_TO_SEND_WL_PACKS * sizeof(tWirelessPackage) ]; /* The variable used to hold the queue's data structure. */
	static uint8_t xStaticQueuePayloadReceived[NUMBER_OF_UARTS][ QUEUE_NUM_OF_RECEIVED_PAYLOAD_PACKS * sizeof(tWirelessPackage) ]; /* The variable used to hold the queue's data structure. */
	static StaticQueue_t ucQueueStoragePacksToSend[NUMBER_OF_UARTS]; /* The array to use as the queue's storage area. */
	static StaticQueue_t ucQueueStoragePayloadReceived[NUMBER_OF_UARTS]; /* The array to use as the queue's storage area. */
#endif
	for(int uartNr=0; uartNr<NUMBER_OF_UARTS; uartNr++)
	{
#if configSUPPORT_STATIC_ALLOCATION
		queueGeneratedPayloadPacks[uartNr] = xQueueCreateStatic( QUEUE_NUM_OF_READY_TO_SEND_WL_PACKS, sizeof(tWirelessPackage), xStaticQueuePacksToSend[uartNr], &ucQueueStoragePacksToSend[uartNr]);
		queueReceivedPayloadPacks[uartNr] = xQueueCreateStatic( QUEUE_NUM_OF_RECEIVED_PAYLOAD_PACKS, sizeof(tWirelessPackage), xStaticQueuePayloadReceived[uartNr], &ucQueueStoragePayloadReceived[uartNr]);
#else
		queuePackagesToSend[uartNr] = xQueueCreate( QUEUE_NUM_OF_READY_TO_SEND_WL_PACKS, sizeof(tWirelessPackage));
		queueReceivedPayloadPacks[uartNr] = xQueueCreate( QUEUE_NUM_OF_RECEIVED_WL_PACKS, sizeof(tWirelessPackage));
#endif
		if((queueGeneratedPayloadPacks[uartNr] == NULL) || (queueReceivedPayloadPacks == NULL))
			while(true){} /* malloc for queue failed */
		vQueueAddToRegistry(queueGeneratedPayloadPacks[uartNr], queueNameReadyToSendPacks[uartNr]);
		vQueueAddToRegistry(queueReceivedPayloadPacks[uartNr], queueNameReceivedPayload[uartNr]);
	}
}


/*!
* \fn static void generateDataPackage(uint8_t deviceNumber, Queue* dataSource)
* \brief Function to generate a data package, reading data from the data source.
* \param deviceNumber: Number of device.
* \param dataSource: Pointer to the queue where the data where the data is read.
* \param wPackage: Pointer to package structure.
* \return true if a package was generated and saved in wPackage, false otherwise.
*/
static bool generateDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage)
{
	static uint32_t tickTimeSinceFirstCharReceived[NUMBER_OF_UARTS]; /* static variables are initialized as 0 by default */
	static bool dataWaitingToBeSent[NUMBER_OF_UARTS];
	static const uint8_t packHeaderBuf[PACKAGE_HEADER_SIZE - 1] = { PACK_START, PACK_TYPE_DATA_PACKAGE, 0, 0, 0, 0, 0, 0, 0, 0 };
	char infoBuf[60];

	uint16_t numberOfBytesInRxQueue = (uint16_t) numberOfBytesInRxByteQueue(MAX_14830_DEVICE_SIDE, deviceNr);
	uint32_t timeWaitedForPackFull = xTaskGetTickCount()-tickTimeSinceFirstCharReceived[deviceNr];

	if((deviceNr >= NUMBER_OF_UARTS) || (pPackage == NULL)) /* check validity of function parameters */
	{
		return false;
	}
	/* check if enough data to fill package (when not configured to 0) or maximum wait time for full package done */
	if( ( (numberOfBytesInRxQueue >= config.UsualPacketSizeDeviceConn[deviceNr]) && (0 != config.UsualPacketSizeDeviceConn[deviceNr]) ) ||
		(numberOfBytesInRxQueue >= PACKAGE_MAX_PAYLOAD_SIZE) ||
		( (dataWaitingToBeSent[deviceNr] == true) && (timeWaitedForPackFull >= pdMS_TO_TICKS(config.PackageGenMaxTimeout[deviceNr])) ) )
	{
		/* reached usual packet size or timeout, generate package
		 * check if there is enough space to store package in queue before generating it
		 * Dropping of data in case all queues are full is handled in byte queue.
		 * When reading data from HW buf but no space in byte queue, oldest bytes will be popped from queue and dropped.
		 * Hopefully, this will do and no dropping of data on purpose is needed anywhere else for Rx side. */
		/* limit payload of package */
		//vTracePrint(appHandlerUserEvent[0], "enter");
		if(numberOfBytesInRxQueue > PACKAGE_MAX_PAYLOAD_SIZE)
		{
			numberOfBytesInRxQueue = PACKAGE_MAX_PAYLOAD_SIZE;
		}
		/* Put together package */
		/* put together payload by allocating memory and copy data */
		pPackage->payloadSize = numberOfBytesInRxQueue;
		pPackage->payload = (uint8_t*) FRTOS_pvPortMalloc(numberOfBytesInRxQueue*sizeof(int8_t));
		if(pPackage->payload == NULL) /* malloc failed */
		{
			return false;
		}
		/* get data from queue */
		for (uint16_t cnt = 0; cnt < pPackage->payloadSize; cnt++)
		{
			if(popFromByteQueue(MAX_14830_DEVICE_SIDE, deviceNr, &pPackage->payload[cnt]) != pdTRUE) /* ToDo: handle queue failure */
			{
				UTIL1_strcpy(infoBuf, sizeof(infoBuf), "Warning: Pop from UART ");
				UTIL1_strcatNum8u(infoBuf, sizeof(infoBuf), pPackage->devNum);
				UTIL1_strcat(infoBuf, sizeof(infoBuf), " not successful");
				pushMsgToShellQueue(infoBuf);
				numberOfDroppedBytes[MAX_14830_DEVICE_SIDE][deviceNr] += cnt;
				FRTOS_vPortFree(pPackage->payload);
				pPackage->payload = NULL;
				return false;
			}
		}
		/* put together the rest of the header */
		pPackage->packType = PACK_TYPE_DATA_PACKAGE;
		pPackage->devNum = deviceNr;
		pPackage->payloadNr = ++payloadNumTracker[deviceNr];
		/* reset last timestamp */
		tickTimeSinceFirstCharReceived[deviceNr] = 0;
		/* reset flag that timestamp was updated */
		dataWaitingToBeSent[deviceNr] = false;
		//vTracePrint(appHandlerUserEvent[1], "exit");
		return true;
	}
	else if(numberOfBytesInRxQueue > 0)
	{
		/* there is data available, update timestamp that data is available (but only if the timestamp wasn't already updated) */
		if (dataWaitingToBeSent[deviceNr] == false)
		{
			tickTimeSinceFirstCharReceived[deviceNr] = xTaskGetTickCount();
			dataWaitingToBeSent[deviceNr] = true;
		}
		return false;
	}
	else
	{
		/* package was not generated */
		return false;
	}
	return false;
}

/*!
* \fn static bool generateAckPackage(tWirelessPackage* pReceivedDataPack, tWirelessPackage* pAckPack)
* \brief Function to generate a receive acknowledge package, reading data from the data source.
* \param pAckPack: Pointer to acknowledge packet to be created
* \param pReceivedDataPack: Pointer to the structure that holds the wireless package that the acknowledge is generated for
* \return true if a package was generated and saved, false otherwise.
*/
static bool generateAckPackage(tWirelessPackage* pReceivedDataPack, tWirelessPackage* pAckPack)
{
	/* default header = { PACK_START, PACK_TYPE_REC_ACKNOWLEDGE, 0, 0, 0, 0, 0, 0, 0, 0 } */
	/* prepare wireless package */
	pAckPack->packType = PACK_TYPE_REC_ACKNOWLEDGE;
	pAckPack->devNum = pReceivedDataPack->devNum;
	pAckPack->packNr = ++sentAckNumTracker[pReceivedDataPack->devNum];
	pAckPack->payloadNr = pReceivedDataPack->payloadNr;
	pAckPack->payloadSize = sizeof(pAckPack->packNr);	/* as payload, the timestamp of the package to be acknowledged is saved */
	/* get space for acknowladge payload (which consists of packNr of datapackage*/
	pAckPack->payload = (uint8_t*) FRTOS_pvPortMalloc(pAckPack->payloadSize*sizeof(int8_t));
	if(pAckPack->payload == NULL) /* malloc failed */
		return false;
	/* generate payload */
	for (uint16_t cnt = 0; cnt < pAckPack->payloadSize; cnt++)
	{
		pAckPack->payload[cnt] = *((uint8_t*)(&pReceivedDataPack->packNr) + cnt);
	}
	return true;
}

/*!
* \fn static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage)
* \brief Function to generate a test data package pair used to determine the network metrics
*/
static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage)
{
	if(generateTestDataPackage(deviceNr, pPackage,false))
	{
		if(pushToGeneratedPacksQueue(deviceNr, pPackage) != pdTRUE)
		{
			vPortFree(pPackage->payload);
			pPackage->payload = NULL;
		}
	}
	if(generateTestDataPackage(deviceNr, pPackage,false))
	{
		if(pushToGeneratedPacksQueue(deviceNr, pPackage) != pdTRUE)
		{
			vPortFree(pPackage->payload);
			pPackage->payload = NULL;
		}
	}
}


/*!
* \fn static void generateTestDataPackages(tUartNr deviceNr, tWirelessPackage* pPackage)
* \brief Function to generate a test data package used to determine the network metrics
* \param returned true = packet was returned.
* \return true if a package was generated and saved in wPackage, false otherwise.
*/
static bool generateTestDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage, bool returned)
{
	tTestPackagePayload payload;
	static const uint8_t packHeaderBuf[PACKAGE_HEADER_SIZE - 1] = { PACK_START, PACK_TYPE_NETWORK_TEST_PACKAGE, 0, 0, 0, 0, 0, 0, 0, 0 };

	uint16_t numberOfBytesInRxQueue = (uint16_t) numberOfBytesInRxByteQueue(MAX_14830_WIRELESS_SIDE, deviceNr);
	uint16_t numberOfBytesInTxQueue = (uint16_t) numberOfBytesInTxByteQueue(MAX_14830_WIRELESS_SIDE, deviceNr);

	if((deviceNr >= NUMBER_OF_UARTS) || (pPackage == NULL)) /* check validity of function parameters */
	{
		return false;
	}

	/* Put together package */

	/* Fill Payload	 */
	payload.returned = !returned;
	if(!returned)
	{
		payload.sendTimestamp = xTaskGetTickCount();
		payload.longestQueue = 0;
	}
	else
	{
		if(numberOfBytesInRxQueue>numberOfBytesInTxQueue)
			payload.longestQueue = numberOfBytesInRxQueue;
		else
			payload.longestQueue = numberOfBytesInTxQueue;
	}

	/* put together payload by allocating memory and copy data */
	pPackage->payloadSize = sizeof(tTestPackagePayload);	//Payload of the testpackets
	pPackage->payload = (uint8_t*) FRTOS_pvPortMalloc(sizeof(tTestPackagePayload));
	if (pPackage->payload == NULL) /* malloc failed */
	{
		return false;
	}

	uint8_t *bytePtrPayload = (uint8_t*)&payload;
	for(int i = 0 ; i < sizeof(tTestPackagePayload) ; i++)
	{
		pPackage->payload[i] = bytePtrPayload[i];
	}

	/* put together the rest of the header */
	pPackage->packType = PACK_TYPE_NETWORK_TEST_PACKAGE;
	pPackage->devNum = deviceNr;
	//pPackage->payloadNr = ++payloadNumTracker[deviceNr];

	return true;
}


static bool processReceivedPayload(tWirelessPackage* pPackage)
{
//	char infoBuf[50];
//	/* send data out at correct device side if packages received in order*/
//	switch (config.PayloadNumberingProcessingMode[pPackage->devNum])
//	{
//	case PAYLOAD_NUMBER_IGNORED:
//		pushPayloadOut(pPackage);
//		sysTimeLastPushedOutPayload[pPackage->devNum] = pPackage->payloadNr; /* no need to keep track of package numbering, but done anyway here */
//		break;
//	case PAYLOAD_REORDERING:
//		if (sysTimeLastPushedOutPayload[pPackage->devNum] <= pPackage->payloadNr) /* old package received */
//		{
//			break;
//		}
//		else if (sysTimeLastPushedOutPayload[pPackage->devNum] != pPackage->payloadNr + 1) /* package not received in order, saving to buffer needed */
//
//		{
//			tWirelessPackage nextPack;
//			/* package cant be stored, no empty space */
//			if (nofReorderingPacksStored[pPackage->devNum]
//					>= sizeof(reorderingPacksOccupiedAtIndex[pPackage->devNum])) {
//				pushOldestPackOutIfTimeout(pPackage->devNum, true); /* force oldest package to be pushed out */
//				while (pushNextStoredPackOut(pPackage->devNum))
//					; /* push any other packages out that are now stored in order */
//			}
//			/* iterate through array to find an empty spot and store package there */
//			for (int i = 0; i < sizeof(reorderingPacks[pPackage->devNum]); i++) {
//				if (reorderingPacksOccupiedAtIndex[pPackage->devNum][i] != 1) /* spot empty? */
//				{
//					reorderingPacks[pPackage->devNum][i] = *pPackage; /* save package at new spot */
//					reorderingPacksOccupiedAtIndex[pPackage->devNum][i] = 1; /*mark spot as occupied */
//					nofReorderingPacksStored[pPackage->devNum]++;
//					break; /* leave for loop */
//				}
//			}
//		} else if (sysTimeLastPushedOutPayload[pPackage->devNum] == pPackage->payloadNr + 1) /* package received in order */
//				{
//			pushPayloadOut(pPackage); /* push out current package */
//			/* payload is freed at the end of this function */
//			sysTimeLastPushedOutPayload[pPackage->devNum] = pPackage->payloadNr; /* update package number tracker */
//			while (pushNextStoredPackOut(pPackage->devNum))
//				;
//		}
//		break;
//	case ONLY_SEND_OUT_NEW_PAYLOAD:
//		if (sysTimeLastPushedOutPayload[pPackage->devNum] < pPackage->payloadNr) /* package is newer than the last one pushed out */
//		{
//			pushPayloadOut(pPackage);
//			sysTimeLastPushedOutPayload[pPackage->devNum] = pPackage->payloadNr;
//		}
//		break;
//	default:
//		UTIL1_strcpy(infoBuf, sizeof(infoBuf),"Error: Wrong configuration for PACK_NUMBERING_PROCESSING_MODE, value not in range\r\n");
//		LedRed_On();
//		pushMsgToShellQueue(infoBuf);
//		pushPayloadOut(pPackage);
//		sysTimeLastPushedOutPayload[pPackage->devNum] = pPackage->payloadNr; /* no need to keep track of package numbering, but done anyway here */
//	}
	return true;
}


/*!
* \fn static void pushPayloadOut(tWirelessPackage package)
* \brief Pushes the payload of a wireless package out on the correct device
* \param pPackage: pointer to package to be pushed out
*/
static void pushPayloadOut(tWirelessPackage* pPackage)
{
	static char infoBuf[80];
	for(uint16_t cnt=0; cnt<pPackage->payloadSize; cnt++)
	{
		if(pushToByteQueue(MAX_14830_DEVICE_SIDE, pPackage->devNum, &pPackage->payload[cnt]) == pdFAIL)
		{
			XF1_xsprintf(infoBuf, "%u: Warning: Push to device byte array for UART %u failed", xTaskGetTickCount(), pPackage->devNum);
			pushMsgToShellQueue(infoBuf);
			numberOfDroppedBytes[MAX_14830_WIRELESS_SIDE][pPackage->devNum]++;
		}
	}
}


/*!
* \fn static void pushNextStoredPackOut(tUartNr uartNr)
* \brief Iterates through the stored packages and pushes the next one out if it is the right order
* \param uartNr: which device is affected
*/
//static bool pushNextStoredPackOut(tUartNr uartNr)
//{
//	tWirelessPackage nextPack;
//	uint32_t nofReorderingPacksLeft = nofReorderingPacksStored[uartNr];
//	for(int i=0; i<sizeof(reorderingPacks[uartNr]); i++)
//	{
//		if(reorderingPacksOccupiedAtIndex[uartNr][i])
//		{
//			nofReorderingPacksLeft--;
//			nextPack = reorderingPacks[uartNr][i];
//			if(nextPack.payloadNr == sysTimeLastPushedOutPayload[uartNr]+1)
//			{
//				pushPayloadOut(&nextPack);
//				vPortFree(nextPack.payload);
//				nextPack.payload = NULL;
//				reorderingPacksOccupiedAtIndex[uartNr][i] = 0;
//				nofReorderingPacksStored[uartNr]--;
//				sysTimeLastPushedOutPayload[uartNr] = nextPack.payloadNr;
//				return true;
//			}
//		}
//		if(nofReorderingPacksLeft <= 0)
//		{
//			/* no need to iterate over the entire array if we know that there are no more packages stored */
//			return false;
//		}
//	}
//	return false;
//}

/*!
* \fn static bool pushOldestPackOutIfTimeout(tUartNr uartNr, bool forced)
* \brief Iterates through the receivedPackages array and pushes the package out with the
* lowest sysTime if its timeout is done or if forced == true then the oldest package is pushed out
* no matter if its timeout is done or not.
* \param uartNr: specify which device side
* \param forced: force oldest package out, no matter if timeout done or not -> freeing space
*/
//static bool pushOldestPackOutIfTimeout(tUartNr uartNr, bool forced)
//{
//	tWirelessPackage oldestPack;
//	tWirelessPackage tmpPack;
//	bool firstIteration = true;
//	int oldestPackIndex = 0;
//
//	/* find package with the oldest timestamp (sysTime */
//	for(int i=0; i<sizeof(reorderingPacks[uartNr]); i++)
//	{
//		if(reorderingPacksOccupiedAtIndex[uartNr][i]) /* there is a package stored at this index */
//		{
//			tmpPack = reorderingPacks[uartNr][i];
//			if(firstIteration == true) /* on first iteration, initialize variable with any package */
//			{
//				oldestPack = tmpPack;
//				oldestPackIndex = i;
//				firstIteration = false; /* only initialize once */
//			}
//
//			if(tmpPack.payloadNr < oldestPack.payloadNr) /* found older package than the previous one */
//			{
//				oldestPack = tmpPack;
//				oldestPackIndex = i;
//			}
//		}
//	}
//
//	if(firstIteration == true) /* the array is empty, no package found */
//	{
//		return false;
//	}
//
//	if(forced || (oldestPack.timestampPackageReceived + config.PayloadReorderingTimeout[uartNr] > xTaskGetTickCount())) /* oldest package is timed out or needs to be pushed out */
//	{
//		pushPayloadOut(&oldestPack);
//		vPortFree(oldestPack.payload);
//		oldestPack.payload = NULL;
//		reorderingPacksOccupiedAtIndex[uartNr][oldestPackIndex] = false;
//		nofReorderingPacksStored[uartNr]--;
//		sysTimeLastPushedOutPayload[uartNr] = oldestPack.payloadNr;
//		return true;
//	}
//	else /* oldest package is not timed out */
//	{
//		return false;
//	}
//}





/*!
* \fn static ByseType_t pushToReadyToSendPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
* \brief Stores package in queue
* \param uartNr: UART number where data was collected.
* \param pPackage: The location of the package to be pushed to the queue
* \return Status if xQueueSendToBack has been successful
*/
static BaseType_t pushToGeneratedPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueueSendToBack(queueGeneratedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}

	return pdFAIL; /* if uartNr was not in range */
}



/*!
* \fn ByseType_t popToReadyToSendPackFromQueue(tUartNr uartNr, tWirelessPackage* pPackage)
* \brief Pops a package from queue
* \param uartNr: UART number where data was collected.
* \param pPackage: Pointer to empty package
* \return Status if xQueueReceive has been successful
*/
BaseType_t popFromGeneratedPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueueReceive(queueGeneratedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}
	return pdFAIL; /* if uartNr was not in range */
}

/*!
* \fn ByseType_t nofReadyToSendPackInQueue(tUartNr uartNr)
* \brief Peeks at queue to see how many elements are inside
* \param uartNr: UART number where data was collected.
* \return Status if xQueueReceive has been successful
*/
BaseType_t nofGeneratedPayloadPacksInQueue(tUartNr uartNr)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return uxQueueMessagesWaiting( queueGeneratedPayloadPacks[uartNr] );
	}
	return 0; /* if uartNr was not in range */
}

/*!
* \fn ByseType_t peekAtReadyToSendPackInQueue(tUartNr uartNr)
* \brief Peeks at queue and copies next element into pPackage
* \param uartNr: UART number where data was collected.
* \param pPackage: Pointer to tWirelessPackage element where data will be copied
* \return Status if xQueuePeek has been successful
*/
BaseType_t peekAtGeneratedPayloadPackInQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueuePeek( queueGeneratedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}
	return 0; /* if uartNr was not in range */
}


/*!
* \fn ByseType_t pushToReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
* \brief Stores package in queue
* \param uartNr: UART number where data was collected.
* \param pPackage: The location of the package to be pushed to the queue
* \return Status if xQueueSendToBack has been successful
*/
BaseType_t pushToReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueueSendToBack(queueReceivedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}
	return pdFAIL; /* if uartNr was not in range */
}



/*!
* \fn static ByseType_t popFromReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
* \brief Pops a package from queue
* \param uartNr: UART number where data was collected.
* \param pPackage: Pointer to empty package
* \return Status if xQueueReceive has been successful
*/
static BaseType_t popFromReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueueReceive(queueReceivedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}
	return pdFAIL; /* if uartNr was not in range */
}

/*!
* \fn ByseType_t peekAtReceivedPayloadPacksQueue(tUartNr uartNr)
* \brief Peeks at queue and copies next element into pPackage
* \param uartNr: UART number where data was collected.
* \param pPackage: Pointer to tWirelessPackage element where data will be copied
* \return Status if xQueuePeek has been successful
*/
static BaseType_t peekAtReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return xQueuePeek( queueReceivedPayloadPacks[uartNr], pPackage, ( TickType_t ) pdMS_TO_TICKS(TRANSPORT_HANDLER_QUEUE_DELAY) );
	}
	return 0; /* if uartNr was not in range */
}


/*!
* \fn static ByseType_t nofReceivedPayloadPacksInQueue(tUartNr uartNr)
* \brief Peeks at queue to see how many elements are inside
* \param uartNr: UART number where data was collected.
* \return Status if xQueueReceive has been successful
*/
static BaseType_t nofReceivedPayloadPacksInQueue(tUartNr uartNr)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return uxQueueMessagesWaiting( queueReceivedPayloadPacks[uartNr] );
	}
	return 0; /* if uartNr was not in range */
}


/*!
* \fn ByseType_t freeSpaceInReceivedPayloadPacksQueue(tUartNr uartNr)
* \brief Returns number of free slots in queue
* \param uartNr: UART number where payload will be pushed out
* \return Number of elements that can still be pushed to this queue
*/
BaseType_t freeSpaceInReceivedPayloadPacksQueue(tUartNr uartNr)
{
	if(uartNr < NUMBER_OF_UARTS)
	{
		return ( QUEUE_NUM_OF_RECEIVED_PAYLOAD_PACKS - uxQueueMessagesWaiting( queueReceivedPayloadPacks[uartNr] ));
	}
	return 0; /* if uartNr was not in range */
}

/*!
* \fn void resetAllTransportPackageCounters(void)
* \brief Resets packageNr counter when sessionNr changed on other side
*/
void resetAllTransportPackageCounters(void)
{
	// ToDo: push all payload out and reset payload counter
}


