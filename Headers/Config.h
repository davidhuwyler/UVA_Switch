#ifndef HEADERS_CONFIG_H
#define HEADERS_CONFIG_H

#include <stdbool.h>
#include "SpiHandler.h"

typedef enum ePayloadNumbering
{
	PAYLOAD_NUMBER_IGNORED = 0x01,
	PAYLOAD_REORDERING = 0x02,
	ONLY_SEND_OUT_NEW_PAYLOAD = 0x03
} tPayloadNumbering;

typedef enum eDebugOutput
{
	DEBUG_OUTPUT_NONE = 0x01,
	DEBUG_OUTPUT_SHELL_COMMANDS_ONLY = 0x02,
	DEBUG_OUTPUT_FULLLY_ENABLED = 0x03
} tDebugOutput;

typedef enum eLoadBalancing
{
	LOAD_BALANCING_AS_CONFIGURED = 0x01,
	LOAD_BALANCING_SWITCH_WL_CONN_WHEN_ACK_NOT_RECEIVED = 0x02,
	LOAD_BALANCING_USE_ALGORITHM = 0x03
} tLoadBalancing;

typedef struct Configurations {
	/* BaudRateConfiguration */
   int BaudRatesWirelessConn[NUMBER_OF_UARTS]; //
   int BaudRatesDeviceConn[NUMBER_OF_UARTS]; //
   /* ConnectionConfiguration */
   int PrioWirelessConnDev[NUMBER_OF_UARTS][NUMBER_OF_UARTS]; /* [uartNr][prioPerUart] */
   int SendCntWirelessConnDev[NUMBER_OF_UARTS][NUMBER_OF_UARTS]; /* [uartNr][numberOfSendAttempts] */
   /* TransmissionConfiguration */
   int ResendDelayWirelessConn[NUMBER_OF_UARTS]; /* [delayPerWirelessConn] */
   int MaxThroughputWirelessConn[NUMBER_OF_UARTS]; // ToDo: unused!!
   int UsualPacketSizeDeviceConn[NUMBER_OF_UARTS];
   int PackageGenMaxTimeout[NUMBER_OF_UARTS];
   int DelayDismissOldPackagePerDev[NUMBER_OF_UARTS];
   bool SendAckPerWirelessConn[NUMBER_OF_UARTS];
   bool UseCtsPerWirelessConn[NUMBER_OF_UARTS];
   tPayloadNumbering PayloadNumberingProcessingMode[NUMBER_OF_UARTS]; /* per device side, NOT per wireless side */
   int PayloadReorderingTimeout[NUMBER_OF_UARTS];
   tLoadBalancing LoadBalancingMode;
   bool SyncMessagingModeEnabledPerWlConn[NUMBER_OF_UARTS];
   bool UseGolayPerWlConn[NUMBER_OF_UARTS];
   /* SoftwareConfiguration */
   bool TestHwLoopbackOnly;
   bool EnableStressTest;
   tDebugOutput GenerateDebugOutput;
   bool LoggingEnabled;
   int SdCardSyncInterval_s; // [s]
   int SpiHandlerTaskInterval; // [ms]
   int PackageHandlerTaskInterval; // [ms]
   int NetworkHandlerTaskInterval; // [ms]
   int NetworkMetricsTaskInterval; // [ms]
   int TransportHandlerTaskInterval; // [ms]
   int ToggleGreenLedInterval; // [ms]
   int ThroughputPrintoutTaskInterval_s; // [sec]
   int ShellTaskInterval; // [ms]
   int LoggerTaskInterval; // [ms]
} Configuration;

extern Configuration config;


/*!
* \fn uint16_t numberOfBytesInRxByteQueue(tSpiSlaves spiSlave, tUartNr uartNr)
* \brief Reads config file and stores it in global variable
*/
bool readConfig(void);

/*!
* \fn bool readTestConfig(void)
* \brief Reads "TestConfiguration.ini" file -> to check if above functions work correctly
* \return true if test config was read successfully
*/
bool readTestConfig(void);

#endif
