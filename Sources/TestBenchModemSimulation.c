/*
 * RoutingAlgorithmTestBench.c
 *
 *  Created on: Oct 12, 2018
 *      Author: dave
 */
#include "PackageHandler.h"
#include "TestBenchModemSimulation.h"
#include "Config.h"

/* global variables, only used in this file */
static uint16_t byteErrorTestPatternWirelessLink[TESTBENCH_MODEM_SIMULATION_NOF_SCENARIOS][NUMBER_OF_UARTS][BYTE_ERROR_TEST_PATTERN_LENGTH] ={
		//Easy Scenario
		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000}},

		//Medium Scenario
		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 5000},
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 5000}},

		//Hard Scenario
		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 5000},
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 30991, 30992, 30993, 30994, 30995, 30996, 30997, 30998, 30999,31000},
		{110, 111, 112, 160, 161, 162, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 5000}},
};

static uint16_t latecyVariationTestPattern[TESTBENCH_MODEM_SIMULATION_NOF_SCENARIOS][LATENCY_VARIATION_TEST_PATTERN_LENGHT] = {
		//Easy Scenario
		{40,40,20,20,20,40,40,10,70,120,120,150,150,150,80,40,40,40,40,70},

		//Medium Scenario
		{80,80,50,50,60,60,80,80,140,220,220,300,300,300,160,80,80,80,80,140},

		//Hard Scenario
		{300,300,300,300,300,300,300,80,140,220,220,300,300,300,160,80,300,80,300,300}
};


/*!
* \fn  bool RoutingAlgorithmTestBench_getReceivePermission(uint8_t wirelessNr)
*  The SPI Handler can call this function to decide if the Byte should be discarded or used
*  This is only used, if the configuration ENABLE_ROUTING_ALGORITHM_TEST_BENCH is Enabled
*  \return true, the byte should get used
*/
bool TestBenchModemSimulation_getByteReceivePermission(uint8_t wirelessNr)
{
#ifdef USE_BYTE_LEVEL_ERRORS
	static uint16_t byteErrorPatternIndex[4];
	static uint16_t byteCounter[4];
	if(config.EnableRoutingAlgorithmTestBench)
	{
		byteCounter[wirelessNr] ++;
		if(byteErrorTestPatternWirelessLink[config.TestBenchModemSimulationScenario][wirelessNr][byteErrorPatternIndex[wirelessNr]] == byteCounter[wirelessNr])
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
#else
	return true;
#endif
}

/*!
* \fn uint16_t RoutingAlorithmTestBench_getNetworkHandlerDelay(void)
*  Network Handler calls this functin if RoutingArgorithmTestBench is enabled
*  It returns a varialble delay time for the Task, to sumulate different latecies of modems
*  \return taskt delay for the network handler
*/
uint16_t TestBenchModemSimulation_getNetworkHandlerDelay(void)
{
	static uint16_t delayPatternIndex = 0;
	uint16_t delay;
	if(config.EnableRoutingAlgorithmTestBench)
	{
		delay = latecyVariationTestPattern[config.TestBenchModemSimulationScenario][delayPatternIndex];
		delayPatternIndex ++;
		delayPatternIndex = delayPatternIndex % LATENCY_VARIATION_TEST_PATTERN_LENGHT;

		return delay;
	}

	return config.ResendDelayWirelessConn;
}
