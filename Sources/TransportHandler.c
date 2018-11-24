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
#include "Logger.h"
#include "PanicButton.h"

/* --------------- prototypes ------------------- */
static bool processReceivedPayload(tWirelessPackage* pPackage);
static bool generateDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage);
static bool generateAckPackage(tWirelessPackage* pReceivedDataPack, tWirelessPackage* pAckPack);
static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage);
static bool generateTestDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage, bool returned,bool firstPackOfPacketPair);
static BaseType_t pushToGeneratedPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);
static void pushPayloadOut(tWirelessPackage* package);
static bool pushNextStoredPackOut(tUartNr wlConn);
static bool pushOldestPackOutIfTimeout(tUartNr wlConn, bool forced);
static void initTransportHandlerQueues(void);
static BaseType_t peekAtReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);
static BaseType_t nofReceivedPayloadPacksInQueue(tUartNr uartNr);
static BaseType_t popFromReceivedPayloadPacksQueue(tUartNr uartNr, tWirelessPackage* pPackage);
static void checkSessionNr(tWirelessPackage* pPackage);
static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy);


/* --------------- global variables -------------------- */
static xQueueHandle queueGeneratedPayloadPacks[NUMBER_OF_UARTS]; /* Outgoing data to wireless side */
static xQueueHandle queueReceivedPayloadPacks[NUMBER_OF_UARTS]; /* Outgoing data to wireless side */
static char* queueNameReadyToSendPacks[] = {"queueGeneratedPacksFromDev0", "queueGeneratedPacksFromDev1", "queueGeneratedPacksFromDev2", "queueGeneratedPacksFromDev3"};
static char* queueNameReceivedPayload[] = {"queueReceivedPacksFromDev0", "queueReceivedPacksFromDev1", "queueReceivedPacksFromDev2", "queueReceivedPacksFromDev3"};
static uint16_t payloadNumTracker[NUMBER_OF_UARTS];
static uint16_t sentAckNumTracker[NUMBER_OF_UARTS];
static uint16_t testPackNumTracker[NUMBER_OF_UARTS];
static tPackageBuffer sendBuffer[NUMBER_OF_UARTS];								/*Packets are stored which wait for the acknowledge */
static tPackageBuffer receiveBuffer[NUMBER_OF_UARTS];							/*Packets are stored which wait for reordering */
static bool remotePanicMode = false;

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
	static bool workaroundToStartUAVswitch = true;
	const TickType_t taskInterval = pdMS_TO_TICKS(config.TransportHandlerTaskInterval);
	tWirelessPackage package,pAckPack;
	bool request = true;
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */
	uint16_t latency;

	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		/* generate data packages and put those into the package queue */
		for(int deviceNr = 0; deviceNr<NUMBER_OF_UARTS; deviceNr++)
		{

			/*------------------------ Generate TestPackets if requested ---------------------------*/
			if ((popFromRequestNewTestPacketPairQueue(&request) == pdTRUE && config.UseProbingPacksWlConn[deviceNr] == true) || workaroundToStartUAVswitch)
			{
				workaroundToStartUAVswitch = false;  //Todo finde the rootcause why workaround needed...
				sendOutTestPackagePair(deviceNr, &package);
			}

			/*------------------ Generate Packages From Raw Data (Device Bytes)---------------------*/
			if (generateDataPackage(deviceNr, &package))
			{

				package.panicMode = PanicButton_GetVal();

				logger_incrementDeviceSentPack(package.devNum);
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
						if(packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr,&latency))
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
					if(packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr,&latency))
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

				checkSessionNr(&package);

			/*--------------> Incoming Package == DataPackage <-----------------*/
				if(package.packType == PACK_TYPE_DATA_PACKAGE)
				{
					remotePanicMode = package.panicMode;
					if(packageBuffer_putIfNotOld(&receiveBuffer[deviceNr],&package) != true)			//Put data-package into receiveBuffer
					{
						if(packageBuffer_getPackage(&receiveBuffer[deviceNr],&package,package.payloadNr,&latency))
						{
							vPortFree(package.payload);
							package.payload = NULL;
						}
						break;
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

						popFromReceivedPayloadPacksQueue(deviceNr, &package);
						vPortFree(package.payload);
						package.payload = NULL;
					}
				}

			/*--------------> Incoming Package == Acknowledge <-----------------*/
				else if(package.packType == PACK_TYPE_REC_ACKNOWLEDGE)
				{
					uint16_t payloadNrToAck = package.payloadNr;
					uint16_t payloadNrTransmissionOk = package.packNr;
					uint8_t wirelessConnNr = package.payload[0];
					uint16_t numberOfSendTries;
					bool gotApack = false;
					popFromReceivedPayloadPacksQueue(deviceNr, &package);
					vPortFree(package.payload);
					package.payload = NULL;

					//Delete Acknowledged package from all sendBuffer
					packageBuffer_setCurrentPayloadNR(&sendBuffer[deviceNr],payloadNrTransmissionOk);
					packageBuffer_freeOlderThanCurrentPackage(&sendBuffer[deviceNr]);
					
					while(packageBuffer_getPackageWithVar(&sendBuffer[deviceNr],&package,&numberOfSendTries,payloadNrToAck,&latency))
					{
						vPortFree(package.payload);
						package.payload = NULL;

						if(!gotApack)
						{
							gotApack = true;
							logger_logDeviceToDeviceLatency(package.devNum,(numberOfSendTries+1)*latency);
							logger_logModemLatency(wirelessConnNr,latency);
						}
					}
				}

			/*--------------> Incoming Package == NetworkTestPackage <----------*/

				else if (package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST || package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND)
				{
					/* Copy payload out of testpackage */
					tTestPackagePayload payload;
					uint8_t *bytePtrPayload = (uint8_t*) &payload;
					for (int i = 0; i < sizeof(tTestPackagePayload); i++)
					{
						bytePtrPayload[i] = package.payload[i];
					}

					/* Return the Testpackage if this device is not the Sender */
					bool packIsFirstOfPair = false;
					if(package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST)
						packIsFirstOfPair = true;
					if (!payload.returned && generateTestDataPackage(deviceNr, &package,true,packIsFirstOfPair))
					{
						if (pushToGeneratedPacksQueue(deviceNr,	&package) != pdTRUE)
						{
							vPortFree(package.payload);
							package.payload = NULL;
						}
						popFromReceivedPayloadPacksQueue(deviceNr, &package);
						vPortFree(package.payload);
						package.payload = NULL;
					}

					/* Test-Packet returned from receiver */
					else if(payload.returned)
					{
						/* Update the timestamp with the received time */
						//Copy payload out of testpackage
						tTestPackagePayload payload;
						uint8_t *bytePtrPayload = (uint8_t*) &payload;
						for (int i = 0; i < sizeof(tTestPackagePayload); i++)
						{
							bytePtrPayload[i] = package.payload[i];
						}
						payload.sendTimestamp =  xTaskGetTickCount();
						//Copy payload back into testpackage
						bytePtrPayload = (uint8_t*) &payload;
						for (int i = 0; i < sizeof(tTestPackagePayload); i++)
						{
							package.payload[i] = bytePtrPayload[i];
						}

						//Put the TestPackage into the queue for the NetworkMetrics
						tWirelessPackage tempPack;
						copyPackage(&package,&tempPack);
						pushToTestPacketResultsQueue(&tempPack);
						popFromReceivedPayloadPacksQueue(deviceNr, &package);
						vPortFree(package.payload);
						package.payload = NULL;
					}
				}
			}


			/*------------------------ Resend Unacknowledged Wireless Packages ---------------------------*/
			uint16_t numberOfResendAttempts;
			while(packageBuffer_getNextPackageOlderThanTimeoutWithVar(&sendBuffer[deviceNr],&package,&numberOfResendAttempts,networkMetrics_getResendDelayWirelessConn()))
			{
				if(numberOfResendAttempts<config.ResendCountWirelessConn)  //Resend
				{
					if(!packageBuffer_putWithVar(&sendBuffer[deviceNr],&package,(numberOfResendAttempts+1)))//Reinsert Package in the Buffer with new Timestamp
					{
						//Should never happen because place was freed 3 lines above... TODO Handle case
					}
					if (pushToGeneratedPacksQueue(deviceNr, &package) != pdTRUE)		//Put data-package into Queues
					{
						vPortFree(package.payload);
						package.payload = NULL;
						packageBuffer_getPackage(&sendBuffer[deviceNr],&package,package.payloadNr,&latency);
						vPortFree(package.payload);
						package.payload = NULL;
					}
				}
				else												//Max Number of resends reached... Delete Package
				{
					logger_incrementDeviceFailedToSendPack(package.devNum);
					vPortFree(package.payload);
					package.payload = NULL;
				}
			}



			/*------------------------ Send out all packages from the buffer which are in order ---------------------------*/
			while(packageBuffer_getNextOrderedPackage(&receiveBuffer[deviceNr],&package))
			{
				if(freeSpaceInTxByteQueue(MAX_14830_DEVICE_SIDE, package.devNum) >= package.payloadSize)
				{
					packageBuffer_setCurrentPayloadNR(&receiveBuffer[deviceNr], package.payloadNr);
					pushPayloadOut(&package);
					vPortFree(package.payload);
					package.payload = NULL;
					logger_incrementDeviceReceivedPack(package.devNum);
				}
				else
				{	//If the TX Byte Queue hasnt enough space, the package gets reinserted into the Buffer
					packageBuffer_put(&receiveBuffer[deviceNr],&package);
					vPortFree(package.payload);
					package.payload = NULL;
					break;
				}
			}
			/*------------------------ Delete received Wireless-Packets out of Order if timeOut has occurred ---------------------------*/
			while(packageBuffer_getNextPackageOlderThanTimeout(&receiveBuffer[deviceNr],&package,config.PayloadReorderingTimeout))
			{
				packageBuffer_setCurrentPayloadNR(&receiveBuffer[deviceNr], package.payloadNr);
				vPortFree(package.payload);
				package.payload = NULL;
				logger_incrementDeletedOutOfOrderPacks(package.devNum);
			}
			
			/*------------------------ Delete received Wireless-Packets with old payload---------------------------*/
			packageBuffer_freeOlderThanCurrentPackage(&receiveBuffer[deviceNr]);
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
	//static const uint8_t packHeaderBuf[PACKAGE_HEADER_SIZE - 1] = { PACK_START, PACK_TYPE_DATA_PACKAGE, 0, 0, 0, 0, 0, 0, 0, 0 };
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
	pAckPack->packNr = packageBuffer_getCurrentPayloadNR(&receiveBuffer[pReceivedDataPack->devNum]);
	pAckPack->payloadNr = pReceivedDataPack->payloadNr;
	pAckPack->payloadSize = sizeof(int8_t);	/* as payload, the sent Modem Numer is saved */
	/* get space for acknowladge payload (which consists of packNr of datapackage*/
	pAckPack->payload = (uint8_t*) FRTOS_pvPortMalloc(sizeof(int8_t));
	if(pAckPack->payload == NULL) /* malloc failed */
		return false;
	/* the payload is filled in the network handler if not */

	/* Fill payload with received Modem Nr of Data Pack -> Acknowledges go back to sender Modem*/
	pAckPack->payload[0] = pReceivedDataPack->receivedModemNr;

	return true;
}

