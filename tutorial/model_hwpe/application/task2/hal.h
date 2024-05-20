/*
 * Copyright (C) 2018-2019 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors:  Francesco Conti <fconti@iis.ee.ethz.ch>
 *           Renzo Andri <andrire@iis.ee.ethz.ch>
 *           Arpan Suravi Prasad <prasadar@iis.ee.ethz.ch>
 */
#include <stdio.h>

#ifndef __HAL_HWPE_H__
#define __HAL_HWPE_H__

/* REGISTER MAP */

// global address map + event IDs
#define HWPE_ADDR_BASE      0x00302000
#define HWPE_EVT0           13


// commands
#define HWPE_COMMIT_AND_TRIGGER 0x00
#define HWPE_STATUS             0x0c
#define HWPE_SOFT_CLEAR         0x14

// job configuration
#define HWPE_REGISTER_OFFS       0x20
#define HWPE_REG_INPUT_PTR       0x00
#define HWPE_REG_WEIGHT_PTR      0x04
#define HWPE_REG_OUTPUT_PTR      0x08
#define HWPE_REG_WEIGHT_OFFS     0x0c

/* LOW-LEVEL HAL */
// For all the following functions we use __builtin_pulp_OffsetedWrite and __builtin_pulp_OffsetedRead
// instead of classic load/store because otherwise the compiler is not able to correctly factorize
// the HWPE base in case several accesses are done, ending up with twice more code
#if defined(__riscv__) && !defined(RV_ISA_RV32)
  #define HWPE_WRITE_CMD(offset, value)        __builtin_pulp_OffsetedWrite(value, (int volatile *)(HWPE_ADDR_BASE), offset)
  #define HWPE_WRITE_CMD_BE(offset, value, be) *(char volatile *)(HWPE_ADDR_BASE + offset + be) = value
  // #define HWPE_READ_CMD(offset)                (__builtin_pulp_OffsetedRead(*(int volatile *)(HWPE_ADDR_BASE), offset))
  #define HWPE_READ_CMD(ret, offset)           ret = (*(int volatile *)(HWPE_ADDR_BASE + offset))

  #define HWPE_WRITE_REG(offset, value)        __builtin_pulp_OffsetedWrite(value, (int volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS), offset)
  #define HWPE_WRITE_REG_BE(offset, value, be) *(char volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS + offset + be) = value
  // #define HWPE_READ_REG(offset)                (__builtin_pulp_OffsetedRead(*(int volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS), offset))
  #define HWPE_READ_REG(ret, offset)           ret = (*(int volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS + offset))
#else
  #define HWPE_WRITE_CMD(offset, value)        *(int volatile *)(HWPE_ADDR_BASE + offset) = value
  #define HWPE_WRITE_CMD_BE(offset, value, be) *(char volatile *)(HWPE_ADDR_BASE + offset + be) = value
  #define HWPE_READ_CMD(ret, offset)           ret = (*(int volatile *)(HWPE_ADDR_BASE + offset))

  #define HWPE_WRITE_REG(offset, value)        *(int volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS + offset) = value
  #define HWPE_WRITE_REG_BE(offset, value, be) *(char volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS + offset + be) = value
  #define HWPE_READ_REG(ret, offset)           ret = (*(int volatile *)(HWPE_ADDR_BASE + HWPE_REGISTER_OFFS + offset))
#endif

#define HWPE_BARRIER_NOSTATUS()      eu_evt_maskWaitAndClr (1 << HWPE_EVT0)
#define HWPE_BARRIER()               do { eu_evt_maskWaitAndClr (1 << HWPE_EVT0); } while((*(int volatile *)(HWPE_ADDR_BASE + HWPE_STATUS)) != 0)
#define HWPE_BUSYWAIT()              do {                                         } while((*(int volatile *)(HWPE_ADDR_BASE + HWPE_STATUS)) != 0)
#define HWPE_BARRIER_ACQUIRE(job_id) job_id = HWPE_READ_CMD(job_id, HWPE_ACQUIRE); \
                                     while(job_id < 0) { eu_evt_maskWaitAndClr (1 << HWPE_EVT0); HWPE_READ_CMD(job_id, HWPE_ACQUIRE); };

#endif /* __HAL_HWPE_H__ */