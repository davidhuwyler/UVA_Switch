/*
 * PackageBuffer.h
 * Functions to handle the PackageBuffer Structures.
 *
 *  Created on: Sep 15, 2018
 *      Author: dave
 */

#ifndef HEADERS_PACKAGEBUFFER_H_
#define HEADERS_PACKAGEBUFFER_H_

#include <stdint.h>
#include "PackageHandler.h"


/*! \def PACKAGE_BUFFER_SIZE
*  \brief BufferSize in packages
*/
#define PACKAGE_BUFFER_SIZE					400

typedef struct sPackageBuffer
{
	tWirelessPackage packageArray[PACKAGE_BUFFER_SIZE];     		// data buffer
	bool indexIsEmpty[PACKAGE_BUFFER_SIZE];							// indicates if a entry in the buffer is free or occupied
    uint64_t sysTickTimestampBufferInsertion[PACKAGE_BUFFER_SIZE];	// Timestampt in sysTicks at the moment the Package gets inserted into the buffer
    uint16_t variable[PACKAGE_BUFFER_SIZE];							// Free Usable Variable per Package in the buffer (ex. Used for counting send attempts)
    size_t count;     												// number of packages in the buffer
    size_t freeSpace;  												// number of free space for packages in the buffer
    uint16_t payloadNrLastInOrder; 									// The payloadNR of the last sent Package which were correct ordered or sent anyway
} tPackageBuffer;


/*!
* \fn void packageBuffer_init(tPackageBuffer* buffer)
* \brief Initializes the buffer fields
*/
void packageBuffer_init(tPackageBuffer* buffer);

/*!
* \fn bool packageBuffer_free(tPackageBuffer* buffer)
* \brief Frees all memorie in the buffer. Also frees Payloads of packages
* \return true if successful
*/
bool packageBuffer_free(tPackageBuffer* buffer);

/*!
* \fn bool packageBuffer_freeOlderThanCurrentPackage(tPackageBuffer* buffer)
* \brief Frees all packets in the buffer with a lower or equal payloadNr as the current payloadNr 
* \return true if successful
*/
bool packageBuffer_freeOlderThanCurrentPackage(tPackageBuffer* buffer);

/*!
* \fn bool packageBuffer_put(tWirelessPackage* packet);
* \brief Copies the packet into the buffer. Payload dosnt get Copied!
* \return true if successful
*/
bool packageBuffer_put(tPackageBuffer* buffer, tWirelessPackage* packet);

/*!
* \fn bool packageBuffer_putIfNotOld(tPackageBuffer* buffer, tWirelessPackage* packet)
* \brief Copies the packet into the buffer. Payload dosnt get Copied!
*  Only gets into buffer if package if payload is not jet in the buffer and is new (payLoadNr)
* \return true if successful
*/
bool packageBuffer_putIfNotOld(tPackageBuffer* buffer, tWirelessPackage* packet);

/*!
* \fn bool packageBuffer_putWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t variable);
* \brief Copies the packet into the buffer. Payload dosnt get Copied!
* \return true if successful
*/
bool packageBuffer_putWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t variable);

/*!
* \fn bool packageBuffer_getNextOrderedPackage(tWirelessPackage* packet);
* \brief returns a copy of the next buffered packet in order. Needs To be freed after sending!
* 		  frees the package from the buffer
* \return true if there is an ordered package left in buffer
*/
bool packageBuffer_getNextOrderedPackage(tPackageBuffer* buffer, tWirelessPackage* packet);

/*!
* \fn bool packageBuffer_getNextPackageOlderThanTimeout(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t timeOutTicks);
* \brief returns a copy of the next buffered packet which was longer in the buffer than timeOutTicks
* 		  frees the package from the buffer
* \return true if there is an older package in buffer than timeOutTicks
*/
bool packageBuffer_getNextPackageOlderThanTimeout(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t timeOutTicks);

/*!
* \fn bool packageBuffer_getNextPackageOlderThanTimeoutWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t* variable,uint16_t timeOutTicks);
* \brief returns a copy of the next buffered packet which was longer in the buffer than timeOutTicks
* 		  frees the package from the buffer
* \return true if there is an older package in buffer than timeOutTicks
*/
bool packageBuffer_getNextPackageOlderThanTimeoutWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t* variable,uint16_t timeOutTicks);

/*!
* \fn bool packageBuffer_getOldestPackage(tWirelessPackage* packet);
* \brief  returns a copy of the oldes buffered packet (by payloadNR). Needs To be freed after sending!
* 		  frees the package from the buffer
* \return true if successful
*/
bool packageBuffer_getOldestPackage(tPackageBuffer* buffer, tWirelessPackage* packet);


/*!
* \fn bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr)
* \brief  returns a copy of the requested packet. Needs To be freed after sending!
* 		  frees the package from the buffer
* \return true if successful
*/
bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr);


/*!
* \fn bool packageBuffer_getArrayOfPackagePayloadNrInBuffer(tPackageBuffer* buffer, uint16_t* payloadNrArray[PACKAGE_BUFFER_SIZE]);
* \param sizeOfPayloadNrArray variable to store the size of the array
* \param payloadNrArray Array to store the PayloadNrArray
* \return true if successful
*/
bool packageBuffer_getArrayOfPackagePayloadNrInBuffer(tPackageBuffer* buffer,size_t* sizeOfPayloadNrArray ,uint16_t payloadNrArray[PACKAGE_BUFFER_SIZE]);

/*!
* \fn packageBuffer_setCurrentPayloadNR(tPackageBuffer* buffer,uint16_t payloadNr);
* \brief sets the payloadNr Counter to the specified payloadNr
*/
void packageBuffer_setCurrentPayloadNR(tPackageBuffer* buffer,uint16_t payloadNr);

/*!
* \fn uint16_t packageBuffer_getCurrentPayloadNR(tPackageBuffer* buffer);
* \brief returns the last payloadNr which was received
*/
uint16_t packageBuffer_getCurrentPayloadNR(tPackageBuffer* buffer);

#endif /* HEADERS_PACKAGEBUFFER_H_ */
