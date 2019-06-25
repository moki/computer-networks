#include "ptpool.h"

int ptpool_attr_init(ptpool_attr_t *attr) {
	if (!attr)
		return -1;
	attr->queue_block_onfull = true;
	attr->queue_size = 1;
	attr->pool_size = 1;
	return 0;
}

int ptpool_attr_setpoolsize(ptpool_attr_t *attr, size_t size) {
	if (!attr)
		return -1;
	if (!size)
		return -1;
	attr->pool_size = size;
	return 0;
}

int ptpool_attr_setqueuesize(ptpool_attr_t *attr, size_t size) {
	if (!attr)
		return -1;
	if (!size)
		return -1;
	attr->queue_size = size;
	return 0;
}

int ptpool_attr_setblockonful(ptpool_attr_t *attr, bool value) {
	if (!attr)
		return -1;
	if (value != true && value != false)
		return -1;
	attr->queue_block_onfull = value;
	return 0;
}

int ptpool_init(ptpool_t *thread_pool, ptpool_attr_t *attr) {
	if (!thread_pool)
		return -1;
	if (!attr)
		return -1;
	pthread_t *threads;
	size_t i;
	int err;
	thread_pool->queue_block_onfull = attr->queue_block_onfull;
	thread_pool->queue_size = attr->queue_size;
	thread_pool->pool_size = attr->pool_size;
	thread_pool->queue_shutdown = 0;
	thread_pool->queue_itemsin = 0;
	thread_pool->queue_closed = 0;
	thread_pool->queue_head = NULL;
	thread_pool->queue_last = NULL;
	err = pthread_mutex_init(&(thread_pool->queue_lock), NULL);
	if (err)
		return -1;
	err = pthread_cond_init(&(thread_pool->queue_notempty), NULL);
	if (err)
		return -1;
	err = pthread_cond_init(&(thread_pool->queue_notfull), NULL);
	if (err)
		return -1;
	err = pthread_cond_init(&(thread_pool->queue_empty), NULL);
	if (err)
		return -1;
	threads = malloc(sizeof(pthread_t) * thread_pool->pool_size);
        if (!threads)
		return -1;
        for (i = 0; i < thread_pool->pool_size; i++) {
                err = pthread_create(&(threads[i]), NULL, ptpool_worker, thread_pool);
                if (err)
                        return -1;
        }
	thread_pool->threads = threads;

	return 0;
}

void *ptpool_worker(void *thread_pool) {
	if (!thread_pool)
		return NULL;
	ptpool_t *pool = (ptpool_t *) thread_pool;
	ptpool_wqueue_t *work = NULL;
	int err;
	for (;;) {
		pthread_mutex_lock(&(pool->queue_lock));

		while (!(pool->queue_itemsin) && (!pool->queue_shutdown)) {
			err = pthread_cond_wait(&(pool->queue_notempty), &(pool->queue_lock));
			if (err)
				exit(EXIT_FAILURE);
		}


		if (pool->queue_shutdown) {
			err = pthread_mutex_unlock(&(pool->queue_lock));
			if (err)
				exit(EXIT_FAILURE);
			pthread_exit(NULL);
		}

		/* get work off the queue */
		err = ptpool_wqueue_get(thread_pool, &work);
		if (err)
			exit(EXIT_FAILURE);

		pthread_mutex_unlock(&(pool->queue_lock));
		(*(work->routine))(work->arg);
		free(work);
	}
	pthread_exit(NULL);
}

int ptpool_wqueue_get(ptpool_t *thread_pool, ptpool_wqueue_t **work) {
	if (!thread_pool)
		return -1;
	if (!work)
		return -1;
	int err;

	*work = thread_pool->queue_head;
	thread_pool->queue_itemsin = thread_pool->queue_itemsin - 1;
	if (thread_pool->queue_itemsin == 0) {
		thread_pool->queue_last = NULL;
		thread_pool->queue_head = NULL;
	} else {
		thread_pool->queue_head = thread_pool->queue_head->next;
	}
	if (thread_pool->queue_block_onfull &&
	    thread_pool->queue_itemsin == thread_pool->queue_size - 1) {
		err = pthread_cond_broadcast(&(thread_pool->queue_notfull));
		if (err)
			return -1;
	}
	if (thread_pool->queue_itemsin == 0) {
		err = pthread_cond_signal(&(thread_pool->queue_empty));
		if (err)
			return -1;
	}

	return 0;
}

