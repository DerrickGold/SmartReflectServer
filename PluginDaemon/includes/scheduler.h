//
// Created by Derrick on 2016-01-12.
//

#ifndef MAGICMIRROR_SCHEDULER_H
#define MAGICMIRROR_SCHEDULER_H

#include <time.h>
#include <signal.h>

typedef struct Schedule_s {

    timer_t timerid;

    struct itimerspec its;
    struct sigevent sev;
    struct sigaction sa;

    int running;
} Schedule_t;

typedef enum {
    SCHEDULER_PAUSE, SCHEDULER_RESUME
} SCHEDULER_STATE;

extern int Scheduler_setCallback(Schedule_t *schedule, int (*cb)(void *), void *data);

extern int Scheduler_createTimer(Schedule_t *schedule, int length);

extern int Scheduler_pause(Schedule_t *schedule, SCHEDULER_STATE state);

extern int Scheduler_start(Schedule_t *schedule);

extern int Scheduler_delete(Schedule_t *schedule);

extern int Scheduler_isInitialized(Schedule_t *schedule);

extern int Scheduler_isTicking(Schedule_t *schedule);

extern void Scheduler_setImmediateUpdate(Schedule_t *schedule);

#define Scheduler_unpause(s) Scheduler_pause(s, SCHEDULER_RESUME);

#endif //MAGICMIRROR_SCHEDULER_H
