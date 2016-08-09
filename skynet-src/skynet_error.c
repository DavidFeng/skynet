#include "skynet.h"
#include "skynet_handle.h"
#include "skynet_mq.h"
#include "skynet_server.h"
#include "skynet_timer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <time.h>

#define LOG_MESSAGE_SIZE 256

// assume buffsize is always larger than need
static int prepend_timestamp(struct skynet_context * context, char buff[], size_t buffsize) {
	static uint32_t start_time = 0;
	if (start_time == 0) {
		start_time = skynet_starttime();
	}
	static uint64_t last_now = (uint64_t) -1;
  static int r = 0;
	uint64_t now = skynet_now();
  if (last_now == now) {
    return r;
  } else {
    last_now = now;
  }
	uint64_t tsp = now / 100 + start_time;
	time_t t = (time_t) tsp;
	struct tm tmr;
	if (localtime_r(&t, &tmr) != NULL) {
		int reslen = (int)strftime(buff, buffsize, "[%F %T", &tmr);
		int n = snprintf(buff + reslen, buffsize - reslen, "-%02ld] ", now % 100);
    r = reslen + n;
		return r;
	} else {
		return 0;
	}
}

void
skynet_error(struct skynet_context * context, const char *msg, ...) {
	static uint32_t logger = 0;
	if (logger == 0) {
		logger = skynet_handle_findname("logger");
	}
	if (logger == 0) {
		return;
	}

	static char tmp[LOG_MESSAGE_SIZE];
	char *data;

	int offset = prepend_timestamp(context, tmp, LOG_MESSAGE_SIZE);

	va_list ap;

	va_start(ap,msg);
	int len = vsnprintf(tmp + offset, LOG_MESSAGE_SIZE - offset, msg, ap);
	va_end(ap);
	if (len >=0 && len < LOG_MESSAGE_SIZE - offset) {
		data = skynet_strdup(tmp);
	} else {
		int max_size = LOG_MESSAGE_SIZE;
		for (;;) {
			max_size *= 2;
			data = skynet_malloc(max_size);
			va_start(ap,msg);
			len = vsnprintf(data + offset, max_size - offset, msg, ap);
			va_end(ap);
			if (len < max_size) {
				if (len < 0) {
					skynet_free(data);
					perror("vsnprintf error :");
					return;
				} else {
					strncpy(data, tmp, offset);
				}
				break;
			}
			skynet_free(data);
		}
	}

	struct skynet_message smsg;
	if (context == NULL) {
		smsg.source = 0;
	} else {
		smsg.source = skynet_context_handle(context);
	}
	smsg.session = 0;
	smsg.data = data;
	smsg.sz = (len + offset) | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
	skynet_context_push(logger, &smsg);
}

