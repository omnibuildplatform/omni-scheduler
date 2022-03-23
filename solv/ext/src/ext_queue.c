/*
 * ext_queue.c
 *
 * extend libsolv queue
 *
 */

#include "ext_queue.h"
#include "util.h"

/*
 * create queue
 */
Queue *
ext_queue_create()
{
  Queue *q = (Queue *)solv_calloc(1, sizeof(*q));

  queue_init(q);

  return q;
}

/*
 * free queue
 */
void
ext_queue_free(Queue *q)
{
  if (!q)
    return;

  queue_free(q);

  solv_free(q);
}
