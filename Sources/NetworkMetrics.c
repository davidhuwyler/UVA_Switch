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

/* global variables, only used in this file */
static xQueueHandle queueRequestNewTestPacketPair; /* Outgoing Requests for new TestPacketPairs for the TransportHandler */
static xQueueHandle queueTestPacketResults; /* Incoming TestPacketPair Results from the TransportHandler */
static const char* queueNameRequestNewTestPacketPair = {"RequestNewTestPacketPair"};
static const char* queueNameTestPacketResults = {"TestPacketResults"};

/* prototypes of local functions */
static void initnetworkMetricsQueues(void);
static BaseType_t  generateTestPacketPairRequest();




/*!
* \fn void networkMetrics_TaskEntry(void)
* \brief Task computes the network metrics used for routing
*/
void networkMetrics_TaskEntry(void* p)
{
	const TickType_t taskInterval = pdMS_TO_TICKS(config.NetworkMetricsTaskInterval);
//	tWirelessPackage package;
	TickType_t xLastWakeTime = xTaskGetTickCount(); /* Initialize the lastWakeTime variable with the current time. */


	for(;;)
	{
		vTaskDelayUntil( &xLastWakeTime, taskInterval ); /* Wait for the next cycle */
		generateTestPacketPairRequest();
	}
}

/*!
* \fn void networkMetrics_TaskInit(void)
* \brief
*/
void networkMetrics_TaskInit(void)
{
	initnetworkMetricsQueues();
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

/*!
* \fn void initnetworkMetricsQueues(void)
* \brief This function initializes the array of queues
*/
static void initnetworkMetricsQueues(void)
{
	static uint8_t xStaticQueueToAssemble[ QUEUE_NOF_TEST_PACKET_REQUESTS * sizeof(bool) ]; /* The variable used to hold the queue's data structure. */
	static uint8_t xStaticQueueToDisassemble[ QUEUE_NOF_TEST_PACKET_RESULTS * sizeof(tTestPackageResults) ]; /* The variable used to hold the queue's data structure. */
	static StaticQueue_t ucQueueStorageToAssemble; /* The array to use as the queue's storage area. */
	static StaticQueue_t ucQueueStorageToDisassemble; /* The array to use as the queue's storage area. */

	queueRequestNewTestPacketPair = xQueueCreateStatic( QUEUE_NOF_TEST_PACKET_REQUESTS, sizeof(bool), xStaticQueueToAssemble, &ucQueueStorageToAssemble);
	queueTestPacketResults = xQueueCreateStatic( QUEUE_NOF_TEST_PACKET_RESULTS, sizeof(tTestPackageResults), xStaticQueueToDisassemble, &ucQueueStorageToDisassemble);

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
BaseType_t pushToTestPacketResultsQueue(tTestPackageResults* results)
{
	return xQueueSendToBack(queueTestPacketResults, results, ( TickType_t ) pdMS_TO_TICKS(NETWORK_METRICS_QUEUE_DELAY) );
}
