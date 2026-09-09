/*
 * countdown_timer.c
 *
 * Timer creation, destruction, modification, list handling and persistence.
 * The CountdownTimer structure is not exposed to prevent direct modification.
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/09/15
 */

#include "countdown_timer.h"
#include "common.h"

#define COUNTDOWN_TIMER_EXPIRED ((int64_t)-9223372036854775807LL - 1)

struct CountdownTimer {
  int64_t     start_ms;     //< epoch of start time in milliseconds
  int64_t     duration_ms;  //< total duration in milliseconds
  int32_t     id;           //< random unique integer for timeline pins
  char        buff[16];     //< buffer for printing time string into
  bool        paused;       //< current state
  time_t      last_update;
} __attribute__((__packed__));


// gets the current epoch time in ms
int64_t countdown_timer_get_epoch_ms(void) {
  time_t sec;
  uint16_t msec;
  time_ms(&sec, &msec);
  return (int64_t)sec * MSEC_IN_SEC + (int64_t)msec;
}

// creates a CountdownTimer and sets its values to defaults
CountdownTimer *countdown_timer_create(int64_t duration, int32_t *current_id_max) {
  CountdownTimer *countdown_timer = malloc(sizeof(CountdownTimer));
  if (!countdown_timer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create CountdownTimer");
    return NULL;
  }
  *countdown_timer = (CountdownTimer) {
    .duration_ms = duration,
    .paused = true,
  };
  countdown_timer_rand_id(countdown_timer, current_id_max);
  return countdown_timer;
}

// destroys a CountdownTimer freeing its memory
void countdown_timer_destroy(CountdownTimer *countdown_timer) {
  free(countdown_timer);
}

// starts a CountdownTimer
// the "start" time is kept up to date by adding the current epoch when
// starting, and subtracting it when stopping
void countdown_timer_start(CountdownTimer *countdown_timer) {
  if (!countdown_timer->paused || countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
    return;
  }
  countdown_timer->start_ms += countdown_timer_get_epoch_ms();
  countdown_timer->paused = false;
  countdown_timer->last_update = time(NULL);
}

// stops a CountdownTimer (see countdown_timer_start) and gives it a new ID for pins
void countdown_timer_stop(CountdownTimer *countdown_timer, int32_t *current_id_max) {
  if (countdown_timer->paused) {
    return;
  }
  countdown_timer->start_ms -= countdown_timer_get_epoch_ms();
  countdown_timer->paused = true;
  countdown_timer->last_update = time(NULL);
  countdown_timer_rand_id(countdown_timer, current_id_max);
}

// update the current time of a CountdownTimer
// also modifies the start time as necessary to reflect the change
// and if the duration is less than the update, changes the duration
void countdown_timer_update(CountdownTimer *countdown_timer, int64_t duration,
    bool update_duration) {
  if (countdown_timer->duration_ms < duration || update_duration) {
    countdown_timer->duration_ms = duration;
  }
  countdown_timer->start_ms = (countdown_timer->paused ? 0 : countdown_timer_get_epoch_ms())
    + duration - countdown_timer->duration_ms;
  countdown_timer->last_update = time(NULL);
}

// finds the first expired running CountdownTimer, resetting all expired ones
CountdownTimer *countdown_timer_check_ended(CountdownTimer **timer_array,
                                            uint8_t timer_array_count) {
  CountdownTimer *return_timer = NULL;
  int64_t now = countdown_timer_get_epoch_ms();
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    CountdownTimer *timer = timer_array[ii];
    if (!timer->paused && timer->start_ms + timer->duration_ms <= now) {
      timer->start_ms = COUNTDOWN_TIMER_EXPIRED;
      timer->paused = true;
      if (!return_timer) {
        return_timer = timer;
      }
    }
  }
  return return_timer;
}

