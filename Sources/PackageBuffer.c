/*
 * PackageBuffer.c
 *
 *  Created on: Sep 15, 2018
 *      Author: dave
 */
#include "PackageBuffer.h"
#include "FRTOS.h"

/* --------------- prototypes ------------------- */
static bool getIndexOfFreeSpaceInBuffer(tPackageBuffer* buffer, uint16* index);
static bool getIndexOfNextOrderedPackage(tPackageBuffer* buffer, uint16* index);
static bool getIndexOfOldestPackage(tPackageBuffer* buffer, uint16* index);

/*!
* \fn void packageBuffer_init(tPackageBuffer* buffer)
* \brief Initializes the buffer fields
*/
void packageBuffer_init(tPackageBuffer* buffer)
{
	buffer->count = 0;
	buffer->freeSpace = PACKAGE_BUFFER_SIZE;
	buffer->payloadNrLastInOrder = 0;
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		buffer->indexIsEmpty[i] = true;
	}
}

/*!
* \fn bool packageBuffer_free(tPackageBuffer* buffer)
* \brief Frees all memorie in the buffer. Also frees Payloads of packages
* \return true if successful
*/
bool packageBuffer_free(tPackageBuffer* buffer)
{
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		buffer->indexIsEmpty[i] = true;
		vPortFree(buffer->packageArray[i].payload);
		buffer->packageArray[i].payload = NULL;
	}
}

