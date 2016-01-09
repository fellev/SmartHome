/*
 * events.h
 *
 *  Created on: Jan 9, 2016
 *      Author: felix
 */

#ifndef EVENTS_H_
#define EVENTS_H_
#include <pthread.h>

typedef struct {
	pthread_mutex_t		mutex;
	pthread_cond_t		condition;
	bool				isTriggered;
} __attribute__ ((aligned(4))) condition_and_mutex_ts;


/**
 * \brief Creates a new event.
 *
 * \param[out]	cm	Returns the newly created event.
 *
 * \return Linux error code.
 */
int event_create(condition_and_mutex_ts** cm);

/**
 * \brief Causes the calling thread to wait on an event until the event is signaled.
 *
 * \param[in]	eventHandle		Handle to the event.
 * \param[in]	timeout			The number of milliseconds to wait before the
 * thread stops listening. -1 indicates that the wait time is infinite. If a
 * timeout does occur, the value ETIMEDOUT will be returned.
 *
 * \return Linux error code.
 */
int event_waitFor(condition_and_mutex_ts* cm, int timeout);

/**
 * \brief Puts the provided event into a signaled state.
 *
 * \param[in]	cm		event.
 *
 * \return Linux error code.
 */
int event_signal(condition_and_mutex_ts* cm);

#endif /* EVENTS_H_ */
