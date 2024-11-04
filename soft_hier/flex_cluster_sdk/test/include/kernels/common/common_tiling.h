#ifndef _COMMON_TILING_H_
#define _COMMON_TILING_H_
#include <math.h>
#include "flex_cluster_arch.h"

uint32_t get_base_dimension(uint32_t M, uint32_t N, uint32_t K){
 	uint32_t m_tile = (M+ARCH_NUM_CLUSTER_Y-1)/ARCH_NUM_CLUSTER_Y;
 	uint32_t k_tile = (K+ARCH_NUM_CLUSTER_X-1)/ARCH_NUM_CLUSTER_X;
 	return (m_tile < k_tile)? m_tile : k_tile;
}

uint32_t max_cluster_utilization_tiling(uint32_t M, uint32_t N, uint32_t K, uint32_t elem_size, uint32_t tcdm_factor){
	uint32_t tcdm_eigen_size = (uint32_t)sqrt(ARCH_CLUSTER_TCDM_SIZE/(tcdm_factor*elem_size));
	uint32_t redmule_eigen_size = (uint32_t)sqrt(ARCH_REDMULE_CE_HEIGHT*ARCH_REDMULE_CE_WIDTH*(ARCH_REDMULE_CE_PIPE+1));
	uint32_t base_dimension = get_base_dimension(M,N,K);
	return (base_dimension < tcdm_eigen_size)? base_dimension : tcdm_eigen_size;
}

uint32_t max_tcdm_utilization_tiling(uint32_t M, uint32_t N, uint32_t K, uint32_t elem_size, uint32_t tcdm_factor){
	uint32_t tcdm_eigen_size = (uint32_t)sqrt(ARCH_CLUSTER_TCDM_SIZE/(tcdm_factor*elem_size));
	uint32_t redmule_eigen_size = (uint32_t)sqrt(ARCH_REDMULE_CE_HEIGHT*ARCH_REDMULE_CE_WIDTH*(ARCH_REDMULE_CE_PIPE+1));
	uint32_t base_dimension = get_base_dimension(M,N,K);
	return tcdm_eigen_size;
}

uint32_t max_redmule_utilization_tiling(uint32_t M, uint32_t N, uint32_t K, uint32_t elem_size, uint32_t tcdm_factor){
	uint32_t tcdm_eigen_size = (uint32_t)sqrt(ARCH_CLUSTER_TCDM_SIZE/(tcdm_factor*elem_size));
	uint32_t redmule_eigen_size = (uint32_t)sqrt(ARCH_REDMULE_CE_HEIGHT*ARCH_REDMULE_CE_WIDTH*(ARCH_REDMULE_CE_PIPE+1));
	uint32_t base_dimension = get_base_dimension(M,N,K);
	return (base_dimension < redmule_eigen_size)? redmule_eigen_size : (base_dimension > tcdm_eigen_size)? tcdm_eigen_size : base_dimension;
}

#endif