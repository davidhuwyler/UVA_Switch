/*
 * NetworkMetrics.c
 *
 *  Created on: Sep 8, 2018
 *      Author: dave
 */

#include "Config.h"
#include "FreeRTOS.h"
#include "NetworkMetrics.h"
#include "TransportHandler.h"
#include "PackageBuffer.h"

/* global variables, only used in this file */
static xQueueHandle queueRequestNewTestPacketPair; /* Outgoing Requests for new TestPacketPairs for the TransportHandler */
static xQueueHandle queueTestPacketResults; /* Incoming TestPacketPair Results from the TransportHandler */
static const char* queueNameRequestNewTestPacketPair = {"RequestNewTestPacketPair"};
static const char* queueNameTestPacketResults = {"TestPacketResults"};
static tPackageBuffer testPackageBuffer[NUMBER_OF_UARTS];

/* prototypes of local functions */
static void initnetworkMetricsQueues(void);
static BaseType_t  generateTestPacketPairRequest();
static void calculateMetrics(void);
static void copyTestPackagePayload(tWirelessPackage* testPackage, tTestPackagePayload* payload);
static bool findPacketPairInBuffer(tWirelessPackage* sentPack1 , tWirelessPackage* sentPack2, tWirelessPackage* receivedPack1, tWirelessPackage* receivedPack2,uint16_t deviceID, uint16_t startPairNr);


/*!
* \fn void networkMetrics_TaskEntry(void)
* \brief Task computes the network metrics used for routing
*/
void networkMetrics_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.NetworkMetricsTaskInterval);
	tWirelessPackage package;
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */


	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		generateTestPacketPairRequest();

//		while(uxQueueMessagesWaiting( queueTestPacketResults ))
//				{
//							xQueueReceive(queueTestPacketResults, &package, 0);
//							vPortFree(package.payload);
//							package.payload = NULL;
//				}

		/* Put all Test-Packets from the Transport-Handler (queueTestPacketResults) into the PacketBuffer */
		while(uxQueueMessagesWaiting( queueTestPacketResults )) /* While Test Packets in the Queue */
		{
			xQueuePeek( queueTestPacketResults, &package, 0);
			if(packageBuffer_putNotUnique(&testPackageBuffer[package.devNum],&package) != true)
			{
				// No free space in the buffer
				break;
			}
			else
			{
				packageBuffer_setCurrentPayloadNR(&testPackageBuffer[package.devNum],package.payloadNr); //Set the highest TestPacket Number in the buffer
				xQueueReceive(queueTestPacketResults, &package, 0);
				vPortFree(package.payload);
				package.payload = NULL;
			}

		}

		calculateMetrics();
	}
}

/*!
* \fn void networkMetrics_TaskInit(void)
* \brief
*/
void networkMetrics_TaskInit(void)
{
	initnetworkMetricsQueues();
	for(int i = 0; i< NUMBER_OF_UARTS ; i++)
	{
		packageBuffer_init(&testPackageBuffer[i]);
		packageBuffer_setCurrentPayloadNR(&testPackageBuffer[i],1);
	}

}


static void calculateMetrics(void)
{
	static uint16_t currentPairNr[NUMBER_OF_UARTS] = {1,1,1,1};
	for(int deviceID = 0 ;  deviceID < NUMBER_OF_UARTS ; deviceID ++)
	{

		/*            					  -------------      -------------
		 * 	 Sender ---------->  		 | sentPack1   |    | sentPack2   |   	----------> Receiver
		 * 	 							 | Payload x   |	| Payload y   |
		 * 	 							 | ret = false |	| ret = false |
		 *            					  -------------      -------------
		 * 		       					  -------------      -------------
		 * 	 Sender <----------		 	 | rec.Pack1   |    | rec.Pack2   |   	<---------- Receiver
		 * 	 							 | Payload x   |	| Payload y   |
		 * 	 							 | ret = true  |	| ret = true  |
		 *            					  -------------      -------------
		 */

		tWirelessPackage sentPack1 , sentPack2, receivedPack1, receivedPack2, tempPack;
		tTestPackagePayload payload;

		for(int i = currentPairNr[deviceID] ;  i <= packageBuffer_getCurrentPayloadNR(&testPackageBuffer[deviceID]) ; i++)
		{
			if(findPacketPairInBuffer(&sentPack1 , &sentPack2, &receivedPack1, &receivedPack2,deviceID,i))
			{
				//Do metrics

				vPortFree(sentPack1.payload);
				sentPack1.payload = NULL;
				vPortFree(sentPack2.payload);
				sentPack2.payload = NULL;
				vPortFree(receivedPack1.payload);
				receivedPack1.payload = NULL;
				vPortFree(receivedPack2.payload);
				receivedPack2.payload = NULL;
			}
		}


		while(packageBuffer_getNextPackageOlderThanTimeout(&testPackageBuffer[deviceID],&tempPack,TIMEOUT_TEST_PACKET_RETURN))
		{
			//If pack is a sent pack then one pack was lost! bacause the received pack never came back...
			if(currentPairNr[deviceID] < tempPack.payloadNr)
				currentPairNr[deviceID] = tempPack.payloadNr;
			vPortFree(tempPack.payload);
			tempPack.payload = NULL;
		}

		packageBuffer_freeOlderThanCurrentPackage(&testPackageBuffer[deviceID]);
	}
}


