/*
 * countdown_timer.h
 *
 * Timer creation, destruction, modification, list handling and persistence.
 * The CountdownTimer structure is not exposed to prevent direct modification.
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/09/15
 */

#pragma once

#include <pebble.h>

typedef struct CountdownTimer CountdownTimer;

// gets the current epoch time in milliseconds
int64_t countdown_timer_get_epoch_ms(void);

// creates a new CountdownTimer with the given duration in milliseconds
CountdownTimer *countdown_timer_create(int64_t duration, int32_t *current_id_max);

// destroys an existing CountdownTimer
void countdown_timer_destroy(CountdownTimer *countdown_timer);

// starts or resumes a CountdownTimer
void countdown_timer_start(CountdownTimer *countdown_timer);

// stops or pauses a CountdownTimer
void countdown_timer_stop(CountdownTimer *countdown_timer, int32_t *current_id_max);

// sets the time the timer has left; update_duration also sets the total duration
void countdown_timer_update(CountdownTimer *countdown_timer, int64_t duration,
                            bool update_duration);

// resets all finished timers and returns the first one found overdue (or NULL)
CountdownTimer *countdown_timer_check_ended(CountdownTimer **timer_array,
                                            uint8_t timer_array_count);

// adds a CountdownTimer to the front of an array, popping the oldest if full
void countdown_timer_list_add(CountdownTimer **timer_array, uint8_t timer_array_max,
                              uint8_t *timer_array_count, CountdownTimer *countdown_timer);

// removes a CountdownTimer pointer from an array (does not destroy the timer)
void countdown_timer_list_remove(CountdownTimer **timer_array, uint8_t *timer_array_count,
                                 uint8_t timer_index);

// gets the index of a CountdownTimer pointer in an array, or -1 if not found
int16_t countdown_timer_list_get_timer_index(CountdownTimer **timer_array,
    uint8_t timer_array_count, CountdownTimer *countdown_timer);

// gets the running timer with the least time left (or NULL)
CountdownTimer *countdown_timer_list_get_closest_timer(CountdownTimer **timer_array,
                                                       uint8_t timer_array_count);

// gets the timer which was updated most recently (or NULL)
CountdownTimer *countdown_timer_list_get_last_updated_timer(CountdownTimer **timer_array,
                                                            uint8_t timer_array_count);

// gets a CountdownTimer by its ID, or NULL if none matches
CountdownTimer *countdown_timer_list_get_timer_by_id(CountdownTimer **timer_array,
                                                     uint8_t timer_array_count, int32_t id);

// destroys all CountdownTimers in an array
void countdown_timer_list_destroy_all(CountdownTimer **timer_array, uint8_t *timer_array_count);

// saves a list of timers to persistent storage; key is incremented per item
void countdown_timer_list_save(CountdownTimer **timer_array, uint8_t timer_array_count,
                               uint32_t key);

// loads a list of timers from persistent storage (allocates the timers)
void countdown_timer_list_load(CountdownTimer **timer_array, uint8_t timer_array_max,
                               uint8_t *timer_array_count, uint32_t key);

// whether the timer is paused
bool countdown_timer_get_paused(CountdownTimer *countdown_timer);

// remaining time in milliseconds
int64_t countdown_timer_get_current_time(CountdownTimer *countdown_timer);

// time to display in milliseconds; an expired timer shows its total duration
int64_t countdown_timer_get_display_time(CountdownTimer *countdown_timer);

// gives the CountdownTimer a new ID
void countdown_timer_rand_id(CountdownTimer *countdown_timer, int32_t *current_id_max);

int32_t countdown_timer_get_id(CountdownTimer *countdown_timer);

// total duration in milliseconds
int64_t countdown_timer_get_duration(CountdownTimer *countdown_timer);

time_t countdown_timer_get_last_update(CountdownTimer *countdown_timer);

// formats a time in milliseconds as h:mm:ss (or m:ss) into buff
void countdown_timer_format_text(int64_t value, char *buff, uint8_t size);

// formats the timer's display time into its own buffer and returns that buffer
char *countdown_timer_format_own_buff(CountdownTimer *countdown_timer);
