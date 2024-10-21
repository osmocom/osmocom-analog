#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include "../libsample/sample.h"
#include "../libmobile/sender.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"

void call_down_audio(void __attribute__((unused)) *decoder, void __attribute__((unused)) *decoder_priv, int __attribute__((unused)) callref, uint16_t __attribute__((unused)) sequence, uint8_t __attribute__((unused)) marker, uint32_t __attribute__((unused)) timestamp, uint32_t __attribute__((unused)) ssrc, uint8_t __attribute__((unused)) *payload, int __attribute__((unused)) payload_len) { }
int call_down_setup(int __attribute__((unused)) callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char __attribute__((unused)) *dialing) { return 0; }
void call_down_release(int __attribute__((unused)) callref, int __attribute__((unused)) cause) { }
void call_down_disconnect(int __attribute__((unused)) callref, int __attribute__((unused)) cause) { }
void call_down_answer(int __attribute__((unused)) callref, struct timeval __attribute__((unused)) *tv_meter) { }
void print_help(const char *) { }
void sender_send(sender_t __attribute__((unused)) *sender, sample_t __attribute__((unused)) *samples, uint8_t __attribute__((unused)) *power, int __attribute__((unused)) count) { }
void sender_receive(sender_t __attribute__((unused)) *sender, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) count, double __attribute__((unused)) rf_level_db) { }
void dump_info(void);
void dump_info() {}