static bool findPacketPairInBuffer(tWirelessPackage* sentPack1 , tWirelessPackage* sentPack2, tWirelessPackage* receivedPack1, tWirelessPackage* receivedPack2,uint16_t deviceID, uint16_t startPairNr)
{
	typedef enum eFindPacketPairState{ FIND_SENT_PACK1, FIND_SENT_PACK2, FIND_RECEIVED_PACK1, FIND_RECEIVED_PACK2,FOUND_ALL,FOUND_NOT_ALL} eFindPacketPairState;
	eFindPacketPairState state = FIND_SENT_PACK1;
	tWirelessPackage tempPack;
	tTestPackagePayload payload;

	while(state!= FOUND_ALL && state!=FOUND_NOT_ALL)
	{
		switch (state)
			{
				case FIND_SENT_PACK1:
					if(packageBuffer_getPackage(&testPackageBuffer[deviceID],&tempPack,startPairNr))
					{
						copyTestPackagePayload(&tempPack,&payload);
						if(tempPack.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST  &&  !payload.returned)
						{
							*sentPack1 = tempPack;
							state = FIND_SENT_PACK2;
						}
						else
						{
							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],&tempPack);
							vPortFree(tempPack.payload);
							tempPack.payload = NULL;
							state = FOUND_NOT_ALL;
						}
					}
					else
					{
						state = FOUND_NOT_ALL;
					}
					break;
				case FIND_SENT_PACK2:
					if(packageBuffer_getPackage(&testPackageBuffer[deviceID],&tempPack,startPairNr+1))
					{
						copyTestPackagePayload(&tempPack,&payload);
						if(tempPack.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND  &&  !payload.returned)
						{
							*sentPack2 = tempPack;
							state = FIND_RECEIVED_PACK1;
						}
						else
						{
							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
							vPortFree(sentPack1->payload);
							sentPack1->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],&tempPack);
							vPortFree(tempPack.payload);
							tempPack.payload = NULL;
							state = FOUND_NOT_ALL;
						}
					}
					else
					{
						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
						vPortFree(sentPack1->payload);
						sentPack1->payload = NULL;

						state = FOUND_NOT_ALL;
					}
					break;
				case FIND_RECEIVED_PACK1:
					if(packageBuffer_getPackage(&testPackageBuffer[deviceID],&tempPack,startPairNr))
					{
						copyTestPackagePayload(&tempPack,&payload);
						if(tempPack.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_FIRST  &&  payload.returned)
						{
							*receivedPack1 = tempPack;
							state = FIND_RECEIVED_PACK2;
						}
						else
						{
							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
							vPortFree(sentPack1->payload);
							sentPack1->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack2);
							vPortFree(sentPack2->payload);
							sentPack2->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],&tempPack);
							vPortFree(tempPack.payload);
							tempPack.payload = NULL;
							state = FOUND_NOT_ALL;
						}
					}
					else
					{
						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
						vPortFree(sentPack1->payload);
						sentPack1->payload = NULL;

						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack2);
						vPortFree(sentPack2->payload);
						sentPack2->payload = NULL;

						state = FOUND_NOT_ALL;
					}
					break;
				case FIND_RECEIVED_PACK2:
					if(packageBuffer_getPackage(&testPackageBuffer[deviceID],&tempPack,startPairNr+1))
					{
						copyTestPackagePayload(&tempPack,&payload);
						if(tempPack.packType == PACK_TYPE_NETWORK_TEST_PACKAGE_SECOND  &&  payload.returned)
						{
							*receivedPack2 = tempPack;
							state = FOUND_ALL;
						}
						else
						{
							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
							vPortFree(sentPack1->payload);
							sentPack1->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack2);
							vPortFree(sentPack2->payload);
							sentPack2->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],receivedPack1);
							vPortFree(receivedPack1->payload);
							receivedPack1->payload = NULL;

							packageBuffer_putNotUnique(&testPackageBuffer[deviceID],&tempPack);
							vPortFree(tempPack.payload);
							tempPack.payload = NULL;
							state = FOUND_NOT_ALL;
						}
					}
					else
					{
						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack1);
						vPortFree(sentPack1->payload);
						sentPack1->payload = NULL;

						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],sentPack2);
						vPortFree(sentPack2->payload);
						sentPack2->payload = NULL;

						packageBuffer_putNotUnique(&testPackageBuffer[deviceID],receivedPack1);
						vPortFree(receivedPack1->payload);
						receivedPack1->payload = NULL;

						state = FOUND_NOT_ALL;
					}
					break;
				default:
					state = FOUND_NOT_ALL;
					break;
			}
	}

	if(state == FOUND_ALL)
		return true;
	else
		return false;
}



