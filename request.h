#ifndef __REQUEST_H__
#include "segel.h"
#include "stat_thread.h"

typedef struct timeval *Time;
typedef struct request_m* Request;

void requestHandle(Request req);
Request createRequest(int fd, Time arrive_time);
void *copyRequest(void *request);
void destroyRequest(void *request);
int getFdRequest(Request request);
Time getArriveTimeRequest(Request request);
StatThread getThreadRequest(Request request);
void setDispatchRequest(Request request, Time new_dispatch_time);
void requestSetThread(Request request, StatThread new_thread);

#endif
