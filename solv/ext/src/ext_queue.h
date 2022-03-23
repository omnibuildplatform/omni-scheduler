/*
 * ext_queue.h
 *
 */

#ifndef OMNI_SCHEDULER_EXT_QUEUE_H
#define OMNI_SCHEDULER_EXT_QUEUE_H

#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Queue * ext_queue_create();

extern void ext_queue_free(Queue *q);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXT_QUEUE_H */