/*!
* \fn void generateTestPacketPairRequest(void)
* \brief This function initializes the array of queues
*/
static BaseType_t  generateTestPacketPairRequest()
{
	static bool request = true;
	BaseType_t result = pdTRUE;

	/* Fill Queue with requests for TestPaketPairs for every UART */
	for( int i = 0 ; i < NUMBER_OF_UARTS ; i++)
	{
		BaseType_t tempResult = xQueueSendToBack(queueRequestNewTestPacketPair, &request, ( TickType_t ) pdMS_TO_TICKS(NETWORK_METRICS_QUEUE_DELAY));
		if (tempResult != pdTRUE)
		{
			result = tempResult;
		}
	}
	return result;
}

static void copyTestPackagePayload(tWirelessPackage* testPackage, tTestPackagePayload* payload)
{
	uint8_t *bytePtrPayload = (uint8_t*) payload;
	for (int i = 0; i < sizeof(tTestPackagePayload); i++)
	{
		bytePtrPayload[i] = testPackage->payload[i];
	}
}

/*!
* \fn void initnetworkMetricsQueues(void)
* \brief This function initializes the array of queues
*/
static void initnetworkMetricsQueues(void)
{
	static uint8_t xStaticQueueToAssemble[ QUEUE_NOF_TEST_PACKET_REQUESTS * sizeof(bool) ]; /* The variable used to hold the queue's data structure. */
	static uint8_t xStaticQueueToDisassemble[ QUEUE_NOF_TEST_PACKET_RESULTS * sizeof(tWirelessPackage) ]; /* The variable used to hold the queue's data structure. */
	static StaticQueue_t ucQueueStorageToAssemble; /* The array to use as the queue's storage area. */
	static StaticQueue_t ucQueueStorageToDisassemble; /* The array to use as the queue's storage area. */

	queueRequestNewTestPacketPair = xQueueCreateStatic( QUEUE_NOF_TEST_PACKET_REQUESTS, sizeof(bool), xStaticQueueToAssemble, &ucQueueStorageToAssemble);
	queueTestPacketResults = xQueueCreateStatic( QUEUE_NOF_TEST_PACKET_RESULTS, sizeof(tWirelessPackage), xStaticQueueToDisassemble, &ucQueueStorageToDisassemble);

	if( (queueRequestNewTestPacketPair == NULL) || (queueTestPacketResults == NULL) )
	{
		while(true){} /* malloc for queue failed */
	}
	vQueueAddToRegistry(queueRequestNewTestPacketPair, queueNameRequestNewTestPacketPair);
	vQueueAddToRegistry(queueTestPacketResults, queueNameTestPacketResults);
}

/*!
* \fn ByseType_t popFromRequestNewTestPacketPairQueue(bool* request)
* \brief Pops a package from queue
* \param request: Pointer to result
* \return Status if xQueueReceive has been successful
*/
BaseType_t popFromRequestNewTestPacketPairQueue(bool* request)
{
	return xQueueReceive(queueRequestNewTestPacketPair, request, ( TickType_t ) pdMS_TO_TICKS(NETWORK_METRICS_QUEUE_DELAY) );
}

/*!
* \fn ByseType_t pushToTestPacketResultsQueue(tTestPackageResults* results);
* \brief Stores results in queue
* \param tTestPackageResults: The location of the results to be pushed to the queue
* \return Status if xQueueSendToBack has been successful
*/
BaseType_t pushToTestPacketResultsQueue(tWirelessPackage* results)
{
	return xQueueSendToBack(queueTestPacketResults, results, ( TickType_t ) pdMS_TO_TICKS(NETWORK_METRICS_QUEUE_DELAY) );
}
