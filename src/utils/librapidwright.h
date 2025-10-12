#ifndef __LIBRAPIDWRIGHT_H
#define __LIBRAPIDWRIGHT_H

#include "graal_isolate.h"


#if defined(__cplusplus)
extern "C" {
#endif

void readPhysNetlist_(graal_isolatethread_t*, char*);

void routeMain_(graal_isolatethread_t*);

void initialize_(graal_isolatethread_t*);

char routeOneIteration_(graal_isolatethread_t*, int);

void getRouteGraphAndConnections_(graal_isolatethread_t*, int*, int*, int**, int**, int*, int**, int*, int*, int*);

void writeRoutes_(graal_isolatethread_t*, int*, int*);

void resetBoundingBox_(graal_isolatethread_t*, int*, int*);

void routeFinish_(graal_isolatethread_t*);

void writePhysNetlist_(graal_isolatethread_t*, char*);

#if defined(__cplusplus)
}
#endif
#endif