/*!
* \fn static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage)
* \brief Function to generate a test data package pair used to determine the network metrics
*/
static void sendOutTestPackagePair(tUartNr deviceNr, tWirelessPackage* pPackage)
{
	if(generateTestDataPackage(deviceNr, pPackage,false,true))
	{
		tWirelessPackage tempPack;
		copyPackage(pPackage,&tempPack);

		pushToTestPacketResultsQueue(&tempPack);
		if(pushToGeneratedPacksQueue(deviceNr, pPackage) != pdTRUE)
		{
			vPortFree(pPackage->payload);
			pPackage->payload = NULL;
		}
	}
	if(generateTestDataPackage(deviceNr, pPackage,false,false))
	{
		tWirelessPackage tempPack;
		copyPackage(pPackage,&tempPack);
		pushToTestPacketResultsQueue(&tempPack);
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
static bool generateTestDataPackage(tUartNr deviceNr, tWirelessPackage* pPackage, bool returned,bool firstPackOfPacketPair)
{
	tTestPackagePayload payload;
	//static const uint8_t packHeaderBuf[PACKAGE_HEADER_SIZE - 1] = { PACK_START, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	if((deviceNr >= NUMBER_OF_UARTS) || (pPackage == NULL)) /* check validity of function parameters */
	{
		return false;
	}

	/* Put together package */

	/* Fill Payload	 */
	payload.returned = returned;
	if(returned)
	{
		pPackage->payloadNr = pPackage->payloadNr;

		//Extract old sendTimestamp from received Test-Package
		tTestPackagePayload oldPayload;
		uint8_t *bytePtrPayload = (uint8_t*) &oldPayload;
		for (int i = 0; i < sizeof(tTestPackagePayload); i++)
		{
			bytePtrPayload[i] = pPackage->payload[i];
		}
		payload.sendTimestamp =oldPayload.sendTimestamp;
	}
	if(!returned)
	{
		pPackage->payloadNr = ++testPackNumTracker[deviceNr];
		payload.sendTimestamp = xTaskGetTickCount();
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

	if(firstPackOfPacketPair)
		pPackage->packType = PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST;
	else
		pPackage->packType = PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND;
	pPackage->devNum = deviceNr;


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
* \fn void checkSessionNr(tWirelessPackage* pPackage)
* \brief Resets receivedBuffer if sessionNr changed
*/
static void checkSessionNr(tWirelessPackage* pPackage)
{
	static uint8_t lastSessionNr = 0;
	if(pPackage->sessionNr != lastSessionNr)
	{
		for(int i = 0 ; i < NUMBER_OF_UARTS ; i++)
		{
			packageBuffer_free(&receiveBuffer[i]);
		}

		if(pPackage->packType == PACK_TYPE_DATA_PACKAGE)
			packageBuffer_setCurrentPayloadNR(&receiveBuffer[pPackage->devNum], pPackage->payloadNr);
		lastSessionNr = pPackage->sessionNr;
	}
}

/*!
* \fn static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy)
* \brief Copies the content of the original package into the copy, allocates memory for payload of copy
* \return bool: true if successful, false otherwise
*/
static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy)
{
	*copy = *original;
	copy->payload = FRTOS_pvPortMalloc(original->payloadSize*sizeof(int8_t));
	if(copy->payload == NULL)
	{
		return false;
	}
	for(int cnt = 0; cnt < copy->payloadSize; cnt++)
	{
		copy->payload[cnt] = original->payload[cnt];
	}
	return true;
}
