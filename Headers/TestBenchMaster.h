/*
 * TestBenchMaster.h
 *
 *  Created on: Oct 19, 2018
 *      Author: dave
 */

#ifndef TEST_BENCH_MASTER_H_
#define TEST_BENCH_MASTER_H_


#define NOF_BYTES_UART_RECEIVE_BUFFER 100

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
