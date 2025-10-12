#ifndef __LIBRAPIDWRIGHT_H
#define __LIBRAPIDWRIGHT_H

#include <graal_isolate_dynamic.h>


#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*readPhysNetlist__fn_t)(graal_isolatethread_t*, char*);

typedef void (*routeMain__fn_t)(graal_isolatethread_t*);

typedef void (*initialize__fn_t)(graal_isolatethread_t*);

typedef char (*routeOneIteration__fn_t)(graal_isolatethread_t*, int);

typedef void (*getRouteGraphAndConnections__fn_t)(graal_isolatethread_t*, int*, int*, int**, int**, int*, int**, int*, int*, int*);

typedef void (*writeRoutes__fn_t)(graal_isolatethread_t*, int*, int*);

typedef void (*resetBoundingBox__fn_t)(graal_isolatethread_t*, int*, int*);

typedef void (*routeFinish__fn_t)(graal_isolatethread_t*);

typedef void (*writePhysNetlist__fn_t)(graal_isolatethread_t*, char*);

#if defined(__cplusplus)
}
#endif
#endif
