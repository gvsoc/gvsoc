/*
 * Copyright (C) 2020-2022  GreenWaves Technologies, ETH Zurich, University of Bologna
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
 * Authors: Arpan Suravi Prasad, ETH Zurich (prasadar@iis.ee.ethz.ch)
 */

#ifndef __HWPE_REG_PARAMS_H
#define __HWPE_REG_PARAMS_H

#define HWPE_REG_INPUT_PTR          0
#define HWPE_REG_WEIGHT_PTR         1
#define HWPE_REG_OUTPUT_PTR         2
#define HWPE_REG_WEIGHT_OFFS        3
       
#define HWPE_NB_REG                 4

#define HWPE_REG_COMMIT_AND_TRIGGER 0x00
#define HWPE_REG_STATUS             0x0c
#define HWPE_REG_SOFT_CLEAR         0x14
#define HWPE_REGISTER_OFFS          0x20

#define CLUSTER_MASK                0x0FFFFFFF


#define INPUT_LINEAR_BUFFER_SIZE   8
#define OUTPUT_LINEAR_BUFFER_SIZE  1
#define BINCONV_PER_COLUMN         8
#define COLUMN_PER_PE              8
#define WEIGHT_OFFSET_LATENCY      2
#define L1_BANDWIDTH               8 // in bytes

#define WEIGHT_OFFSET_LATENCY      2


#endif