#ifndef HEADERS_CONFIG_H
#define HEADERS_CONFIG_H

#include <stdbool.h>
#include "SpiHandler.h"

typedef enum eDebugOutput
{
	DEBUG_OUTPUT_NONE = 0x01,
	DEBUG_OUTPUT_SHELL_COMMANDS_ONLY = 0x02,
	DEBUG_OUTPUT_FULLLY_ENABLED = 0x03
} tDebugOutput;

typedef enum eRoutingMethode
{
	ROUTING_METHODE_HARD_RULES = 0x01,
	ROUTING_METHODE_METRICS = 0x02
} tRoutingMethode;

typedef enum eRoutinMethodeVariant
{
	ROUTING_METHODE_VARIANT_1 = 0x01,
	ROUTING_METHODE_VARIANT_2 = 0x02,
	ROUTING_METHODE_VARIANT_3 = 0x03
}tRoutinMethodeVariant;

typedef enum eTestBenchRoutingAlgorithmModes
{
	TESTBENCH_OFF = 0x00,
	TESTBENCH_MODE_MODEM_SIMULATOR = 0x01,
	TESTBENCH_MODE_MASTER = 0x02
} tTestBenchRoutingAlgorithmModes;

typedef enum eTestBenchModemSimulationScenarios
{
	TESTBENCH_MODEM_SIMULATION_EASY = 0x00,
	TESTBENCH_MODEM_SIMULATION_MEDIUM = 0x01,
	TESTBENCH_MODEM_SIMULATION_HARD = 0x02,
	TESTBENCH_MODEM_SIMULATION_NOF_SCENARIOS = 0x03
} tTestBenchModemSimulationScenarios;

typedef struct Configurations {
	/* BaudRateConfiguration */
   int BaudRatesWirelessConn[NUMBER_OF_UARTS]; //
   int BaudRatesDeviceConn[NUMBER_OF_UARTS]; //

   /* ConnectionConfiguration */
   int PrioDevice[NUMBER_OF_UARTS];
   int fallbackWirelessLink[NUMBER_OF_UARTS];
   int secondFallbackWirelessLink[NUMBER_OF_UARTS];
   /* TransmissionConfiguration */
   int ResendDelayWirelessConn; /* [delayPerWirelessConn] */
   int ResendCountWirelessConn; /* [delayPerWirelessConn] */
   int UsualPacketSizeDeviceConn[NUMBER_OF_UARTS];
   int PackageGenMaxTimeout[NUMBER_OF_UARTS];
   int PayloadReorderingTimeout;
   tRoutingMethode RoutingMethode;
   tRoutinMethodeVariant RoutingMethodeVariant;
   bool UseProbingPacksWlConn[NUMBER_OF_UARTS];
   int CostPerPacketMetric[NUMBER_OF_UARTS];
   bool UseGolayPerWlConn[NUMBER_OF_UARTS];
   /* SoftwareConfiguration */
   bool TestHwLoopbackOnly;
   bool EnableStressTest;
   tTestBenchRoutingAlgorithmModes EnableRoutingAlgorithmTestBench;
   tTestBenchModemSimulationScenarios TestBenchModemSimulationScenario;
   int testBenchMasterUsedChannels[NUMBER_OF_UARTS];
   tDebugOutput GenerateDebugOutput;
   bool LoggingEnabled;
   int SdCardSyncInterval_s; // [s]
   int SpiHandlerTaskInterval; // [ms]
   int PackageHandlerTaskInterval; // [ms]
   int NetworkHandlerTaskInterval; // [ms]
   int NetworkMetricsTaskInterval; // [ms]
   int TransportHandlerTaskInterval; // [ms]
   int TestBenchMasterTaskInterval; // [ms]
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
