/*
 * NetworkMetrics.h
 *
 *  Created on: Sep 8, 2018
 *      Author: dave
 */

#ifndef HEADERS_NETWORKMETRICS_H_
#define HEADERS_NETWORKMETRICS_H_

#include "PackageHandler.h"

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
#define QUEUE_NOF_TEST_PACKET_RESULTS   			50

/*! \def TIMEOUT_TEST_PACKET_RETURN
*  \brief Timeout for the Test-Packet to return to the sender [ms]
*/
#define TIMEOUT_TEST_PACKET_RETURN     				2000

/*! \def NOF_PACKS_FOR_PACKET_LOSS_RATIO
*  \brief How many Test-Packets should be taken into account for the RLR metric
*/
#define NOF_PACKS_FOR_PACKET_LOSS_RATIO     		20


#define RRT_FILTER_PARAM 0.9
#define SBPP_FILTER_PARAM 0.9
#define MIN_RTT_RESEND_DELAY 50
#define MAX_RTT_RESEND_DELAY 10000
#define SECURITY_FACTOR_RTT_TO_RESEND_DELAY 2

#define SCALING_FACTOR_SBPP_FOR_Q 10
#define SCALING_DIVIDER_RTT_FOR_Q 6
#define SCALING_DIVIDER_PLR_FOR_Q 6

#define BANDWITH_USAGE_PER_CHANNEL 0.5

#define Q_HIGH_THRESHOLD 300
#define Q_MID_THRESHOLD 50
#define Q_LOW_THRESHOLD 20

//#define PRINT_METRICS
//#define PRINT_Q
//#define PRINT_WIRELESSLINK_TO_USE
#define OUTPUT_USED_WIRELESSLINK_GPIOS


#define HARD_RULE_NOF_RESEND_BEFORE_REDUNDAND_THREE 3
#define HARD_RULE_NOF_RESEND_BEFORE_REDUNDAND_ALL 4
#define HARD_RULE_NOF_SAME_RULE_VARIANT_2 5

typedef enum
{
	ROUTING_ALGORITHM_RULE_NR_1,
	ROUTING_ALGORITHM_RULE_NR_2,
	ROUTING_ALGORITHM_RULE_NR_3,
	ROUTING_ALGORITHM_RULE_NR_4,
	ROUTING_ALGORITHM_RULE_NR_5,
	ROUTING_ALGORITHM_RULE_PANIC_MODE
} enumRoutingAlgoRules_t;

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
* \fn uint16_t networkMetrics_getResendDelayWirelessConn(void)
* \brief calculates the delay for resending packets according to the RTT Metric.
*  if test-packets are disabled, the configured value from the config file RESEND_DELAY_WIRELESS_CONN
*  is returned.
*/
uint16_t networkMetrics_getResendDelayWirelessConn(void);

/*!
* \fn  void networkMetrics_getLinksToUse(uint16_t bytesToSend,bool* wirelessLinksToUseParam)
*  in the Bool-Array wirelessLinksToUseParam the wireless links to use get saved. They are choosen by the routingAlgorithm
*  if priorityData==true, the Data is handled with priority
*  \return true, if packet has a link to be sent, false if no link is available at the moment
*/
bool networkMetrics_getLinksToUse(uint16_t bytesToSend,bool* wirelessLinksToUseParam, uint16_t payloadNr, uint8_t deviceNr);

/*!
* \fn ByseType_t popFromRequestNewTestPacketPairQueue(bool* request)
* \brief Pops a package from queue
* \param request: Pointer to result
* \return Status if xQueueReceive has been successful
*/
BaseType_t popFromRequestNewTestPacketPairQueue(bool* request);

/*!
* \fn BaseType_t pushToTestPacketResultsQueue(tWirelessPackage* results);
* \brief Stores results in queue
* \param tTestPackageResults: The location of the results to be pushed to the queue
* \return Status if xQueueSendToBack has been successful
*/
BaseType_t pushToTestPacketResultsQueue(tWirelessPackage* results);


#endif /* HEADERS_NETWORKMETRICS_H_ */
