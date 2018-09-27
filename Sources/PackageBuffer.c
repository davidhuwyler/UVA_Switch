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
static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy);
static bool checkIfPackageInBuffer(tPackageBuffer* buffer, uint16 payloadNr);
static bool updateTickCounter(void);


static uint64_t tickCounter = 0;

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
		buffer->sysTickTimestampBufferInsertion[i] = 0;
		buffer->variable[i] = 0;
	}
}

/*!
* \fn bool packageBuffer_free(tPackageBuffer* buffer)
* \brief Frees all memorie in the buffer. Also frees Payloads of packages
* \return true if successful
*/
bool packageBuffer_free(tPackageBuffer* buffer)
{
	buffer->count = 0;
	buffer->freeSpace = PACKAGE_BUFFER_SIZE;
	buffer->payloadNrLastInOrder = 0;
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		if(!buffer->indexIsEmpty[i])
		{
			vPortFree(buffer->packageArray[i].payload);
			buffer->packageArray[i].payload = NULL;
		}

		buffer->indexIsEmpty[i] = true;
		buffer->sysTickTimestampBufferInsertion[i] = 0;
		buffer->variable[i] = 0;
	}
}

/*!
* \fn bool packageBuffer_freeOlderThanCurrentPackage(tPackageBuffer* buffer)
* \brief Frees all packets in the buffer with a lower or equal payloadNr as the current payloadNr 
* \return true if successful
*/
bool packageBuffer_freeOlderThanCurrentPackage(tPackageBuffer* buffer)
{
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		if(!buffer->indexIsEmpty[i] && buffer->packageArray[i].payloadNr <= buffer->payloadNrLastInOrder)
		{
			vPortFree(buffer->packageArray[i].payload);
			buffer->packageArray[i].payload = NULL;
			buffer->count --;
			buffer->freeSpace ++;
			buffer->indexIsEmpty[i] = true;
			buffer->sysTickTimestampBufferInsertion[i] = 0;
			buffer->variable[i] = 0;
		}
	}
}

