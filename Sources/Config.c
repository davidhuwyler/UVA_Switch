#include "MINI.h"
#include "Config.h"
#include <stdlib.h> //atoi()
#include <stdbool.h>
#include "Platform.h"

#define TEMP_CSV_SIZE 50
#define DEFAULT_CSV_STRING "0, 0, 0, 0"
#define DEFAULT_BOOL 0
#define DEFAULT_INT 1000


/* prototypes */
void csvToInt(char inputString[], int outputArray[]);
void csvToBool(char inputString[], bool outputArray[]);
void setDefaultConfigValues(void);
void validateSwConfiguration(void);

/* global variables */
Configuration config;

/*!
* \fn void csvToInt(char inputString[], int outputArray[])
* \brief reads a string with format "2350, 324087, 134098,3407,23407" and returns it as array.
*  Values need to be comma separated, whitespaces are ignored and can be of any length.
* \param inputString: string where numbers should be read from
* \param outputArray: Array where parsed numbers should be stored
*/
void csvToInt(char inputString[], int outputArray[])
{
	int output;
	int indexLastDigit = 0;
	int indexFirstDigit = 0;
	int count = 0;
	for (int i=0; inputString[i]; i++)
	  count += (inputString[i] == ','); // find number of integers saved as csv
	for(int i=0; i<(count+1); i++)
	{
		while((inputString[indexFirstDigit] < '0') || (inputString[indexFirstDigit] > '9')) // fast foreward to the beginning of number
		{
			indexFirstDigit++;
			indexLastDigit++;
		}
		while((inputString[indexLastDigit] >= '0') && (inputString[indexLastDigit] <= '9')) // find the last digit that is part of one number
			indexLastDigit++;
		inputString[indexLastDigit] = '\0';
		output = atoi(&inputString[indexFirstDigit]);
		outputArray[i] = output;
		indexFirstDigit = indexLastDigit+1;
		indexLastDigit = indexLastDigit+1;
	}
}

/*!
* \fn void csvToBool(char inputString[], bool outputArray[])
* \brief reads a string with format "1, 0, 1, 0,0" and returns it as array.
*  Values need to be comma separated, whitespaces are ignored and can be of any length.
* \param inputString: string where numbers should be read from
* \param outputArray: Array where parsed numbers should be stored
*/
void csvToBool(char inputString[], bool outputArray[])
{
	int indexLastDigit = 0;
	int indexFirstDigit = 0;
	int count = 0;
	for (int i=0; inputString[i]; i++)
	  count += (inputString[i] == ','); // find number of integers saved as csv
	for(int i=0; i<(count+1); i++)
	{
		while((inputString[indexFirstDigit] < '0') || (inputString[indexFirstDigit] > '9')) // fast foreward to the beginning of number
		{
			indexFirstDigit++;
			indexLastDigit++;
		}
		while((inputString[indexLastDigit] >= '0') && (inputString[indexLastDigit] <= '9')) // find the last digit that is part of one number
			indexLastDigit++;
		inputString[indexLastDigit] = '\0';
		outputArray[i] = atoi(&inputString[indexFirstDigit]) > 0 ? true : false;
		indexFirstDigit = indexLastDigit+1;
		indexLastDigit = indexLastDigit+1;
	}
}

/*!
* \fn bool readTestConfig(void)
* \brief Reads "TestConfiguration.ini" file -> to check if above functions work correctly
* \return true if test config was read successfully
*/
bool readTestConfig(void)
{
	char sectionName[] = "TestConfiguration";
  	char intKey[] = "myInt";
  	char boolKey[] = "myBool";
  	char arrKey[] = "myArray";
  	char fooValue[] = "foo";
  	int arrSize = 30;
  	char arrStringValue[arrSize];
  	int arrInt[4];
  	char fileName[] = "TestConfig.ini";
  	long int valInt = MINI_ini_getl(sectionName, intKey, 0, fileName);
  	bool valBool = MINI_ini_getbool(sectionName, boolKey, 0, fileName);
  	int numberOfCharsCopied = MINI_ini_gets(sectionName, arrKey, fooValue, arrStringValue, arrSize, fileName);
  	csvToInt(arrStringValue, arrInt);
  	return true;
}

