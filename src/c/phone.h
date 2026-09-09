/*
 * NOTE: This is an older version of some code to send pins to the Timeline.
 * I am currently writing a Timeline pin library, and so I am not updating
 * this, as it will be replaced.
 */


#pragma once

#include <pebble.h>
#include "countdown_timer.h"

// public functions
void phone_send_pin(CountdownTimer *countdown_timer);
void phone_delete_pin(CountdownTimer *countdown_timer);
void phone_connect(void);
void phone_disconnect(void);
