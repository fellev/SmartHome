/*
 * events.c
 *
 *  Created on: Jan 9, 2016
 *      Author: felix
 */


#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "events.h"

int event_create(condition_and_mutex_ts** cm)
{
	int errorCode;

	*cm = (condition_and_mutex_ts*) malloc(sizeof(condition_and_mutex_ts));

	errorCode = pthread_mutex_init(&(*cm)->mutex, NULL);

	if (errorCode != 0)
		return errorCode;

	errorCode = pthread_cond_init(&(*cm)->condition, NULL);

	if (errorCode != 0)
		return errorCode;

	(*cm)->isTriggered = false;

	return 0;
}

int event_waitFor(condition_and_mutex_ts* cm, int timeout)
{
	int errorCode, loopErrorCode;
	struct timespec delta;
	struct timespec abstime;

	// Compute our timeout.
	if (timeout != -1) {

		clock_gettime(CLOCK_REALTIME, &abstime);

		int nano = abstime.tv_nsec + (timeout % 1000) * 1000000;

		delta.tv_nsec = nano % 1000000000;
		delta.tv_sec = abstime.tv_sec + timeout / 1000 + nano / 1000000000;
	}

	errorCode = pthread_mutex_lock(&cm->mutex);

	if (errorCode != 0)
		return errorCode;

	while (!cm->isTriggered) {

		if (timeout == -1) {
			loopErrorCode = pthread_cond_wait(&cm->condition, &cm->mutex);

			if (loopErrorCode != 0)
				return errorCode;
		}
		else {
			loopErrorCode = pthread_cond_timedwait(&cm->condition, &cm->mutex, &delta);

			if (loopErrorCode == ETIMEDOUT) {
				cm->isTriggered = false;
				errorCode = pthread_mutex_unlock(&cm->mutex);

				return loopErrorCode;
			}
		}
	}

	cm->isTriggered = false;

	errorCode = pthread_mutex_unlock(&cm->mutex);

	if (errorCode != 0)
		return errorCode;

	return 0;
}

int event_signal(condition_and_mutex_ts* cm)
{
	int errorCode;

	errorCode = pthread_mutex_lock(&cm->mutex);

	cm->isTriggered = true;

	if (errorCode != 0)
		return errorCode;

	errorCode = pthread_cond_signal(&cm->condition);

	if (errorCode != 0)
		return errorCode;

	errorCode = pthread_mutex_unlock(&cm->mutex);

	if (errorCode != 0)
		return errorCode;

	return 0;
}
