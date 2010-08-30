#include <stdio.h>
#include <unistd.h>
#include <zmq.h>
#include "bzmq.h"

int main(int argc, char **argv)
{
	printf("main:\n");

	void *ctx = zmq_init(1);

	bzmq_generator_t *gen = bzmq_generator_init(ctx);
	bzmq_pool_t *p = bzmq_pool_init(ctx, NULL, 10);

	bzmq_pool_start(p);

	// generate crap

	bzmq_generator(gen, 10, 0);

	printf("generator round 2\n");

	bzmq_generator(gen, 10, 1);
//	sleep(1);

	bzmq_pool_destroy(p);
	bzmq_generator_destroy(gen);	
	zmq_term(ctx);

	pthread_exit(NULL);
}
