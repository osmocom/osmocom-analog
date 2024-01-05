#pragma once

#include <osmocom/core/logging.h>
#include "categories.h"

extern int loglevel;

#define LOGP_CHAN(cat, level, fmt, arg...) LOGP(cat, level, "(chan %s) " fmt, CHAN, ## arg)

void get_win_size(int *w, int *h);
void lock_logging(void);
void unlock_logging(void);
void enable_limit_scroll(bool enable);
void logging_limit_scroll_top(int lines);
void logging_limit_scroll_bottom(int lines);
const char *debug_amplitude(double level);
const char *debug_db(double level_db);
void logging_print_help(void);
int parse_logging_opt(const char *optarg);
void logging_init(void);

