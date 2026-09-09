#pragma once

#include <pebble.h>
#include "countdown_timer.h"

// Timeline pins for timers, sent to the phone via AppMessage.
void phone_send_pin(CountdownTimer *countdown_timer);
void phone_delete_pin(CountdownTimer *countdown_timer);
void phone_connect(void);
void phone_disconnect(void);
