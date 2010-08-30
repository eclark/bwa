#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <zmq.h>
#include "bzmq.h"

#define INENDPOINT "inproc://in"
#define OUTENDPOINT "inproc://out"
#define CTRLENDPOINT "inproc://ctrl"

void bzmq_msg_free (void *data, void *hint)
{
	free(data);
}

bzmq_pool_t *bzmq_pool_init(void *ctx, void *data, int size)
{
	bzmq_pool_t *pool = (bzmq_pool_t *)malloc(sizeof(bzmq_pool_t));
	pool->thread = (pthread_t *)calloc(size, sizeof(pthread_t));
	pool->tdata = (bzmq_data_t *)calloc(size, sizeof(bzmq_data_t));
	pool->n_thread = size;

	int t;
	for (t = 0; t < size; t++) {
		pool->tdata[t].tid = t;
		pool->tdata[t].ctx = ctx;
		pool->tdata[t].data = data;
	}

	pthread_attr_init(&pool->attr);
	pthread_attr_setdetachstate(&pool->attr, PTHREAD_CREATE_DETACHED);

	return pool;
}

void bzmq_pool_destroy(bzmq_pool_t *pool)
{
	pthread_attr_destroy(&pool->attr);
	free(pool->thread);
	free(pool);
}

void bzmq_pool_start(bzmq_pool_t *pool)
{
	int t, rc;
	for (t = 0; t < pool->n_thread; t++) {
		rc = pthread_create(&pool->thread[t], &pool->attr, bzmq_work, (void *)&pool->tdata[t]);
		assert(!rc);
	}
}

void *bzmq_work(void *data)
{
	bzmq_data_t *tdata = (bzmq_data_t *)data;
	void *ctx = tdata->ctx;

	printf("thread[%d]: start\n", tdata->tid);

	int rc;

	void *in = zmq_socket(ctx, ZMQ_PULL);
	void *out = zmq_socket(ctx, ZMQ_PUSH);
	void *ctrl = zmq_socket(ctx, ZMQ_SUB);

	rc = zmq_connect(in, INENDPOINT);
	if (rc) {
		perror("zmq_connect(in)");
		abort();
	}
	rc = zmq_connect(out, OUTENDPOINT);
	assert(!rc);

	rc = zmq_connect(ctrl, CTRLENDPOINT);
	assert(!rc);

	static const int one = 1;

	zmq_pollitem_t items[2] = {
		{ in, 0, ZMQ_POLLIN, 0 },
		{ ctrl, 0, ZMQ_POLLIN, 0 }
	};

	while(zmq_poll(items, 2, -1) > -1) {
		zmq_msg_t msg;

		if (items[0].revents & ZMQ_POLLIN) {
			zmq_msg_init(&msg);
			rc = zmq_recv(in, &msg, 0);
			assert(!rc);

			int i = *(int *)zmq_msg_data(&msg);

			printf("thread[%d]: %d\n", tdata->tid, i);
	
			zmq_msg_close(&msg);

			zmq_msg_init_data(&msg, (void *)&one, sizeof(int), NULL, NULL);
			zmq_send(out, &msg, 0);
			zmq_msg_close(&msg);
		}
		if (items[1].revents & ZMQ_POLLIN)
			break;
	}

	zmq_close(in);
	zmq_close(out);
	zmq_close(ctrl);

	return NULL;
}

bzmq_generator_t *bzmq_generator_init(void *ctx)
{
	int rc;

	bzmq_generator_t *gen = (bzmq_generator_t *)malloc(sizeof(bzmq_generator_t));
	gen->ctx = ctx;

	void *in = zmq_socket(ctx, ZMQ_PUSH);
	void *out = zmq_socket(ctx, ZMQ_PULL);
	void *ctrl = zmq_socket(ctx, ZMQ_PUB);
	rc = zmq_bind(in, INENDPOINT);
	assert(!rc);
	rc = zmq_bind(out, OUTENDPOINT);
	assert(!rc);
	rc = zmq_bind(ctrl, CTRLENDPOINT);
	assert(!rc);

	gen->in = in;
	gen->out = out;
	gen->ctrl = ctrl;

	return gen;
}

int bzmq_generator_destroy(bzmq_generator_t *gen)
{
	int rc;

	if ((rc = zmq_close(gen->in)))
		goto error;

	if ((rc = zmq_close(gen->out)))
		goto error;

	if ((rc = zmq_close(gen->ctrl)))
		goto error;	

	return 0;

	error:
		return rc;
}

/*
	Sends 0 to n_seq to inproc://in while receiving from inproc://out
		ctx = zeromq context,
		maxn = maximium number to send into inproc://in
		close = if true, close after maxn is received.
*/
void bzmq_generator(bzmq_generator_t *gen, int maxn, int close)
{
	int rc;

	void *in = gen->in;
	void *out = gen->out;
	void *ctrl = gen->ctrl;

	zmq_pollitem_t items[2] = {
		{ in, 0, ZMQ_POLLOUT, 0 },
		{ out, 0, ZMQ_POLLIN, 0 }
	};

	int pollin = 0;  // changes to 1 when we're done sending.
	int n = 0;
	int rn = 0;
	while(zmq_poll(&items[pollin], pollin == 0 ? 2 : 1, -1) > -1) {
		if(pollin == 0 && items[0].revents & ZMQ_POLLOUT) {
			rc = bzmq_send_int_nb(in, n);
			if (rc == 0 && n < maxn) {
				n++;
			} else if (n == maxn) {
				pollin = 1;
			}
		}
		if(items[1].revents & ZMQ_POLLIN) {
			zmq_msg_t msg;
			
			rc = zmq_msg_init(&msg);
			assert(!rc);
			rc = zmq_recv(out, &msg, ZMQ_NOBLOCK);
			assert(!rc || rc == EAGAIN);
			if (rc == 0) {
				rn++;
				if (rn == maxn) {
					break;
				}
			}
		}
	}

	static const int one = 1;

	if (close) {
		zmq_msg_t msg;

		zmq_msg_init_data(&msg, (void *)&one, sizeof(int), NULL, NULL);
		zmq_send(ctrl, &msg, 0);
		zmq_msg_close(&msg);
	}

}

int bzmq_send_int_nb(void *sock, int n)
{
	zmq_msg_t msg;
	int rc;

	void *data = malloc(sizeof(int));
	memcpy(data, &n, sizeof(int));
	zmq_msg_init_data(&msg, data, sizeof(int), bzmq_msg_free, NULL);

	rc = zmq_send(sock, &msg, ZMQ_NOBLOCK);
	assert(rc == EAGAIN || rc == 0);

	zmq_msg_close(&msg);

	return rc;
}