/*!
* \fn bool packageBuffer_put(tWirelessPackage* packet);
* \brief Copies the packet into the buffer.
* \return true if successful
*/
bool packageBuffer_put(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	updateTickCounter();
	if(buffer->freeSpace > 0)
	{
		uint16_t indexOfFreePackage;
		if((checkIfPackageInBuffer(buffer,packet->payloadNr)))
				return true;
		if(getIndexOfFreeSpaceInBuffer(buffer, &indexOfFreePackage))
		{
			buffer->indexIsEmpty[indexOfFreePackage] = false;
			buffer->sysTickTimestampBufferInsertion[indexOfFreePackage] = tickCounter;
			buffer->variable[indexOfFreePackage] = 0;
			buffer->freeSpace --;
			buffer->count ++;
			bool result = copyPackage(packet,&buffer->packageArray[indexOfFreePackage]);
			return result;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_putIfNotOld(tPackageBuffer* buffer, tWirelessPackage* packet)
* \brief Copies the packet into the buffer.
*  Only gets into buffer if package if payload is not jet in the buffer and is new (payLoadNr)
* \return true if successful
*/
bool packageBuffer_putIfNotOld(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	updateTickCounter();
	if(buffer->freeSpace > 0)
	{
		uint16_t indexOfFreePackage;
		if((checkIfPackageInBuffer(buffer,packet->payloadNr)) || (packet->payloadNr<=buffer->payloadNrLastInOrder))
				return true;
		if(getIndexOfFreeSpaceInBuffer(buffer, &indexOfFreePackage))
		{
			buffer->indexIsEmpty[indexOfFreePackage] = false;
			buffer->sysTickTimestampBufferInsertion[indexOfFreePackage] = tickCounter;
			buffer->variable[indexOfFreePackage] = 0;
			buffer->freeSpace --;
			buffer->count ++;
			bool result = copyPackage(packet,&buffer->packageArray[indexOfFreePackage]);
			return result;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_put(tWirelessPackage* packet);
* \brief Copies the packet into the buffer.
* \return true if successful
*/
bool packageBuffer_putWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t variable)
{
	updateTickCounter();
	if(buffer->freeSpace > 0)
	{
		uint16_t indexOfFreePackage;
		//if((checkIfPackageInBuffer(buffer,packet->payloadNr)) || (packet->payloadNr<=buffer->payloadNrLastInOrder))
		//		return true;
		if(getIndexOfFreeSpaceInBuffer(buffer, &indexOfFreePackage))
		{
			/* Copy the Package into the buffer */
			buffer->indexIsEmpty[indexOfFreePackage] = false;
			buffer->sysTickTimestampBufferInsertion[indexOfFreePackage] = tickCounter;
			buffer->variable[indexOfFreePackage] = variable;
			buffer->freeSpace --;
			buffer->count ++;
			bool result = copyPackage(packet,&buffer->packageArray[indexOfFreePackage]);
			return result;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_put(tWirelessPackage* packet);
* \brief Copies the packet into the buffer. Does not check if package payloadNr already in Buffer
* \return true if successful
*/
bool packageBuffer_putNotUnique(tPackageBuffer* buffer, tWirelessPackage* packet)
{
	updateTickCounter();
	if(buffer->freeSpace > 0)
	{
		uint16_t indexOfFreePackage;
		if(getIndexOfFreeSpaceInBuffer(buffer, &indexOfFreePackage))
		{
			buffer->indexIsEmpty[indexOfFreePackage] = false;
			buffer->sysTickTimestampBufferInsertion[indexOfFreePackage] = tickCounter;
			buffer->variable[indexOfFreePackage] = 0;
			buffer->freeSpace --;
			buffer->count ++;
			bool result = copyPackage(packet,&buffer->packageArray[indexOfFreePackage]);
			return result;
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
	updateTickCounter();
	if(buffer->count > 0)
	{
		uint16_t indexOfNextOrderedPackage;
		if(getIndexOfNextOrderedPackage(buffer,&indexOfNextOrderedPackage))
		{
			/* Copy the Package out of the buffer */
			buffer->indexIsEmpty[indexOfNextOrderedPackage] = true;
			buffer->freeSpace ++;
			buffer->count --;
			bool result = copyPackage(&buffer->packageArray[indexOfNextOrderedPackage],packet);
			vPortFree(buffer->packageArray[indexOfNextOrderedPackage].payload);
			buffer->packageArray[indexOfNextOrderedPackage].payload = NULL;
			return result;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getNextPackageOlderThanTimeout(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t timeOutTicks);
* \brief returns a copy of the next buffered packet which was longer in the buffer than timeOutTicks
* 		  frees the package from the queue
* \return true if there is an older package in buffer than timeOutTicks
*/
bool packageBuffer_getNextPackageOlderThanTimeout(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t timeOutTicks)
{
	updateTickCounter();
	if(buffer->count > 0)
	{
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if((!buffer->indexIsEmpty[i]) && ((tickCounter-(buffer->sysTickTimestampBufferInsertion[i])) > timeOutTicks))
			{
				/* Copy the Package out of the buffer */
				buffer->indexIsEmpty[i] = true;
				buffer->freeSpace ++;
				buffer->count --;
				bool result = copyPackage(&buffer->packageArray[i],packet);
				vPortFree(buffer->packageArray[i].payload);
				buffer->packageArray[i].payload = NULL;
				return result;
			}
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getNextPackageOlderThanTimeout(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t timeOutTicks);
* \brief returns a copy of the next buffered packet which was longer in the buffer than timeOutTicks
* 		  frees the package from the queue
* \return true if there is an older package in buffer than timeOutTicks
*/
bool packageBuffer_getNextPackageOlderThanTimeoutWithVar(tPackageBuffer* buffer, tWirelessPackage* packet,uint16_t* variable,uint16_t timeOutTicks)
{
	updateTickCounter();
	if(buffer->count > 0)
	{
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if((!buffer->indexIsEmpty[i]) && ((tickCounter-(buffer->sysTickTimestampBufferInsertion[i])) > timeOutTicks))
			{
				/* Copy the Package out of the buffer */
				buffer->indexIsEmpty[i] = true;
				*variable = buffer->variable[i];
				buffer->freeSpace ++;
				buffer->count --;
				bool result = copyPackage(&buffer->packageArray[i],packet);
				vPortFree(buffer->packageArray[i].payload);
				buffer->packageArray[i].payload = NULL;
				return result;
			}
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
	updateTickCounter();
	if(buffer->count > 0)
	{
		uint16_t indexOfNextOldestPackage;
		if(getIndexOfOldestPackage(buffer, &indexOfNextOldestPackage))
		{
			/* Copy the Package out of the buffer */
			buffer->indexIsEmpty[indexOfNextOldestPackage] = true;
			buffer->freeSpace ++;
			buffer->count --;
			bool result = copyPackage(&buffer->packageArray[indexOfNextOldestPackage],packet);
			vPortFree(buffer->packageArray[indexOfNextOldestPackage].payload);
			buffer->packageArray[indexOfNextOldestPackage].payload = NULL;
			return result;
		}
	}
	return false;
}

/*!
* \fn bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr)
* \brief  returns a copy of the requested packet. Needs To be freed after sending!
* 		  frees the package from the queue
* 		  If there multiple Packages with the same PayloadNR, all of them are deleted (redundant packages)
* \return true if successful
*/
bool packageBuffer_getPackage(tPackageBuffer* buffer, tWirelessPackage* packet, uint16_t payloadNr)
{
	updateTickCounter();
	if(buffer->count > 0)
	{
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if(!buffer->indexIsEmpty[i] && buffer->packageArray[i].payloadNr == payloadNr)
			{
				/* Copy the Package out of the buffer */
				buffer->indexIsEmpty[i] = true;
				buffer->freeSpace ++;
				buffer->count --;
				bool result = copyPackage(&buffer->packageArray[i],packet);
				vPortFree(buffer->packageArray[i].payload);
				buffer->packageArray[i].payload = NULL;
				return result;
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
	updateTickCounter();
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
* \fn packageBuffer_setCurrentPayloadNR(tPackageBuffer* buffer,uint16_t payloadNr);
* \brief sets the payloadNr Counter to the specified payloadNr
*/
void packageBuffer_setCurrentPayloadNR(tPackageBuffer* buffer,uint16_t payloadNr)
{
	if(buffer->payloadNrLastInOrder < payloadNr)
		buffer->payloadNrLastInOrder = payloadNr;
}

/*!
* \fn uint16_t packageBuffer_getCurrentPayloadNR(tPackageBuffer* buffer);
* \brief returns the last payloadNr which was received
*/
uint16_t packageBuffer_getCurrentPayloadNR(tPackageBuffer* buffer)
{
	return buffer->payloadNrLastInOrder;
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
		if((!buffer->indexIsEmpty[i]) && (buffer->packageArray[i].payloadNr == (buffer->payloadNrLastInOrder)+1))
		{
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
		uint16_t oldestPayloadNR = 0;
		for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
		{
			if((oldestPayloadNR == 0) ||
			   (buffer->packageArray[i].payloadNr < oldestPayloadNR))
			{
				oldestPayloadNR = buffer->packageArray[i].payloadNr;
				*index = i;
			}
		}
		if(oldestPayloadNR>0)
		{
			return true;
		}
	}
	return false;
}

/*!
* \fn static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy)
* \brief Copies the content of the original package into the copy, allocates memory for payload of copy
* \return bool: true if successful, false otherwise
*/
static bool copyPackage(tWirelessPackage* original, tWirelessPackage* copy)
{
	*copy = *original;
	copy->payload = FRTOS_pvPortMalloc(original->payloadSize*sizeof(int8_t));
	if(copy->payload == NULL)
	{
		return false;
	}
	for(int cnt = 0; cnt < copy->payloadSize; cnt++)
	{
		copy->payload[cnt] = original->payload[cnt];
	}
	return true;
}

static bool updateTickCounter(void)
{
	static uint16_t lastOsTick = 0, newOsTick;
	newOsTick = xTaskGetTickCount();

	if(newOsTick >= lastOsTick)
		tickCounter += (newOsTick-lastOsTick);
	else
	{
		tickCounter += (0xFFFF-lastOsTick);
		tickCounter += newOsTick;
	}

	lastOsTick = newOsTick;
}

/*!
* \fn static bool checkIfPackageInBuffer(tPackageBuffer* buffer, uint16* payloadNr)
* \return bool: true if package is already in buffer, false otherwise
*/
static bool checkIfPackageInBuffer(tPackageBuffer* buffer, uint16 payloadNr)
{
	for(int i = 0 ; i < PACKAGE_BUFFER_SIZE ; i ++)
	{
		if(!buffer->indexIsEmpty && buffer->packageArray[i].payloadNr == payloadNr)
		{
			return true;
		}
	}
	return false;
}
