#ifndef HEADERS_LOGGER_H_
#define HEADERS_LOGGER_H_

#include "FRTOS.h" /* tBaseType */
#include "SpiHandler.h" /* tUartNr */
#include "PackageHandler.h" /* tWirelessPackage */

/*! \enum eRxTxPackage
*  \brief Enumeration if a package was sent or received from this device.
*/
typedef enum eRxTxPackage
{
	RECEIVED_PACKAGE,
	SENT_PACKAGE,
	NOF_RXTX_PACK_TYPES /* needs to be last in the enum list */
} tRxTxPackage;


void logger_TaskEntry(void* p);

void logger_TaskInit(void);

/*!
* \fn BaseType_t pushPackageToLoggerQueue(tWirelessPackage package, tRxTxPackage rxTxPackage, tUartNr uartNr)
* \brief Logs package content and frees memory of package when it is popped from queue
* \param package: The wireless package itself
* \param rxTxPackage: Information weather this is a received package or a sent package
* \param wlConnNr: wireless connection number over which package was received/sent
* \return pdTRUE if successful, pdFAIL if unsuccessful:
*/
BaseType_t pushPackageToLoggerQueue(tWirelessPackage* pPackage, tRxTxPackage rxTxPackage, tUartNr wlConnNr);


BaseType_t pushByteToLoggerQueue(uint8_t byte, tRxTxPackage rxTxPackage, tUartNr wlConnNr);


void logger_incrementDeviceSentPack(tUartNr deviceNr);
void logger_incrementDeviceReceivedPack(tUartNr deviceNr);
void logger_incrementWirelessSentPack(tUartNr wireLessNr);
void logger_incrementWirelessReceivedPack(tUartNr wireLessNr);
void logger_incrementDeviceFailedToSendPack(tUartNr deviceNr);
void logger_incremenReceivedFaultyPack(tUartNr wirelessNr);
void logger_incrementDeletedOutOfOrderPacks(tUartNr deviceNr);
void logger_logDeviceToDeviceLatency(tUartNr deviceNr,uint16_t latency);
void logger_logModemLatency(tUartNr wirelessNr,uint16_t latency);

//#define DO_PACK_AND_BYTE_LOGGING
#define DO_OVERVIEW_LOGGING

/*! \def MAX_DELAY_LOGGER_MS
*  \brief Maximal delay on queue operations inside Logger task.
*/
#define MAX_DELAY_LOGGER_QUEUE_OPERATION_MS					(5)


/*! \def QUEUE_NUM_OF_PACK_LOG_ENTRIES
*  \brief Size of queue for log entries.
*  */
#ifdef DO_PACK_AND_BYTE_LOGGING
#define QUEUE_NUM_OF_PACK_LOG_ENTRIES			(40)
#else
#define QUEUE_NUM_OF_PACK_LOG_ENTRIES			(1)
#endif
/*! \def QUEUE_NUM_OF_LOG_ENTRIES
*  \brief Size of queue for log entries.
*  */
#ifdef DO_PACK_AND_BYTE_LOGGING
#define QUEUE_NUM_OF_BYTE_LOG_ENTRIES			(2048)
#else
#define QUEUE_NUM_OF_BYTE_LOG_ENTRIES			(1)
#endif
/*! \def MAX_LOG_FILE_SIZE_BYTES
*  \brief Maximum size of one log file before it is renamed and a new file is started.
*  */
#define MAX_LOG_FILE_SIZE_BYTES			(10000000)

/*! \def MAX_NUMBER_OF_OLD_LOG_FILES_KEPT
*  \brief Maximum number of log files that will be saved before oldest one is lost because it is overwritten by new log file
*  */
#define MAX_NUMBER_OF_OLD_LOG_FILES_KEPT			10

/*! \def MAX_DELAY_LOGGER_MS
*  \brief Maximum delay that this task waits for successful queue operations
*  */
#define MAX_DELAY_LOGGER_MS 						0




#endif
