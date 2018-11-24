#include <Config.h>
#include <FreeRTOS.h>
#include <NetworkHandler.h>
#include <portmacro.h>
#include <projdefs.h>
#include <PE_Types.h>
#include <queue.h>
#include <stdio.h> // modulo
#include <stdbool.h>
#include <stdint.h>
#include "UTIL1.h" // strcat
#include <string.h> // strlen
#include <Shell.h> // to print out debugInfo
#include <task.h>
#include <ThroughputPrintout.h>
#include "Logger.h"
#include "LedRed.h"
#include "TransportHandler.h"
#include "RNG.h"
#include "PackageHandler.h"
#include "NetworkMetrics.h"
#include "TestBenchModemSimulation.h"


/* global variables, only used in this file */
//static tWirelessPackage unacknowledgedPackages[MAX_NUMBER_OF_UNACK_PACKS_STORED];
//static bool unacknowledgedPackagesOccupiedAtIndex[MAX_NUMBER_OF_UNACK_PACKS_STORED];
//static int numberOfUnacknowledgedPackages;
//static uint16_t sentPackNumTracker[NUMBER_OF_UARTS];
//static volatile bool ackReceived[NUMBER_OF_UARTS];
//static uint8_t costFunctionPerWlConn[NUMBER_OF_UARTS];

/* prototypes of local functions */
static void initNetworkHandlerQueues(void);
static void initSempahores(void);
static bool processAssembledPackage(tUartNr wlConn);
static bool sendGeneratedWlPackage(tWirelessPackage* pPackage, tUartNr rawDataUartNr);
static void oneToOnerouting(tUartNr deviceNr, bool* wlConnToUse);
static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy);


/*!
* \fn void networkHandler_TaskEntry(void)
* \brief Task generates packages from received bytes (received on device side) and sends those down to
* the packageHandler for transmission.
* When acknowledges are configured, resending is handled here.
*/
void networkHandler_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.NetworkHandlerTaskInterval);
	tWirelessPackage package;
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */

	for(;;)
	{
		if(config.EnableRoutingAlgorithmTestBench)
			vTaskDelayUntil( &xLastWakeTime, TestBenchModemSimulation_getNetworkHandlerDelay()); /* Wait for the next cycle */
		else
			vTaskDelayUntil( &xLastWakeTime, taskInterval); /* Wait for the next cycle */


		/* generate data packages and put those into the package queue */
		for(int deviceNr = 0; deviceNr<NUMBER_OF_UARTS; deviceNr++)
		{
			/* push generated wireless packages out on wireless side */
			//if( nofGeneratedPayloadPacksInQueue(deviceNr) > 0)
			while( nofGeneratedPayloadPacksInQueue(deviceNr) > 0)
			{
				/* find wl connection to use for this package */
				bool wlConnToUse[] = {false, false, false, false};
				bool packSent = false;

				if(peekAtGeneratedPayloadPackInQueue(deviceNr, &package) == pdTRUE) /* peeking at package from upper handler successful? */
				{
					// Test-Packet: No routing needed
					if(package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST || package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND)
					{
						oneToOnerouting(deviceNr, wlConnToUse);
					}
					// Acknowledges go back through the same modem as the payload came
					else if(package.packType == PACK_TYPE_REC_ACKNOWLEDGE)
					{
						oneToOnerouting(package.payload[0], wlConnToUse);
					}
					// Panic Mode! Use all links
					else if(package.panicMode)
					{
						wlConnToUse[0] = true;
						wlConnToUse[1] = true;
						wlConnToUse[2] = true;
						wlConnToUse[3] = true;
					}
					// Data-Packet gets routed with a routing algorithm
					else if(!networkMetrics_getLinksToUse(sizeof(tWirelessPackage)+package.payloadSize, wlConnToUse, package.payloadNr,package.devNum))
					{
						//No link available at the moment... Dump Packet
						packSent = true;
					}

					for(int wlConn = 0; wlConn < NUMBER_OF_UARTS; wlConn++)
					{
						/* this wlconn is configured for the desired priority and there is space in the queue of next handler? */
						if( (wlConnToUse[wlConn] == true) && (freeSpaceInPackagesToDisassembleQueue(wlConn)) )
						{

								tWirelessPackage tmpPack;
								copyPackage(&package, &tmpPack);

								//IF Acknowledge, safe the Modem which the Ack is sent with (only needed for Logging...)
								if(tmpPack.packType == PACK_TYPE_REC_ACKNOWLEDGE)
									tmpPack.payload[0] = wlConn;

								//Send the Pack
								if(sendGeneratedWlPackage(&tmpPack, wlConn) == false) /* send the generated package down and store it internally if ACK is configured */
								{
									/* package couldnt be sent and payload was freed! don't access package anymore! */
									break; /* exit innner for loop */
								}

								//Logging...
								if(tmpPack.packType == PACK_TYPE_DATA_PACKAGE)
									logger_incrementWirelessSentPack(wlConn);
								packSent = true;
						}
					}
				}
				if(packSent)
				{
					popFromGeneratedPacksQueue(deviceNr, &package); /* this is done here because if two wlConn configured with same priority, package cant be removed twice */
					vPortFree(package.payload);
					package.payload = NULL;
				}
			}


			//Route the packets from the Package Handler to the Transporthandler...
			while(nofAssembledPacksInQueue(deviceNr) > 0)
			{
				processAssembledPackage(deviceNr);
			}
		}
	}
}

