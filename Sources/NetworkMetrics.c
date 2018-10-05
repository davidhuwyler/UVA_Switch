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
#include "XF1.h" // xsprintf
#include "Shell.h"

/* global variables, only used in this file */
static xQueueHandle queueRequestNewTestPacketPair; /* Outgoing Requests for new TestPacketPairs for the TransportHandler */
static xQueueHandle queueTestPacketResults; /* Incoming TestPacketPair Results from the TransportHandler */
static const char* queueNameRequestNewTestPacketPair = {"RequestNewTestPacketPair"};
static const char* queueNameTestPacketResults = {"TestPacketResults"};
static tPackageBuffer testPackageBuffer[NUMBER_OF_UARTS];
static bool packetLossIndicatorForPLR[NUMBER_OF_UARTS][NOF_PACKS_FOR_PACKET_LOSS_RATIO];
static uint16_t indexOfPLRarray[NUMBER_OF_UARTS];
static uint16_t RTTraw[NUMBER_OF_UARTS], RTTfiltered[NUMBER_OF_UARTS], SBPPraw[NUMBER_OF_UARTS], SBPPfiltered[NUMBER_OF_UARTS], CPP[NUMBER_OF_UARTS], PLR[NUMBER_OF_UARTS],Q[NUMBER_OF_UARTS];
static uint16_t timeStampLastValidMetric[NUMBER_OF_UARTS];


/* prototypes of local functions */
static void initnetworkMetricsQueues(void);
static BaseType_t  generateTestPacketPairRequest();
static void calculateMetrics(void);
static void copyTestPackagePayload(tWirelessPackage* testPackage, tTestPackagePayload* payload);
static bool findPacketPairInBuffer(tWirelessPackage* sentPack1 , tWirelessPackage* sentPack2, tWirelessPackage* receivedPack1, tWirelessPackage* receivedPack2,uint16_t deviceID, uint16_t startPairNr);
static bool calculateMetric_RoundTripTime(uint16_t* roundTripTime, tWirelessPackage* sentPack,tWirelessPackage* receivedPack);
static bool calculateMetric_SenderBasedPacketPair(uint16_t* senderBasedPacketPair, tWirelessPackage* firstReceivedPackage,tWirelessPackage* secondReceivedPackage);
static bool calculateMetric_PacketLossRatio(uint16_t* packetLossRatio, uint8_t wirelessNr);
static void updatePacketLossRatioPacketNOK(uint8_t wirelessNr);
static void updatePacketLossRatioPacketOK(uint8_t wirelessNr);
static void calculateQ(uint16_t SBPP,uint16_t RTT,uint16_t PLR,uint16_t CPP, uint16_t* Q);
static void exponentialFilter(uint16_t* y_t, uint16_t* x_t, float a);
uint16_t getTimespan(uint16_t timestamp);

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
				/* update the PacketLossRatio Metric array*/
				tTestPackagePayload payload;
				copyTestPackagePayload(&package,&payload);
				if(payload.returned)
					updatePacketLossRatioPacketOK(package.devNum);


				/* Delete the test packet from the queue */
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

