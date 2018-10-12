/*
 * RoutingAlgorithmTestBench.c
 *
 *  Created on: Oct 12, 2018
 *      Author: dave
 */
#include "PackageHandler.h"
#include "RoutingAlgorithmTestBench.h"
#include "Config.h"

/* global variables, only used in this file */
static uint16_t byteErrorTestPatternWirelessLink[NUMBER_OF_UARTS][BYTE_ERROR_TEST_PATTERN_LENGTH] = {{110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 500},
																									  { 50, 100, 150, 200, 250, 500, 550, 600, 650, 651, 652, 653, 654, 655, 656, 657, 658, 659, 660, 2000},
																									  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
																									  {110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 5000}};
static uint16_t latecyVariationTestPattern[LATENCY_VARIATION_TEST_PATTERN_LENGHT] = {40,40,20,20,20,40,40,10,70,120,120,150,150,150,80,40,40,40,40,70};

/*!
* \fn  bool RoutingAlgorithmTestBench_getReceivePermission(uint8_t wirelessNr)
*  The SPI Handler can call this function to decide if the Byte should be discarded or used
*  This is only used, if the configuration ENABLE_ROUTING_ALGORITHM_TEST_BENCH is Enabled
*  \return true, the byte should get used
*/
bool RoutingAlgorithmTestBench_getReceivePermission(uint8_t wirelessNr)
{
	static uint16_t byteErrorPatternIndex[4];
	static uint16_t byteCounter[4];
	if(config.EnableRoutingAlgorithmTestBench)
	{
		byteCounter[wirelessNr] ++;
		if(byteErrorTestPatternWirelessLink[wirelessNr][byteErrorPatternIndex[wirelessNr]] == byteCounter[wirelessNr])
		{
			byteErrorPatternIndex[wirelessNr] ++;
			byteErrorPatternIndex[wirelessNr] = byteErrorPatternIndex[wirelessNr] % BYTE_ERROR_TEST_PATTERN_LENGTH;
			if(byteErrorPatternIndex[wirelessNr] == 0)
				byteCounter[wirelessNr]=0;

			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		return true;
	}
}

/*!
* \fn uint16_t RoutingAlorithmTestBench_getNetworkHandlerDelay(void)
*  Network Handler calls this functin if RoutingArgorithmTestBench is enabled
*  It returns a varialble delay time for the Task, to sumulate different latecies of modems
*  \return taskt delay for the network handler
*/
uint16_t RoutingAlorithmTestBench_getNetworkHandlerDelay(void)
{
	static uint16_t delayPatternIndex = 0;
	uint16_t delay;
	if(config.EnableRoutingAlgorithmTestBench)
	{
		delay = latecyVariationTestPattern[delayPatternIndex];
		delayPatternIndex ++;
		delayPatternIndex = delayPatternIndex % LATENCY_VARIATION_TEST_PATTERN_LENGHT;

		return delay;
	}

	return config.ResendDelayWirelessConn;
}
