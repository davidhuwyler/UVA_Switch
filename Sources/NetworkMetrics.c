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
		transportHandler_GenerateTestPacketPair();
	}
}

/*!
* \fn void networkMetrics_TaskInit(void)
* \brief
*/
void networkMetrics_TaskInit(void)
{

}