/*!
* \fn void networkMetrics_TaskInit(void)
* \brief calcualates network metrics with the data from test packets
*/
static void calculateMetrics(void)
{
	char infoBuf[100];

	static uint16_t currentPairNr[NUMBER_OF_UARTS] = {1,1,1,1};

	for(int wirelessLink = 0 ;  wirelessLink < NUMBER_OF_UARTS ; wirelessLink ++)
	{
		bool foundPair[NUMBER_OF_UARTS] = {false,false,false,false};

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

		for(int i = currentPairNr[wirelessLink] ;  i <= packageBuffer_getCurrentPayloadNR(&testPackageBuffer[wirelessLink]) ; i++)
		{


			//Found correct packet pair in buffer
			if(findPacketPairInBuffer(&sentPack1 , &sentPack2, &receivedPack1, &receivedPack2,wirelessLink,i))
			{

				foundPair[wirelessLink] = true;
				timeStampLastValidMetric[wirelessLink] = xTaskGetTickCount();

				XF1_xsprintf(infoBuf, "----------- WrelessLink%u Metrics -----------\r\n",wirelessLink);
				pushMsgToShellQueue(infoBuf);

				calculateMetric_RoundTripTime(&RTTraw[wirelessLink],&sentPack1,&receivedPack1);
				exponentialFilter(&RTTfiltered[wirelessLink],&RTTraw[wirelessLink],RRT_FILTER_PARAM);
				XF1_xsprintf(infoBuf, "Raw RRT Packet1 = %u ms \tFiltered RRT = %u ms\r\n", RTTraw[wirelessLink], RTTfiltered[wirelessLink]);
				pushMsgToShellQueue(infoBuf);
				calculateMetric_RoundTripTime(&RTTraw[wirelessLink],&sentPack2,&receivedPack2);
				exponentialFilter(&RTTfiltered[wirelessLink],&RTTraw[wirelessLink],RRT_FILTER_PARAM);
				XF1_xsprintf(infoBuf, "Raw RRT Packet2 = %u ms \tFiltered RRT = %u ms\r\n", RTTraw[wirelessLink], RTTfiltered[wirelessLink]);
				pushMsgToShellQueue(infoBuf);

				calculateMetric_SenderBasedPacketPair(&SBPPraw[wirelessLink],&receivedPack1,&receivedPack2);
				exponentialFilter(&SBPPfiltered[wirelessLink],&SBPPraw[wirelessLink],SBPP_FILTER_PARAM);
				XF1_xsprintf(infoBuf, "Raw SBPP = %u Byte/s \t\tFiltered SBPP = %u\r\n", SBPPraw[wirelessLink],SBPPfiltered[wirelessLink]);
				pushMsgToShellQueue(infoBuf);

				calculateMetric_PacketLossRatio(&PLR[wirelessLink], wirelessLink);
				XF1_xsprintf(infoBuf, "PLR = %u %% \r\n", PLR[wirelessLink]);
				pushMsgToShellQueue(infoBuf);

				CPP[wirelessLink] = config.CostPerPacketMetric[wirelessLink];
				XF1_xsprintf(infoBuf, "CPP = %u  \r\n", CPP[wirelessLink]);
				pushMsgToShellQueue(infoBuf);

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

		if(foundPair[wirelessLink] == false) //found no valid packet pair...
		{
			if(RTTraw[wirelessLink]<TIMEOUT_TEST_PACKET_RETURN)
				RTTraw[wirelessLink] = getTimespan(timeStampLastValidMetric[wirelessLink]);
			exponentialFilter(&RTTfiltered[wirelessLink],&RTTraw[wirelessLink],RRT_FILTER_PARAM);


			SBPPraw[wirelessLink] = 0;
			exponentialFilter(&SBPPfiltered[wirelessLink],&SBPPraw[wirelessLink],SBPP_FILTER_PARAM);
		}

		while(packageBuffer_getNextPackageOlderThanTimeout(&testPackageBuffer[wirelessLink],&tempPack,TIMEOUT_TEST_PACKET_RETURN))
		{
			/* update the PacketLossRatio Metric array*/
			tTestPackagePayload payload;
			copyTestPackagePayload(&tempPack,&payload);
			if(!payload.returned)
				updatePacketLossRatioPacketNOK(wirelessLink);

			if(currentPairNr[wirelessLink] < tempPack.payloadNr)
				currentPairNr[wirelessLink] = tempPack.payloadNr;
			vPortFree(tempPack.payload);
			tempPack.payload = NULL;
		}

		//packageBuffer_freeOlderThanCurrentPackage(&testPackageBuffer[wirelessLink]);
	}

	for(int wirelessLink = 0 ;  wirelessLink < NUMBER_OF_UARTS ; wirelessLink ++)
	{
		calculateMetric_PacketLossRatio(&PLR[wirelessLink], wirelessLink);

		calculateQ(SBPPfiltered[wirelessLink],RTTfiltered[wirelessLink],PLR[wirelessLink],CPP[wirelessLink],&Q[wirelessLink]);
		XF1_xsprintf(infoBuf, "Q%u = %u  \r\n", wirelessLink,Q[wirelessLink]);
		pushMsgToShellQueue(infoBuf);
	}


}

/*!
* \fn void exponentialFilter(uint16_t y_t, uint16_t y_t1, uint16_t x_t, float a)
* \brief implements an exponential filter
*  y_t is the current filtered value
*  x_t is the current raw value
*  a is the filter parameter (smaller = faster filter ) (0<a<1)
*/
static void exponentialFilter(uint16_t* y_t, uint16_t* x_t, float a)
{
	if(a<0)
		a=0;
	if(a>1)
		a=1;

	*y_t = (uint16_t)(a*((float)*y_t) + ((1-a)*((float)*x_t)));
}

/*!
* \fn bool calculateMetric_RoundTripTime(uint16_t* roundTripTime, tWirelessPackage* sentPack,tWirelessPackage* receivedPack)
* \brief calculates the RoundTripTime (RTT) Metric. This is the Time a TestPacket takes to go from the sendre to the receiver
* and again back to the sender. This metric is roughly twice the latency.
*/
static bool calculateMetric_RoundTripTime(uint16_t* roundTripTime, tWirelessPackage* sentPack,tWirelessPackage* receivedPack)
{
	tTestPackagePayload payloadSentPack, payloadReceivedPack;
	copyTestPackagePayload(sentPack,&payloadSentPack);
	copyTestPackagePayload(receivedPack,&payloadReceivedPack);

	if(payloadSentPack.sendTimestamp <= payloadReceivedPack.sendTimestamp)
		*roundTripTime = payloadReceivedPack.sendTimestamp-payloadSentPack.sendTimestamp;
	else
		*roundTripTime = (0xFFFF-payloadSentPack.sendTimestamp)+payloadReceivedPack.sendTimestamp;

	return true;
}

/*!
* \fn bool calculateMetric_SenderBasedPacketPair(uint16_t* senderBasedPacketPair, tWirelessPackage* firstReceivedPackage,tWirelessPackage* secondReceivedPackage)
* \brief calculates the SenderBasesPacketPair (SBPP) Metric. This metric is an estimation of the available maximum Bandwith
*/
static bool calculateMetric_SenderBasedPacketPair(uint16_t* senderBasedPacketPair, tWirelessPackage* firstReceivedPackage,tWirelessPackage* secondReceivedPackage)
{
	tTestPackagePayload payloadFirstReceivedPackage, payloadSecondReceivedPackage;
	copyTestPackagePayload(firstReceivedPackage,&payloadFirstReceivedPackage);
	copyTestPackagePayload(secondReceivedPackage,&payloadSecondReceivedPackage);

	uint16_t timeDelayBetwennReceivedPackages;

	if(payloadFirstReceivedPackage.sendTimestamp == payloadSecondReceivedPackage.sendTimestamp)
		return false;
	else if(payloadFirstReceivedPackage.sendTimestamp <= payloadSecondReceivedPackage.sendTimestamp)
		timeDelayBetwennReceivedPackages = payloadSecondReceivedPackage.sendTimestamp - payloadFirstReceivedPackage.sendTimestamp;
	else
		timeDelayBetwennReceivedPackages = (0xFFFF-payloadSecondReceivedPackage.sendTimestamp)+payloadFirstReceivedPackage.sendTimestamp;

	*senderBasedPacketPair = ((sizeof(tWirelessPackage)+sizeof(tTestPackagePayload))*1000)/timeDelayBetwennReceivedPackages;

	return true;
}

/*!
* \fn bool calculateMetric_PacketLossRatio(uint16_t* packetLossRatio)
* \brief calculates the PacketLossRatio (PLR) Metric. Number (0...100 [%]) which indicates how many packets that are lost
*/
static bool calculateMetric_PacketLossRatio(uint16_t* packetLossRatio,uint8_t wirelessNr)
{
	uint16_t nofLostPacks = 0;
	for(int i = 0 ; i < NOF_PACKS_FOR_PACKET_LOSS_RATIO ; i++)
	{
		if(packetLossIndicatorForPLR[wirelessNr][i])
			nofLostPacks ++;
	}


	*packetLossRatio = (100/NOF_PACKS_FOR_PACKET_LOSS_RATIO) * nofLostPacks;
	return true;
}

static void updatePacketLossRatioPacketNOK(uint8_t wirelessNr)
{
	packetLossIndicatorForPLR[wirelessNr][indexOfPLRarray[wirelessNr]] = true;
	indexOfPLRarray[wirelessNr] ++;
	indexOfPLRarray[wirelessNr] %= NOF_PACKS_FOR_PACKET_LOSS_RATIO;
}

/*!
* \fn bool static void updatePacketLossRatio(void)
* \brief updates the ringbuffer for the packet loss Ratio metric. Needs to be called at every insertion of a received test packet into the packet buffer
*/
static void updatePacketLossRatioPacketOK(uint8_t wirelessNr)
{
	packetLossIndicatorForPLR[wirelessNr][indexOfPLRarray[wirelessNr]] = false;
	indexOfPLRarray[wirelessNr] ++;
	indexOfPLRarray[wirelessNr] %= NOF_PACKS_FOR_PACKET_LOSS_RATIO;
}

/*!
* \fn void calculateQ(uint16_t SBPP,uint16_t RTT,uint16_t PLR,uint16_t CPP, uint16_t* Q)
* \brief calculates the quality Factor out of the metrics
*/
static void calculateQ(uint16_t SBPP,uint16_t RTT,uint16_t PLR,uint16_t CPP, uint16_t* Q)
{

	SBPP = SBPP * SCALING_FACTOR_SBPP_FOR_Q;
	RTT = RTT / SCALING_DIVIDER_RTT_FOR_Q;
	PLR = PLR / SCALING_DIVIDER_PLR_FOR_Q;

	if(PLR == 0)
		PLR = 1;

	*Q = SBPP / (RTT*PLR*CPP);
}

/*!
* \fn uint16_t networkMetrics_getResendDelayWirelessConn(void)
* \brief calculates the delay for resending packets according to the RTT Metric.
*  if test-packets are disabled, the configured value from the config file RESEND_DELAY_WIRELESS_CONN
*  is returned.
*/
uint16_t networkMetrics_getResendDelayWirelessConn(void)
{
	if(config.UseProbingPacksWlConn[0] || config.UseProbingPacksWlConn[1] || config.UseProbingPacksWlConn[2] || config.UseProbingPacksWlConn[3])
	{
		uint16_t longestRTT = 0;
		for(int i = 0 ; i < NUMBER_OF_UARTS ; i++)
		{
			if(Q[i] != 0  && longestRTT < RTTfiltered[i])
				longestRTT = RTTfiltered[i];
		}
		return longestRTT * 2;
	}
	else
		return config.ResendDelayWirelessConn;

}

/*!
* \fn bool findPacketPairInBuffer(tWirelessPackage* sentPack1 , tWirelessPackage* sentPack2, tWirelessPackage* receivedPack1, tWirelessPackage* receivedPack2,uint16_t deviceID, uint16_t startPairNr)
* \brief finds test packets from a single packet pair out of the packet buffer
*/
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
		if(config.UseProbingPacksWlConn[i] == true)
		{
			BaseType_t tempResult = xQueueSendToBack(queueRequestNewTestPacketPair, &request, ( TickType_t ) pdMS_TO_TICKS(NETWORK_METRICS_QUEUE_DELAY));
			if (tempResult != pdTRUE)
			{
			result = tempResult;
			}
		}
	}
	return result;
}

/*!
* \fn void copyTestPackagePayload(tWirelessPackage* testPackage, tTestPackagePayload* payload)
* \brief Extracts the payload out of an testpacket
*/
static void copyTestPackagePayload(tWirelessPackage* testPackage, tTestPackagePayload* payload)
{
	uint8_t *bytePtrPayload = (uint8_t*) payload;
	for (int i = 0; i < sizeof(tTestPackagePayload); i++)
	{
		bytePtrPayload[i] = testPackage->payload[i];
	}
}

uint16_t getTimespan(uint16_t timestamp)
{
	uint16_t osTick = xTaskGetTickCount();

	if(osTick >= timestamp)
		return(osTick-timestamp);
	else
		return (0xFFFF-osTick) + timestamp;
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
