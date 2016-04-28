//
// Created by Derrick on 2016-01-12.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>


#include "scheduler.h"
#include "plugin.h"
#include "misc.h"

#define SIG SIGRTMIN
#define CLOCKID CLOCK_REALTIME


//static int (*cbFn) (void *);
//static Schedule_t *(*cbGetSchedule) (void *);
//static sigset_t __mask;

typedef struct ScheduleHandleData_s {
    Schedule_t *schedule;
    void *data;

    int (*cbFn)(void *);
} ScheduleHandleData_t;

static ScheduleHandleData_t *handlerData(Schedule_t *schedule, void *data, int (*cb)(void *)) {

  ScheduleHandleData_t *hd = calloc(1, sizeof(ScheduleHandleData_t));
  if (!hd) {
    SYSLOG(LOG_ERR, "handlerData: Error allocating handler data");
    return NULL;
  }


  hd->schedule = schedule;
  hd->data = data;
  hd->cbFn = cb;
  return hd;
}


static void _handler(int sig, siginfo_t *si, void *uc) {

  ScheduleHandleData_t *hd = (ScheduleHandleData_t *) si->si_value.sival_ptr;
  //pause the scheduler
  Scheduler_pause(hd->schedule, SCHEDULER_PAUSE);

  int status = 0;

  if (hd->cbFn) status = hd->cbFn(hd->data);

  if (status < 0)//callback return -1, remove this scheduled event
    Scheduler_delete(hd->schedule);
  else //unpause scheduler for next event
    Scheduler_unpause(hd->schedule);

}


int Scheduler_setCallback(Schedule_t *schedule, int (*cb)(void *), void *data) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_setCallback: NULL Schedule provided.");
    return 0;
  }

  //set the timer signal event as an information event
  schedule->sa.sa_flags = SA_SIGINFO;
  //set timer callback
  schedule->sa.sa_sigaction = _handler;
  //set callback data
  schedule->sev.sigev_value.sival_ptr = handlerData(schedule, data, cb);

  sigemptyset(&schedule->sa.sa_mask);
  if (sigaction(SIG, &schedule->sa, NULL) == -1) {
    SYSLOG(LOG_ERR, "Scheduler_setCallback: Error establishing handler for signal...");
    return -1;
  }


  return 0;
}

int Scheduler_createTimer(Schedule_t *schedule, int length) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_createTimer: NULL Schedule provided.");
    return 0;
  }

  //block the signal temporarily
  /* sigemptyset(&__mask);
   sigaddset(&__mask, SIG);
   if (sigprocmask(SIG_SETMASK, &__mask, NULL)) {
     SYSLOG(LOG_ERR, "Scheduler_createTimer: Error blocking timer signal...");
     return -1;
   }*/


  //set upt signal event values
  schedule->sev.sigev_notify = SIGEV_SIGNAL;
  schedule->sev.sigev_signo = SIG;

  //create the timer
  if (timer_create(CLOCKID, &schedule->sev, &schedule->timerid) == -1) {
    SYSLOG(LOG_ERR, "Scheduler_createTimer: Error creating timer...");
    return -1;
  }


  //set the timer length now
  schedule->its.it_value.tv_sec = length;
  //want the timer to be a periodic event
  schedule->its.it_interval.tv_sec = length;

  return 0;
}

void Scheduler_setImmediateUpdate(Schedule_t *schedule) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_setImmediateUpdate: NULL Schedule provided.");
    return;
  }

  //update after 1 second
  schedule->its.it_value.tv_sec = 1;
}

//Pause the scheduled event updates
int Scheduler_pause(Schedule_t *schedule, SCHEDULER_STATE state) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_pause: NULL Schedule provided.");
    return 0;
  }

  switch (state) {
    //any value other than zero is true
    default:
    case SCHEDULER_PAUSE: {
      //0'd out clock should disarm timer
      struct itimerspec stop = {};
      memset(&stop, 0, sizeof(struct itimerspec));

      timer_gettime(schedule->timerid, &schedule->its);
      //stop the current clock, store the old value back into its
      //old value should contain the time left before next interval, and the interval itself
      timer_settime(schedule->timerid, 0, &stop, NULL);

    }
      break;

    case SCHEDULER_RESUME:
      timer_settime(schedule->timerid, 0, &schedule->its, NULL);
      break;
  }

  return 0;
}


int Scheduler_start(Schedule_t *schedule) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_start: NULL Schedule provided.");
    return 0;
  }

  if (timer_settime(schedule->timerid, 0, &schedule->its, NULL) == -1) {
    SYSLOG(LOG_ERR, "Scheduler_start: Error starting timer..");
    return -1;
  }

  schedule->running = 1;

  //unblock the signal now
  /*if (sigprocmask(SIG_UNBLOCK, &__mask, NULL) == -1) {
    SYSLOG(LOG_ERR, "Scheduler_start: Error unblocking signals...");
    return -1;
  }*/

  return 0;
}

int Scheduler_delete(Schedule_t *schedule) {

  if (!schedule) {
    SYSLOG(LOG_INFO, "Scheduler_delete: NULL Schedule provided.");
    return 0;
  }

  //delete the timer
  timer_delete(schedule->timerid);

  //free handler data
  if (schedule->sev.sigev_value.sival_ptr)
    free(schedule->sev.sigev_value.sival_ptr);

  //clear the scheduler
  memset(schedule, 0, sizeof(Schedule_t));
  return 0;
}

int Scheduler_isInitialized(Schedule_t *schedule) {

  return (schedule->sev.sigev_value.sival_ptr != NULL);
}

/*
 * Returns boolean value if the scheduler is currently running.
 */
int Scheduler_isTicking(Schedule_t *schedule) {

  return schedule->running;
}
