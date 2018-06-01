/*
 * timer.c
 *
 *  Created on: Nov 27, 2017
 *      Author: tony
 */


#include "applicfg.h"
#include "user_timer.h"



/*  ---------  The timer table --------- */
s_timer_entry timers[MAX_NB_TIMER] = {{TIMER_FREE, NULL, NULL, 0, 0, 0},};

TIMEVAL total_sleep_time = TIMEVAL_MAX;
TIMER_HANDLE last_timer_raw = -1;

#define min_val(a,b) ((a<b)?a:b)


/*!
** -------  Use this to declare a new alarm ------
**
** @param d
** @param id
** @param callback
** @param value
** @param period
**
** @return
**/
TIMER_HANDLE SetAlarm(CO_Data* d, TimerEventType id, TimerCallback_t callback, TIMEVAL value, TIMEVAL period)
{
	TIMER_HANDLE row_number;
	s_timer_entry *row;

	/* in order to decide new timer setting we have to run over all timer rows */
	for(row_number=0, row=timers; row_number <= last_timer_raw + 1 && row_number < MAX_NB_TIMER; row_number++, row++)
	{
		if (callback && 	/* if something to store */
		   row->state == TIMER_FREE) /* and empty row */
		{	/* just store */
			TIMEVAL real_timer_value;
			TIMEVAL elapsed_time;

			if (row_number == last_timer_raw + 1) last_timer_raw++;

			elapsed_time = getElapsedTime();
			/* set next wakeup alarm if new entry is sooner than others, or if it is alone */
			real_timer_value = value;
			real_timer_value = min_val(real_timer_value, TIMEVAL_MAX);

			if (total_sleep_time > elapsed_time && total_sleep_time - elapsed_time > real_timer_value)
			{
				printf("real_timer_value is: %lld\n", real_timer_value);
				total_sleep_time = elapsed_time + real_timer_value;
				setTimer(real_timer_value);
			}

			row->callback = callback;
			row->d = d;
			row->id = id;
			row->val = value + elapsed_time;
			row->interval = period;
			row->state = TIMER_ARMED;
			printf("last_timer_raw is %d\n", last_timer_raw);

			return row_number;
		}
	}

	return TIMER_NONE;
}

void free_all_alarm()
{
	TIMER_HANDLE row_number;
	s_timer_entry *row;

	/* in order to decide new timer setting we have to run over all timer rows */
	for(row_number=0, row=timers; row_number <= last_timer_raw && row_number < MAX_NB_TIMER; row_number++, row++)
	{
		row->callback = NULL;
		row->d = NULL;
		row->id = 0;
		row->val = 0;
		row->interval = 0;
		row->state = TIMER_FREE;
	}

	last_timer_raw = -1;
}

void free_spec_type_alarm(TimerEventType id)
{
	TIMER_HANDLE row_number;
	s_timer_entry *row;

	/* in order to decide new timer setting we have to run over all timer rows */
	for(row_number=0, row=timers; row_number <= last_timer_raw && row_number < MAX_NB_TIMER; row_number++, row++)
	{
		if(row->id == id)
		{
			row->callback = NULL;
			row->d = NULL;
			row->id = 0;
			row->val = 0;
			row->interval = 0;
			row->state = TIMER_FREE;

			if (row_number == last_timer_raw) last_timer_raw--;
		}
	}
}

/*!
**  -----  Use this to remove an alarm ----
**
** @param handle
**
** @return
**/
TIMER_HANDLE DelAlarm(TIMER_HANDLE handle)
{
	/* Quick and dirty. system timer will continue to be trigged, but no action will be preformed. */

	if(handle != TIMER_NONE)
	{
		if(handle == last_timer_raw)
			last_timer_raw--;
		timers[handle].state = TIMER_FREE;
	}

	return TIMER_NONE;
}

/*!
** ------  TimeDispatch is called on each timer expiration ----
**
**/
void TimeDispatch(void)
{
	TIMER_HANDLE i;
	TIMEVAL next_wakeup = TIMEVAL_MAX; /* used to compute when should normaly occur next wakeup */
	/* First run : change timer state depending on time */
	/* Get time since timer signal */
	UNS32 overrun = (UNS32)getElapsedTime();

	TIMEVAL real_total_sleep_time = total_sleep_time + overrun;

	s_timer_entry *row;

	for(i=0, row = timers; i <= last_timer_raw; i++, row++)
	{
		if (row->state & TIMER_ARMED) /* if row is active */
		{
			if (row->val <= real_total_sleep_time) /* to be trigged */
			{
				if (!row->interval) /* if simply outdated */
				{
					row->state = TIMER_TRIG; /* ask for trig */
				}
				else /* or period have expired */
				{
					/* set val as interval, with 32 bit overrun correction, */
					/* modulo for 64 bit not available on all platforms     */
					row->val = row->interval - (overrun % (UNS32)row->interval);
					row->state = TIMER_TRIG_PERIOD; /* ask for trig, periodic */
					/* Check if this new timer value is the soonest */
					if(row->val < next_wakeup)
						next_wakeup = row->val;
				}
			}
			else
			{
				/* Each armed timer value in decremented. */
				row->val -= real_total_sleep_time;

				/* Check if this new timer value is the soonest */
				if(row->val < next_wakeup)
					next_wakeup = row->val;
			}
		}
	}

	/* Remember how much time we should sleep. */
	total_sleep_time = next_wakeup;

	/* Set timer to soonest occurence */
	setTimer(next_wakeup);

	/* Then trig them or not. */
	for(i=0, row = timers; i<=last_timer_raw; i++, row++)
	{
		if (row->state & TIMER_TRIG)
		{
			row->state &= ~TIMER_TRIG; /* reset trig state (will be free if not periodic) */
			if(row->callback)
				(*row->callback)(row->d, row->id); /* trig ! */

			/*added by wangliang*/
			if(row->interval == 0)
			{
				DelAlarm(i);
			}
		}
	}
}