/*!
* \fn void networkHandler_TaskInit(void)
* \brief Initializes all queues that are declared within network handler
* and generates random session number
*/
void networkHandler_TaskInit(void)
{
	initNetworkHandlerQueues();
}

/*!
* \fn void initNetworkHandlerQueues(void)
* \brief This function initializes the array of queues
*/
static void initNetworkHandlerQueues(void)
{
}

/*!
* \fn static bool sendGeneratedWlPackage(tWirelessPackage* pPackage, tUartNr wlConn)
* \brief Sends the generated package to package handler for sending
* \param pPackage: pointer to package that should be sent to out
* \param wlConn: wireless connection where package should be sent out on this first try
* \return true if a package was sent to package handler and stored in buffer successfully
*/
static bool sendGeneratedWlPackage(tWirelessPackage* pPackage, tUartNr wlConn)
{
	char infoBuf[70];

	if(pushToPacksToDisassembleQueue(wlConn, pPackage) != pdTRUE)
	{
		XF1_xsprintf(infoBuf, "%u: Warning: Couldn't push newly generated package from device %u to package queue on wl conn %u \r\n", xTaskGetTickCount(), pPackage->devNum, wlConn);
		pushMsgToShellQueue(infoBuf);
		vPortFree(pPackage->payload);
		pPackage->payload = NULL;
		numberOfDroppedPackages[wlConn]++;
		return false;
	}
	/* update throughput printout */
	numberOfPacksSent[wlConn]++;
	numberOfPayloadBytesSent[wlConn] += pPackage->payloadSize;
	return true; /* package couldnt be stored in unacknowledgedPackagesArray */
}

/*!
* \fn static uint8_t  findWlConnForDevice(tUartNr deviceNr, int prio)
* \brief Checks which wireless connection number is configured with the desired priority
* \return wlConnectionToUse: a number between 0 and (NUMBER_OF_UARTS-1). This priority is not configured if NUMBER_OF_UARTS is returned.
*/
static void oneToOnerouting(tUartNr deviceNr, bool* wlConnToUse)
{
	if(deviceNr == 0)
	{
		wlConnToUse[0] = true;
		wlConnToUse[1] = false;
		wlConnToUse[2] = false;
		wlConnToUse[3] = false;
	}
	else if(deviceNr == 1)
	{
		wlConnToUse[0] = false;
		wlConnToUse[1] = true;
		wlConnToUse[2] = false;
		wlConnToUse[3] = false;
	}
	else if(deviceNr == 2)
	{
		wlConnToUse[0] = false;
		wlConnToUse[1] = false;
		wlConnToUse[2] = true;
		wlConnToUse[3] = false;
	}
	else if(deviceNr == 3)
	{
		wlConnToUse[0] = false;
		wlConnToUse[1] = false;
		wlConnToUse[2] = false;
		wlConnToUse[3] = true;
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
}

/*!
* \fn static bool processAssembledPackage(tUartNr wirelessConnNr)
* \brief Pops received package from queue and checks if it is ACK or Data package.
* ACK package -> deletes the package from the buffer where we wait for ACKS.
* data package -> generates ACK and sends it to package handler queue for packages to send.
* \param wirelessConnNr: The wireless device number where we look for received packages
*/
static bool processAssembledPackage(tUartNr wlConn)
{
	tWirelessPackage package;
	static char infoBuf[150];

	/* if it is a data package -> check if there is enough space on byte queue of device side */
	if(peekAtAssembledPackQueue(wlConn, &package) != pdTRUE) /* peek at package to find out payload size for space on Device Tx Bytes queue */
	{
		return false; /* peek not successful */
	}

	/* no space for package in transport handler or no space for acknowledge in package handler */
	if((package.packType == PACK_TYPE_DATA_PACKAGE) && (freeSpaceInReceivedPayloadPacksQueue(package.devNum) <= 0) ||
	   (package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST) && (freeSpaceInReceivedPayloadPacksQueue(package.devNum) <= 0) ||
	   (package.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND) && (freeSpaceInReceivedPayloadPacksQueue(package.devNum) <= 0) ||
	   ((package.packType == PACK_TYPE_REC_ACKNOWLEDGE) && (freeSpaceInPackagesToDisassembleQueue(wlConn) <= 0)) )
	{
		return false; /* not enough space */
	}
	/* pop package from queue to send it out */
	if(popAssembledPackFromQueue(wlConn, &package) != pdTRUE) /* actually remove package from queue */
	{
		return false; /* coun't be removed */
	}
	if(package.packType <= PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND) /* Known Package type received */
	{
		/* check if data is valid */
		if(package.payloadSize > PACKAGE_MAX_PAYLOAD_SIZE)
		{
			package.payloadSize = PACKAGE_MAX_PAYLOAD_SIZE;
		}
		if(package.devNum > NUMBER_OF_UARTS)
		{
			package.devNum = NUMBER_OF_UARTS-1;
		}

		if(package.packType == PACK_TYPE_DATA_PACKAGE)
		{
			logger_incrementWirelessReceivedPack(wlConn);
			package.receivedModemNr = wlConn;
		}

		/* push package to Trandport handler for processing payload */
		pushToReceivedPayloadPacksQueue(package.devNum, &package);
	}

	else
	{
		XF1_xsprintf(infoBuf, "%u: Error: invalid package type received on wireless connection %u\r\n", xTaskGetTickCount(), wlConn);
		pushMsgToShellQueue(infoBuf);
		FRTOS_vPortFree(package.payload);
	}
	return true;
}
