#ifndef _REDMULE_FP32_H_
#define _REDMULE_FP32_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"

#define MATRIX_DIMENSION 	16
#define MATRIX_SIZE     	(MATRIX_DIMENSION * MATRIX_DIMENSION * sizeof(float))
#define LOG_LENGTH 			8

void test_redmule_fp32(){
	flex_global_barrier_xy();
	if (flex_is_first_core() && flex_get_cluster_id() == 0){
		float * x = (float *)local(0);
		float * w = (float *)local(0 + MATRIX_SIZE);
		float * y = (float *)local(0 + MATRIX_SIZE * 2);

		//initialize matrices
		for (int i = 0; i < (MATRIX_DIMENSION * MATRIX_DIMENSION); ++i)
		{
			x[i] = 0.5f;
			w[i] = 0.5f;
			y[i] = 0.0f;
		}

		//print the matrices
		printf("[Initialize X]\n");
		for (int i = 0; i < LOG_LENGTH; ++i)
		{
			printf("%f\n", x[i]);
		}

		printf("[Initialize W]\n");
		for (int i = 0; i < LOG_LENGTH; ++i)
		{
			printf("%f\n", w[i]);
		}

		printf("[Initialize Y]\n");
		for (int i = 0; i < LOG_LENGTH; ++i)
		{
			printf("%f\n", y[i]);
		}

		//RedMule FP32
		flex_redmule_config(MATRIX_DIMENSION, MATRIX_DIMENSION, MATRIX_DIMENSION);
    	flex_redmule_trigger((uint32_t)x, (uint32_t)w, (uint32_t)y, REDMULE_FP_32);
    	flex_redmule_wait();

		printf("[After RedMule Y]\n");
		for (int i = 0; i < LOG_LENGTH; ++i)
		{
			printf("%f\n", y[i]);
		}
	}
	flex_global_barrier_xy();
}

#endif