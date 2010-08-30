#ifndef _BZMQ_H_
#define _BZMQ_H_

#include <pthread.h>

void *bzmq_work(void *data);
void bzmq_msg_free (void *data, void *hint);

typedef struct {
	int tid;
    void *ctx;
    void *data;
} bzmq_data_t;

typedef struct {
	pthread_t *thread;
	bzmq_data_t *tdata;
	int n_thread;
	pthread_attr_t attr;
} bzmq_pool_t;

typedef struct {
	void *ctx;
	void *in;
	void *out;
	void *ctrl;
} bzmq_generator_t;

bzmq_pool_t *bzmq_pool_init(void *ctx, void *data, int size);
void bzmq_pool_destroy(bzmq_pool_t *pool);
void bzmq_pool_start(bzmq_pool_t *pool);
void *bzmq_work(void *data);
bzmq_generator_t *bzmq_generator_init(void *ctx);
int bzmq_generator_destroy(bzmq_generator_t *gen);
void bzmq_generator(bzmq_generator_t *gen, int maxn, int close);
int bzmq_send_int_nb(void *sock, int n);

#endif
