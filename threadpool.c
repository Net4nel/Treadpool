//Name: Netanel Amiel
//ID: 303136972

#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"


threadpool* create_threadpool(int num_threads_in_pool)
{
	if(num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
	{
		fprintf(stderr, "num_threads_in_pool error\n");
		return NULL;
	}
	
	//Creating new threadpool
	threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
	if(pool == NULL)
	{
		perror("malloc \n");
		return NULL;
	}

	//Starts initiliaze:
	pool->num_threads = num_threads_in_pool;
	pool->qsize = 0;
	pool->shutdown = 0;
	pool->dont_accept = 0;

	pool->qhead = NULL;		
	pool->qtail = NULL;

	if(pthread_mutex_init(&pool->qlock,NULL) != 0)
	{
		fprintf(stderr, "pthread_mutex_init error");
		return NULL;
	}

	if(pthread_cond_init(&pool->q_empty,NULL) != 0)
	{
		fprintf(stderr, "pthread_cond_init error");
		return NULL;
	}
	if(pthread_cond_init(&pool->q_not_empty,NULL) != 0)
	{
		fprintf(stderr, "pthread_cond_init error");
		return NULL;
	}
	
	pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*(num_threads_in_pool));///////////+1
	if(pool->threads == NULL)
	{
		perror("malloc \n");
		return NULL;
	}

	//Threads creation:
	int i;
	for(i=0; i < num_threads_in_pool; i++)
	{
		if(pthread_create(pool->threads+i,NULL, &do_work, pool) < 0) //creating all the threads we need.
		{
			fprintf(stderr, "thread error \n");
			return NULL;
		}	
	}

	return pool;
}



void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	//Verify input:
	if(from_me == NULL)
	{
		fprintf(stderr, "in dispatch: input error \n");
		return;
	}

	if(dispatch_to_here == NULL)
	{
		fprintf(stderr, "in dispatch: input error \n");
		return;
	}

	if(arg == NULL)
	{
		fprintf(stderr, "in dispatch: input error \n");
		return;
	}


	//Starts dispatch-> lock mutex.
	if(pthread_mutex_lock(&from_me->qlock) != 0)
	{
		fprintf(stderr, "pthread_mutex_lock error \n");
		return;
	}

	//Unlock mutex if dont_accept is TRUE
	if(from_me->dont_accept == 1)
	{
		pthread_mutex_unlock(&from_me->qlock);
		return;
	}

	//Create work:
	work_t* work = (work_t*)malloc(sizeof(work_t));
	if(work == NULL)
	{
		perror("malloc ");
		return;
	}

	//Define work
	work->routine = dispatch_to_here;
	work->arg = arg;
	work->next = NULL;

	//"Update" work list
	if(from_me->qsize == 0)
	{
		from_me->qhead = work;
	}
	else
	{
		from_me->qtail->next = work;
	}
	from_me->qtail = work;
	from_me->qsize++;

	if(pthread_cond_signal(&from_me->q_empty) != 0)
	{
		fprintf(stderr, "pthread_cond_signal error \n");
		return;
	}

	if(pthread_mutex_unlock(&from_me->qlock) != 0)
	{
		fprintf(stderr, "pthread_mutex_unlock error \n");
		return;
	}
}


void* do_work(void* p)
{
	if(p == NULL)
	{
		fprintf(stderr," in do_work: input error ");
		return NULL;
	}
	//Create Threadpool within endless loop
	threadpool* pool = (threadpool*)p;
	while(1)
	{
		if(pthread_mutex_lock(&pool->qlock) != 0)
		{
			fprintf(stderr, "pthread_mutex_lock error \n");
			return NULL;
		}	

		if(pool->shutdown == 1) //launch
		{
			pthread_mutex_unlock(&pool->qlock);
			return NULL;
		}

		while(pool->qsize == 0 && pool->shutdown != 1) //wait until queue is empty
		{
			if(pthread_cond_wait(&pool->q_empty, &pool->qlock) != 0)
			return NULL;
		}

		if(pool->shutdown == 1) //shutdown has began
		{
			pthread_mutex_unlock(&pool->qlock);
			return NULL;
		}

		pool->qsize--; //sub the queue size by 1.
		work_t* work = pool->qhead;
		pool->qhead = pool->qhead->next;

		if(pool->qsize == 0)
		{
			pool->qtail = pool->qhead;
			pthread_cond_signal(&pool->q_not_empty);
		}
		
		if(pthread_mutex_unlock(&pool->qlock) != 0)
		{
		fprintf(stderr, "pthread_mutex_unlock error \n");
		return NULL;
		}

		work->routine(work->arg);
		free(work);

	}
}


void destroy_threadpool(threadpool* destroyme)
{
	if(destroyme == NULL)
	{
		fprintf(stderr, "in destroy_threadpool: input error \n");
		return;
	}

	//init threadpool for destruction
	pthread_mutex_lock(&destroyme->qlock);

	destroyme->dont_accept = 1;

	if(destroyme->qsize != 0)
	{
		pthread_cond_wait(&destroyme->q_not_empty, &destroyme->qlock);
	}

	destroyme->shutdown = 1;

	pthread_cond_broadcast(&destroyme->q_empty);
	pthread_mutex_unlock(&destroyme->qlock);

	void* status;

	int i;
	for(i=0;i<destroyme->num_threads;i++)
	{
		pthread_join(destroyme->threads[i], NULL);
	}

	pthread_mutex_destroy(&destroyme->qlock);
	pthread_cond_destroy(&destroyme->q_empty);
	pthread_cond_destroy(&destroyme->q_not_empty);

	free(destroyme->threads);
	free(destroyme);
}
