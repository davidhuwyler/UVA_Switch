/*
 * NetworkMetrics.h
 *
 *  Created on: Sep 8, 2018
 *      Author: dave
 */

#ifndef HEADERS_NETWORKMETRICS_H_
#define HEADERS_NETWORKMETRICS_H_

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



#endif /* HEADERS_NETWORKMETRICS_H_ */