/*!
* \fn bool readConfig(void)
* \brief Reads config file and stores it in global variable
*/
bool readConfig(void)
{
	int tmpIntArr[NUMBER_OF_UARTS];
	int numberOfCharsCopied;
	char copiedCsv[TEMP_CSV_SIZE];
  	char fileName[] = "serialSwitch_Config.ini";

  	/* -------- BaudRateConfiguration -------- */
  	/* BAUD_RATES_WIRELESS_CONN */
  	numberOfCharsCopied = MINI_ini_gets("BaudRateConfiguration", "BAUD_RATES_WIRELESS_CONN",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.BaudRatesWirelessConn);

  	/* BAUD_RATES_DEVICE_CONN */
    numberOfCharsCopied = MINI_ini_gets("BaudRateConfiguration", "BAUD_RATES_DEVICE_CONN",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.BaudRatesDeviceConn);


  	/* -------- ConnectionConfiguration -------- */
  	/* PRIO_DEVICE */
    numberOfCharsCopied = MINI_ini_gets("ConnectionConfiguration", "PRIO_DEVICE",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.PrioDevice);

  	/* FALLBACK_WIRELESS_LINK */
    numberOfCharsCopied = MINI_ini_gets("ConnectionConfiguration", "FALLBACK_WIRELESS_LINK",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.fallbackWirelessLink);

  	/* -------- TransmissionConfiguration -------- */
  	/* ResendDelayWirelessConn */
  	config.ResendDelayWirelessConn = MINI_ini_getl("TransmissionConfiguration", "RESEND_DELAY_WIRELESS_CONN",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* ResendCountWirelessConn */
  	config.ResendCountWirelessConn = MINI_ini_getl("TransmissionConfiguration", "RESEND_COUNT_WIRELESS_CONN",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* USUAL_PACKET_SIZE_DEVICE_CONN */
  	numberOfCharsCopied = MINI_ini_gets("TransmissionConfiguration", "USUAL_PACKET_SIZE_DEVICE_CONN",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.UsualPacketSizeDeviceConn);

  	/* PACKAGE_GEN_MAX_TIMEOUT */
  	numberOfCharsCopied = MINI_ini_gets("TransmissionConfiguration", "PACKAGE_GEN_MAX_TIMEOUT",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.PackageGenMaxTimeout);

  	/* PACK_REORDERING_TIMEOUT */
  	config.PayloadReorderingTimeout = MINI_ini_getl("TransmissionConfiguration", "PAYLOAD_REORDERING_TIMEOUT",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* ROUTING_METHODE */
  	config.RoutingMethode = MINI_ini_getl("TransmissionConfiguration", "ROUTING_METHODE",  DEFAULT_INT, "serialSwitch_Config.ini");
  	switch(config.RoutingMethode)
  	{
		case ROUTING_METHODE_HARD_RULES:
		case ROUTING_METHODE_METRICS:
			break; /* no action when config parameter set right */
		default:
			config.RoutingMethode = ROUTING_METHODE_HARD_RULES; /* ROUTING_METHODE_HARD_RULES if parameter faulty */
  	}

  	/* USE_PROBING_PACKS  */
  	numberOfCharsCopied = MINI_ini_gets("TransmissionConfiguration", "USE_PROBING_PACKS",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToBool(copiedCsv, config.UseProbingPacksWlConn);

  	/* COST_PER_PACKET_METRIC  */
  	numberOfCharsCopied = MINI_ini_gets("TransmissionConfiguration", "COST_PER_PACKET_METRIC",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToInt(copiedCsv, config.CostPerPacketMetric);

  	/* USE_GOLAY_ERROR_CORRECTING_CODE */
  	numberOfCharsCopied = MINI_ini_gets("TransmissionConfiguration", "USE_GOLAY_ERROR_CORRECTING_CODE",  DEFAULT_CSV_STRING, copiedCsv, TEMP_CSV_SIZE, "serialSwitch_Config.ini");
  	csvToBool(copiedCsv, config.UseGolayPerWlConn);

  	/* -------- SoftwareConfiguration -------- */
  	/* TEST_HW_LOOPBACK_ONLY */
  	config.TestHwLoopbackOnly = MINI_ini_getbool("SoftwareConfiguration", "TEST_HW_LOOPBACK_ONLY",  DEFAULT_BOOL, "serialSwitch_Config.ini");

  	/* TEST_HW_LOOPBACK_ONLY */
  	config.EnableStressTest = MINI_ini_getbool("SoftwareConfiguration", "ENABLE_STRESS_TEST",  DEFAULT_BOOL, "serialSwitch_Config.ini");

  	/* ENABLE_ROUTING_ALGORITHM_TEST_BENCH  */
  	config.EnableRoutingAlgorithmTestBench = MINI_ini_getbool("SoftwareConfiguration", "ENABLE_ROUTING_ALGORITHM_TEST_BENCH",  DEFAULT_BOOL, "serialSwitch_Config.ini");

  	/* GENERATE_DEBUG_OUTPUT */
  	config.GenerateDebugOutput = MINI_ini_getl("SoftwareConfiguration", "GENERATE_DEBUG_OUTPUT",  DEFAULT_BOOL, "serialSwitch_Config.ini");
  	switch(config.GenerateDebugOutput)
  	{
		case DEBUG_OUTPUT_NONE:
		case DEBUG_OUTPUT_SHELL_COMMANDS_ONLY:
		case DEBUG_OUTPUT_FULLLY_ENABLED:
			break; /* no action when config parameter set right */
		default:
			config.GenerateDebugOutput = DEBUG_OUTPUT_FULLLY_ENABLED; /* set debug output to enabled when config parameter faulty */
  	}

  	/* LOGGING_ENABLED */
  	config.LoggingEnabled = MINI_ini_getl("SoftwareConfiguration", "LOGGING_ENABLED",  DEFAULT_BOOL, "serialSwitch_Config.ini");

  	/* SD_CARD_SYNC_INTERVAL */
  	config.SdCardSyncInterval_s = MINI_ini_getl("SoftwareConfiguration", "SD_CARD_SYNC_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* SPI_HANDLER_TASK_INTERVAL */
  	config.SpiHandlerTaskInterval = MINI_ini_getl("SoftwareConfiguration", "SPI_HANDLER_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* PACKAGE_GENERATOR_TASK_INTERVAL */
  	config.PackageHandlerTaskInterval = MINI_ini_getl("SoftwareConfiguration", "PACKAGE_GENERATOR_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* NETWORK_HANDLER_TASK_INTERVAL */
    config.NetworkHandlerTaskInterval = MINI_ini_getl("SoftwareConfiguration", "NETWORK_HANDLER_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* NETWORK_METRICS_TASK_INTERVAL */
    config.NetworkMetricsTaskInterval = MINI_ini_getl("SoftwareConfiguration", "NETWORK_METRICS_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

    /* TRANSPORT_HANDLER_TASK_INTERVAL */
    config.TransportHandlerTaskInterval = MINI_ini_getl("SoftwareConfiguration", "TRANSPORT_HANDLER_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* TOGGLE_GREEN_LED_INTERVAL */
  	config.ToggleGreenLedInterval = MINI_ini_getl("SoftwareConfiguration", "TOGGLE_GREEN_LED_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

  	/* THROUGHPUT_PRINTOUT_TASK_INTERVAL */
	config.ThroughputPrintoutTaskInterval_s = MINI_ini_getl("SoftwareConfiguration", "THROUGHPUT_PRINTOUT_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

	/* SHELL_TASK_INTERVAL */
	config.ShellTaskInterval = MINI_ini_getl("SoftwareConfiguration", "SHELL_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

	/* LOGGER_TASK_INTERVAL */
	config.LoggerTaskInterval = MINI_ini_getl("SoftwareConfiguration", "LOGGER_TASK_INTERVAL",  DEFAULT_INT, "serialSwitch_Config.ini");

	validateSwConfiguration();

  	return true;

}

/*!
* \fn void validateSwConfiguration(void)
* \brief Validates config file and makes sure no impossible SW configurations are made
*/
void validateSwConfiguration(void)
{
	/* constrain task execution intervals */
	UTIL1_constrain(config.SdCardSyncInterval_s, 1, 1000); /* 1sec...1000sec */
	UTIL1_constrain(config.SpiHandlerTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.PackageHandlerTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.NetworkHandlerTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.TransportHandlerTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.ShellTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.LoggerTaskInterval, 1, 1000); /* 1ms...1sec */
	UTIL1_constrain(config.ThroughputPrintoutTaskInterval_s, 1, 1000); /* 1sec...1000sec */
	UTIL1_constrain(config.ToggleGreenLedInterval, 1, 1000); /* 1ms...1sec */
}