// adds a new CountdownTimer to the front of an array, popping the oldest if full
void countdown_timer_list_add(CountdownTimer **timer_array, uint8_t timer_array_max,
                              uint8_t *timer_array_count, CountdownTimer *countdown_timer) {
  if (*timer_array_count == timer_array_max) {
    countdown_timer_destroy(timer_array[timer_array_max - 1]);
  } else {
    (*timer_array_count)++;
  }
  memmove(&timer_array[1], &timer_array[0], sizeof(CountdownTimer*) * (timer_array_max - 1));
  timer_array[0] = countdown_timer;
}

// removes a CountdownTimer pointer from an array (does not destroy the timer)
void countdown_timer_list_remove(CountdownTimer **timer_array, uint8_t *timer_array_count,
                                 uint8_t timer_index) {
  if (timer_array == NULL || *timer_array_count == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to remove CountdownTimer from NULL or empty array");
    return;
  }
  if (timer_index < *timer_array_count) {
    memmove(&timer_array[timer_index], &timer_array[timer_index + 1],
      sizeof(CountdownTimer*) * (*timer_array_count - timer_index - 1));
    (*timer_array_count)--;
  }
}

// gets the index of a CountdownTimer pointer in an array, or -1 if not found
int16_t countdown_timer_list_get_timer_index(CountdownTimer **timer_array,
    uint8_t timer_array_count, CountdownTimer *countdown_timer) {
  if (timer_array == NULL || timer_array_count == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to find CountdownTimer in NULL or empty array");
    return -1;
  }
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (timer_array[ii] == countdown_timer) {
      return ii;
    }
  }
  return -1;
}

// gets the running CountdownTimer with the least time left
CountdownTimer *countdown_timer_list_get_closest_timer(CountdownTimer **timer_array,
                                                       uint8_t timer_array_count) {
  CountdownTimer *countdown_timer = NULL;
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (timer_array[ii]->paused) {
      continue;
    }
    if (countdown_timer == NULL || countdown_timer_get_current_time(timer_array[ii]) <
          countdown_timer_get_current_time(countdown_timer)) {
      countdown_timer = timer_array[ii];
    }
  }
  return countdown_timer;
}

// gets the CountdownTimer which was updated most recently
CountdownTimer *countdown_timer_list_get_last_updated_timer(CountdownTimer **timer_array,
                                                            uint8_t timer_array_count) {
  CountdownTimer *countdown_timer = NULL;
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (countdown_timer == NULL || timer_array[ii]->last_update > countdown_timer->last_update) {
      countdown_timer = timer_array[ii];
    }
  }
  return countdown_timer;
}

// gets a CountdownTimer by its ID or returns NULL if none were found
CountdownTimer *countdown_timer_list_get_timer_by_id(CountdownTimer **timer_array,
                                                     uint8_t timer_array_count, int32_t id) {
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (timer_array[ii]->id == id) {
      return timer_array[ii];
    }
  }
  return NULL;
}

// destroys all CountdownTimers in an array
void countdown_timer_list_destroy_all(CountdownTimer **timer_array, uint8_t *timer_array_count) {
  for (uint8_t ii = 0; ii < *timer_array_count; ii++) {
    countdown_timer_destroy(timer_array[ii]);
  }
  *timer_array_count = 0;
}

// saves all the timers to persistent storage
void countdown_timer_list_save(CountdownTimer **timer_array, uint8_t timer_array_count,
                               uint32_t key) {
  persist_write_int(key++, timer_array_count);
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    persist_write_data(key++, timer_array[ii], sizeof(CountdownTimer));
  }
}

