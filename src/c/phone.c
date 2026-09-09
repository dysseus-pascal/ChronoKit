// Timeline pins for timers: sent to the phone (src/pkjs/index.js) via AppMessage.

#include <pebble.h>
#include "phone.h"
#include "common.h"

// AppMessage Keys
#define KEY_DURATION 5
#define KEY_UNIQUEID 10
#define KEY_TOTAL_TIME 15

// send one pin message for the timer; a duration of 0 tells the phone to delete the pin
static void prv_send(CountdownTimer *countdown_timer, int32_t duration_s, int32_t total_time_s) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int32(iter, KEY_UNIQUEID, countdown_timer_get_id(countdown_timer));
  dict_write_int32(iter, KEY_DURATION, duration_s);
  dict_write_int32(iter, KEY_TOTAL_TIME, total_time_s);
  dict_write_end(iter);
  app_message_outbox_send();
}

// send a pin for the timer
void phone_send_pin(CountdownTimer *countdown_timer) {
  prv_send(countdown_timer, countdown_timer_get_current_time(countdown_timer) / MSEC_IN_SEC,
    countdown_timer_get_duration(countdown_timer) / MSEC_IN_SEC);
}

// delete a pin
void phone_delete_pin(CountdownTimer *countdown_timer) {
  prv_send(countdown_timer, 0, 0);
}

// callbacks (logging only)
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason,
                                   void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (APP_MSG_NOT_CONNECTED == (int)reason));
}

// start phone connection
void phone_connect(void) {
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

// stop phone connection
void phone_disconnect(void) {
  app_message_deregister_callbacks();
}
