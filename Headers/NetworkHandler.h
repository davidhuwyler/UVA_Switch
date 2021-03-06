#ifndef HEADERS_NETWORKHANDLER_H_
#define HEADERS_NETWORKHANDLER_H_

#include "SpiHandler.h"
#include "PackageHandler.h"
#include "TransportHandler.h"
#include "FRTOS.h"

/*! \def MAX_DELAY_NETW_HANDLER_MS
*  \brief Maximal delay on queue operations inside networkHandler task.
*/
#define MAX_DELAY_NETW_HANDLER_MS				(0)


/*!
* \fn void networkHandler_TaskEntry(void)
* \brief Task generates packages from received bytes (received on device side) and sends those down to
* the packageHandler for transmission.
* When acknowledges are configured, resending is handled here.
*/
void networkHandler_TaskEntry(void* p);

/*!
* \fn void networkHandler_TaskInit(void)
* \brief Initializes all queues that are declared within network handler
* and generates random session number
*/
void networkHandler_TaskInit(void);


#endif