// loads all timers from persistent storage (also allocates memory for them)
void countdown_timer_list_load(CountdownTimer **timer_array, uint8_t timer_array_max,
                               uint8_t *timer_array_count, uint32_t key) {
  *timer_array_count = 0;
  int32_t stored_count = persist_read_int(key++);
  // Reject a corrupt or foreign-format count (e.g. data left behind by a
  // different/older build that reuses this UUID). Loading more than the array
  // can hold would overflow timer_array.
  if (stored_count < 0 || stored_count > timer_array_max) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: bad count %d", (int)stored_count);
    return;
  }
  for (int32_t ii = 0; ii < stored_count; ii++, key++) {
    // Reject blobs that weren't written by this exact CountdownTimer layout.
    // A size mismatch means the persisted data came from a different struct
    // (an older app or the Rebble-store build) and would otherwise be read as
    // garbage, producing the nonsense "IP-like" timer values.
    if (!persist_exists(key) || persist_get_size(key) != (int)sizeof(CountdownTimer)) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: incompatible blob size");
      countdown_timer_list_destroy_all(timer_array, timer_array_count);
      return;
    }
    CountdownTimer *timer = malloc(sizeof(CountdownTimer));
    if (!timer) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory while loading timers!");
      return;
    }
    persist_read_data(key, timer, sizeof(CountdownTimer));
    // Reject blobs whose contents are nonsensical even though the size matched
    // (e.g. same-sized foreign data). A zero/negative duration would later
    // divide by zero when drawing the menu progress bar, and the expired
    // sentinel must only ever appear on a paused timer.
    bool valid_duration = timer->duration_ms > 0;
    bool valid_expired = timer->start_ms != COUNTDOWN_TIMER_EXPIRED || timer->paused;
    if (!valid_duration || !valid_expired) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: invalid timer contents");
      free(timer);
      countdown_timer_list_destroy_all(timer_array, timer_array_count);
      return;
    }
    timer_array[(*timer_array_count)++] = timer;
  }
}

bool countdown_timer_get_paused(CountdownTimer *countdown_timer) {
  return countdown_timer->paused;
}

// gets the current remaining time of the CountdownTimer in milliseconds
int64_t countdown_timer_get_current_time(CountdownTimer *countdown_timer) {
  if (countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
    return 0;
  }
  int64_t current_time = countdown_timer->duration_ms;
  if (countdown_timer->paused) {
    current_time += countdown_timer->start_ms;
  } else {
    current_time -= countdown_timer_get_epoch_ms() - countdown_timer->start_ms;
  }
  if (current_time < 0) {
    return 0;
  }
  if (current_time > countdown_timer->duration_ms) {
    return countdown_timer->duration_ms;
  }
  return current_time;
}

// gets the time to display in milliseconds: an expired timer displays its total
// duration rather than zero, matching how it looked before it was started
int64_t countdown_timer_get_display_time(CountdownTimer *countdown_timer) {
  if (countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
    return countdown_timer->duration_ms;
  }
  return countdown_timer_get_current_time(countdown_timer);
}

// gives the CountdownTimer a new id: the current epoch in seconds
// this guarantees the same ID is never given unless called twice in the same
// second, which cannot happen as the user cannot create two timers that quickly
void countdown_timer_rand_id(CountdownTimer *countdown_timer, int32_t *current_id_max) {
  countdown_timer->id = time(NULL);
}

int32_t countdown_timer_get_id(CountdownTimer *countdown_timer) {
  return countdown_timer->id;
}

int64_t countdown_timer_get_duration(CountdownTimer *countdown_timer) {
  return countdown_timer->duration_ms;
}

time_t countdown_timer_get_last_update(CountdownTimer *countdown_timer) {
  return countdown_timer->last_update;
}

// formats a value as text and prints it onto the provided memory
void countdown_timer_format_text(int64_t value, char *buff, uint8_t size) {
  uint8_t hr = value / MSEC_IN_HR;
  uint8_t min = value % MSEC_IN_HR / MSEC_IN_MIN;
  uint8_t sec = value % MSEC_IN_MIN / MSEC_IN_SEC;
  if (hr > 0) {
    snprintf(buff, size, "%d:%02d:%02d", hr, min, sec);
  } else {
    snprintf(buff, size, "%d:%02d", min, sec);
  }
}

// formats the timer's display time into its own buffer and returns that buffer
char *countdown_timer_format_own_buff(CountdownTimer *countdown_timer) {
  countdown_timer_format_text(countdown_timer_get_display_time(countdown_timer),
    countdown_timer->buff, sizeof(countdown_timer->buff));
  return countdown_timer->buff;
}
