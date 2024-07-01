#ifndef _EOC_H_
#define _EOC_H_

void eoc(uint32_t val){
	uint32_t * eoc_reg = 0x90000000;
	*eoc_reg = val;
}

#endif