/*
 * TestBenchMaster.h
 *
 *  Created on: Oct 19, 2018
 *      Author: dave
 */

#ifndef TEST_BENCH_MASTER_H_
#define TEST_BENCH_MASTER_H_

#include "Config.h"


//#define UAV_SWITCH_IS_TESTBENCH_MASTER

#ifdef UAV_SWITCH_IS_TESTBENCH_MASTER
#pragma message ( "Device is configured as Testbench Master! No Normal Operation possible" )
#define NOF_BYTES_UART_RECEIVE_BUFFER 200
#define NOF_BYTES_UART_SEND_BUFFER_TESTBENCH 1000
#else
#define NOF_BYTES_UART_RECEIVE_BUFFER 1
#define NOF_BYTES_UART_SEND_BUFFER_TESTBENCH 1
#endif

#define TIMEOUT_FOR_BYTES_TO_GO_THROUGH_TESTBENCH 3000

typedef struct sTestBenchByteBufferEntry
{
	uint8_t byteData;
	uint32_t sendingTimestamp;
	bool isEmpty;
} tTestBenchByteBufferEntry;


/*!
* \fn testBenchMaster_TaskEntry(void* p)
* \brief TestBench Master to test the different Routing Algorithms
*/
void testBenchMaster_TaskEntry(void* p);

/*!
* \fn testBenchMaster_TaskInit(void)
* \brief
*/
void testBenchMaster_TaskInit(void);

/*!
* \fn  testBenchMaster_TaskInit(void)
* \brief set the HW Buffer for the incomming UART Bytes
*/
void testBenchMaster_setUARTreceiveBuffer(void);

#endif /* TEST_BENCH_MASTER_H_ */
