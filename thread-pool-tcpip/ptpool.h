#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

struct ptpool_attr {
	size_t 	   queue_size;
	size_t 	   pool_size;
	bool   	   queue_block_onfull;
};

typedef struct ptpool_attr ptpool_attr_t;

struct ptpool_wqueue {
	struct ptpool_wqueue *next;
	void (*routine)(void *arg);
	void *arg;
};

typedef struct ptpool_wqueue ptpool_wqueue_t;

struct ptpool {
	pthread_mutex_t	 queue_lock;
	ptpool_wqueue_t *queue_head;
	ptpool_wqueue_t *queue_last;
	pthread_cond_t	 queue_notempty;
	pthread_cond_t	 queue_notfull;
	pthread_cond_t	 queue_empty;
	pthread_t	*threads;
	size_t		 queue_itemsin;
	size_t		 queue_size;
	size_t 		 pool_size;
	bool		 queue_block_onfull;
	bool		 queue_shutdown;
	bool		 queue_closed;
};

typedef struct ptpool ptpool_t;

extern int ptpool_init(ptpool_t *thread_pool, ptpool_attr_t *attr);

extern int ptpool_destroy(ptpool_t *thread_pool, bool gracefully);

extern void *ptpool_worker(void *thread_pool);

extern int ptpool_attr_init(ptpool_attr_t *attr);

extern int ptpool_attr_setpoolsize(ptpool_attr_t *attr, size_t size);

extern int ptpool_attr_setqueuesize(ptpool_attr_t *attr, size_t size);

extern int ptpool_attr_setblockonful(ptpool_attr_t *attr, bool value);

extern int ptpool_wqueue_add(ptpool_t *thread_pool, void (*routine)(void *), void *arg);

extern int ptpool_wqueue_get(ptpool_t *thread_pool, ptpool_wqueue_t **work);