/*!
* \fn bool packageBuffer_put(tWirelessPackage* packet);
* \brief Copies the packet into the buffer. Payload dosnt get Copied!
* \return true if successful
*/
bool packageBuffer_put(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	if(buffer->freeSpace > 0)
	{
		uint16_t indexOfFreePackage;
		if(getIndexOfFreeSpaceInBuffer(buffer, &indexOfFreePackage))
		{
			/* Copy the Package into the buffer */
			uint8_t *bytePtrPackageToCopy = (uint8_t*)packet;
			uint8_t *bytePtrPackageInBuffer = (uint8_t*)&buffer->packageArray[indexOfFreePackage];
			for(int i = 0 ; i < sizeof(tWirelessPackage) ; i++)
			{
				bytePtrPackageInBuffer[i] = bytePtrPackageToCopy[i];
			}
			buffer->indexIsEmpty[indexOfFreePackage] = false;
			buffer->freeSpace --;
			buffer->count ++;
			return true;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getNextOrderedPackage(tWirelessPackage* packet);
* \brief returns a copy of the next buffered packet in order. Needs To be freed after sending!
* \param packet to store the copy from the buffer
* \return true if there is an ordered package left in buffer
*/
bool packageBuffer_getNextOrderedPackage(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	if(buffer->count > 0)
	{
		uint16_t indexOfNextOrderedPackage;
		if(getIndexOfNextOrderedPackage(buffer,&indexOfNextOrderedPackage))
		{
			/* Copy the Package out of the buffer */
			uint8_t *bytePtrPackageToCopy = (uint8_t*)packet;
			uint8_t *bytePtrPackageInBuffer = (uint8_t*)&buffer->packageArray[indexOfNextOrderedPackage];
			for(int i = 0 ; i < sizeof(tWirelessPackage) ; i++)
			{
				bytePtrPackageToCopy[i] = bytePtrPackageInBuffer[i];
			}
			buffer->indexIsEmpty[indexOfNextOrderedPackage] = true;
			buffer->freeSpace ++;
			buffer->count --;
			return true;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getOldestPackage(tWirelessPackage* packet);
* \brief  returns a copy of the oldes buffered packet (payloadNR). Needs To be freed after sending!
* \param packet to store the copy from the buffer
* \return true if successful
*/
bool packageBuffer_getOldestPackage(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	if(buffer->count > 0)
	{
		uint16_t indexOfNextOldestPackage;
		if(getIndexOfOldestPackage(buffer, &indexOfNextOldestPackage))
		{
			/* Copy the Package out of the buffer */
			uint8_t *bytePtrPackageToCopy = (uint8_t*)packet;
			uint8_t *bytePtrPackageInBuffer = (uint8_t*)&buffer->packageArray[indexOfNextOldestPackage];
			for(int i = 0 ; i < sizeof(tWirelessPackage) ; i++)
			{
				bytePtrPackageToCopy[i] = bytePtrPackageInBuffer[i];
			}
			buffer->indexIsEmpty[indexOfNextOldestPackage] = true;
			buffer->freeSpace ++;
			buffer->count --;
			return true;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr)
* \brief  returns a copy of the requested packet. Needs To be freed after sending!
* \return true if successful
*/
bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr)
{
	if(buffer->count > 0)
	{
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if(buffer->packageArray[i].payloadNr == payloadNr)
			{
				/* Copy the Package out of the buffer */
				uint8_t *bytePtrPackageToCopy = (uint8_t*)packet;
				uint8_t *bytePtrPackageInBuffer = (uint8_t*)&buffer->packageArray[i];
				for(int i = 0 ; i < sizeof(tWirelessPackage) ; i++)
				{
					bytePtrPackageToCopy[i] = bytePtrPackageInBuffer[i];
				}
				buffer->indexIsEmpty[i] = true;
				buffer->freeSpace ++;
				buffer->count --;
				return true;
			}
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getArrayOfPackagePayloadNrInBuffer(tPackageBuffer* buffer, uint16_t* payloadNrArray[PACKAGE_BUFFER_SIZE]);
* \param sizeOfPayloadNrArray variable to store the size of the array
* \param payloadNrArray Array to store the PayloadNrArray
* \return true if successful
*/
bool packageBuffer_getArrayOfPackagePayloadNrInBuffer(tPackageBuffer* buffer,size_t* sizeOfPayloadNrArray ,uint16_t payloadNrArray[PACKAGE_BUFFER_SIZE])
{
	if(buffer->count > 0)
	{
		sizeOfPayloadNrArray = 0;
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if(buffer->indexIsEmpty[i] == false)
			{
				payloadNrArray[(uint32_t)sizeOfPayloadNrArray] = buffer->packageArray[i].payloadNr;
				sizeOfPayloadNrArray ++;
			}
		}
		return true;
	}
	return false;
}


/*!
* \fn static bool getIndexOfFreeSpaceInBuffer(tPackageBuffer* buffer, uint16* index)
* \brief  returns a free index in the packageBuffer
* \param index of the free packet in the buffer
* \return true if there is still space in the buffer
*/
static bool getIndexOfFreeSpaceInBuffer(tPackageBuffer* buffer, uint16* index)
{
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		if(buffer->indexIsEmpty[i] == true)
		{
			*index = i;
			return true;
		}
	}
	return false;
}

/*!
* \fn static bool getIndexOfNextOrderedPackage(tPackageBuffer* buffer, uint16* index)
* \brief  returns of the next package in order
* \param index of the next package in order
* \return true if the next package was in buffer
*/
static bool getIndexOfNextOrderedPackage(tPackageBuffer* buffer, uint16* index)
{
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		if(buffer->packageArray[i].payloadNr == (buffer->payloadNrLastInOrder)+1)
		{
			buffer->payloadNrLastInOrder++;
			*index = i;
			return true;
		}
	}
	return false;
}

/*!
* \fn bool getIndexOfOldestPackage(tPackageBuffer* buffer, uint16* index);
* \brief  returns the index of the oldes package in the buffer in terms of payloadNR
* \param index of the oldest package
* \return true if successful
*/
static bool getIndexOfOldestPackage(tPackageBuffer* buffer, uint16* index)
{
	if(buffer->count > 0)
	{
		uint16_t oldestPayloadNR = 0xFFFF;
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if(buffer->packageArray[i].payloadNr < oldestPayloadNR)
			{
				*index = i;
			}
		}
		if(oldestPayloadNR<0xFFFF)
		{
			return true;
		}
	}
	return false;
}
