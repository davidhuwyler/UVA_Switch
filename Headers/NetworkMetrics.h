/*
 * NetworkMetrics.h
 *
 *  Created on: Sep 8, 2018
 *      Author: dave
 */

#ifndef HEADERS_NETWORKMETRICS_H_
#define HEADERS_NETWORKMETRICS_H_

/*! \def NETWORK_METRICS_QUEUE_DELAY
*  \brief Number of ticks to wait on byte queue operations within this task
*/
#define NETWORK_METRICS_QUEUE_DELAY					0

/*! \def QUEUE_NOF_TEST_PACKET_REQUESTS
*  \brief how many requests for test packet pairs fit into the queue
*/
#define QUEUE_NOF_TEST_PACKET_REQUESTS				10

/*! \def QUEUE_NOF_TEST_PACKET_RESULTS
*  \brief Queue length of test packets results
*/
#define QUEUE_NOF_TEST_PACKET_RESULTS   			20

/*! \struct sTestPackageResults
*  \brief Structure holds the results of the Test-Packets
*/
typedef struct sTestPackageResults
{
	uint16_t timeBetweenPacketPair;		/* Delay between the two test packets */
	uint16_t rtt1;  					/* Round trip time of the first packet */
	uint16_t rtt2;  					/* Round trip time of the second packet */
	uint16_t maxQueueLength1; 			/* max Byte queue Length first packet*/
	uint16_t maxQueueLength2; 			/* max Byte queue Length second packet */
} tTestPackageResults;

/*!
* \fn void networkMetrics_TaskEntry(void)
* \brief Task computes the network metrics used for routing
*/
void networkMetrics_TaskEntry(void* p);

/*!
* \fn void networkMetrics_TaskInit(void)
* \brief
*/
void networkMetrics_TaskInit(void);

/*!
* \fn ByseType_t popFromRequestNewTestPacketPairQueue(bool* request)
* \brief Pops a package from queue
* \param request: Pointer to result
* \return Status if xQueueReceive has been successful
*/
BaseType_t popFromRequestNewTestPacketPairQueue(bool* request);

/*!
* \fn ByseType_t pushToTestPacketResultsQueue(tTestPackageResults* results);
* \brief Stores results in queue
* \param tTestPackageResults: The location of the results to be pushed to the queue
* \return Status if xQueueSendToBack has been successful
*/
BaseType_t pushToTestPacketResultsQueue(tTestPackageResults* results);


#endif /* HEADERS_NETWORKMETRICS_H_ */