int ptpool_wqueue_add(ptpool_t *thread_pool, void (*routine)(void *), void *arg) {
	if (!thread_pool)
		return -1;
	if (!routine)
		return -1;
	ptpool_wqueue_t *work;
	int err;
	err = pthread_mutex_lock(&(thread_pool->queue_lock));
	if (err)
		return -1;

	/* returns 1 if queue is full, and queue_block_onfull is set to false */
	if (thread_pool->queue_itemsin == thread_pool->queue_size &&
	    !thread_pool->queue_block_onfull) {
		err = pthread_mutex_unlock(&(thread_pool->queue_lock));
		if (err)
			return -1;
		return 1;
	}

	while (thread_pool->queue_itemsin == thread_pool->queue_size &&
	       !(thread_pool->queue_shutdown || thread_pool->queue_closed)) {
		err = pthread_cond_wait(&(thread_pool->queue_notfull), &(thread_pool->queue_lock));
		if (err)
			return -1;
	}

	/* returns 2 if queue is closed or shutdown */
	if (thread_pool->queue_shutdown || thread_pool->queue_closed) {
		err = pthread_mutex_unlock(&(thread_pool->queue_lock));
		if (err)
			return -1;
		return 2;
	}

	work = malloc(sizeof(ptpool_wqueue_t));
	work->routine = routine;
	work->arg = arg;
	work->next = NULL;
	if (thread_pool->queue_itemsin == 0) {
		thread_pool->queue_last = work;
		thread_pool->queue_head = work;
		err = pthread_cond_broadcast(&(thread_pool->queue_notempty));
		if (err)
			return -1;
	} else {
		(thread_pool->queue_last)->next = work;
		thread_pool->queue_last = work;
	}
	thread_pool->queue_itemsin = thread_pool->queue_itemsin + 1;
	err = pthread_mutex_unlock(&(thread_pool->queue_lock));
	if (err)
		return -1;
	return 0;
}

int ptpool_destroy(ptpool_t *thread_pool, bool gracefully) {
	if (!thread_pool)
		return -1;
	size_t i;
	int err;
	ptpool_wqueue_t *cursor;
	err = pthread_mutex_lock(&(thread_pool->queue_lock));
	if (err)
		return -1;
	if (thread_pool->queue_shutdown || thread_pool->queue_closed) {
		err = pthread_mutex_unlock(&(thread_pool->queue_lock));
		if (err)
			return -1;
		return 0;
	}
	thread_pool->queue_closed = true;
	if (gracefully) {
		while (thread_pool->queue_itemsin) {
			err = pthread_cond_wait(&(thread_pool->queue_empty),
						&(thread_pool->queue_lock));
			if (err)
				return -1;
		}
	}
	thread_pool->queue_shutdown = true;
	err = pthread_mutex_unlock(&(thread_pool->queue_lock));
	if (err)
		return -1;
	err = pthread_cond_broadcast(&(thread_pool->queue_notempty));
	if (err)
		return -1;
	err = pthread_cond_broadcast(&(thread_pool->queue_notfull));
	if (err)
		return -1;
	for (i = 0; i < thread_pool->pool_size; i++) {
		err = pthread_join(thread_pool->threads[i], NULL);
		if (err)
			return -1;
	}
	free(thread_pool->threads);
	for (;thread_pool->queue_head;) {
		cursor = thread_pool->queue_head->next;
		free(thread_pool->queue_head);
		thread_pool->queue_head = cursor;
	}

	free(thread_pool);
	return 0;
}
