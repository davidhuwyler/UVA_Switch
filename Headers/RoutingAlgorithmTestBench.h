/*
 * RoutingAlgorithmTestBench.h
 *
 *  Created on: Oct 12, 2018
 *      Author: dave
 */

#ifndef HEADERS_ROUTINGALGORITHMTESTBENCH_H_
#define HEADERS_ROUTINGALGORITHMTESTBENCH_H_


#define BYTE_ERROR_TEST_PATTERN_LENGTH 20
#define LATENCY_VARIATION_TEST_PATTERN_LENGHT 20

/*!
* \fn  bool RoutingAlgorithmTestBench_getReceivePermission(uint8_t wirelessNr)
*  The SPI Handler can call this function to decide if the Byte should be discarded or used
*  This is only used, if the configuration ENABLE_ROUTING_ALGORITHM_TEST_BENCH is Enabled
*  \return true, the byte should get used
*/
bool RoutingAlgorithmTestBench_getReceivePermission(uint8_t wirelessNr);

/*!
* \fn uint16_t RoutingAlorithmTestBench_getNetworkHandlerDelay(void)
*  Network Handler calls this functin if RoutingArgorithmTestBench is enabled
*  It returns a varialble delay time for the Task, to sumulate different latecies of modems
*  \return taskt delay for the network handler
*/
uint16_t RoutingAlorithmTestBench_getNetworkHandlerDelay(void);

#endif /* HEADERS_ROUTINGALGORITHMTESTBENCH_H_ */
