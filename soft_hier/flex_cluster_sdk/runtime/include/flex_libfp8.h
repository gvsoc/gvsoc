// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Bowen Wang, ETH Zurich

// SoftHier Generic FP8 runtime support

#ifndef _FLEX_LIBFP8_H_
#define _FLEX_LIBFP8_H_


union FloatBits {
    float f;
    uint32_t u;
};

void set_lower_byte(float *f_out, uint8_t val) {
    union FloatBits fb = { .f = 0.0f };  // or any other initial float value
    fb.u = (fb.u & 0xFFFFFF00) | val;   // clear lower byte, insert val
    *f_out = fb.f;
    // printf("val: %x, f_out: %x, fb.u: %x\n", val, *f_out, fb.u);
}

// Decode fp8 E5M2 into float32
float fp8_e5m2_to_f32(uint8_t val) {
    if (val == 0x00) return 0.0f;

    uint8_t sign = (val >> 7) & 0x1;
    uint8_t exp  = (val >> 2) & 0x1F;  // 5-bit exponent
    uint8_t frac = val & 0x03;         // 2-bit mantissa

    // Bias = 15
    int exp_val = exp - 15;

    // mantissa = 1 + frac / 4.0
    float mant = 1.0f + (frac * 0.25f);

    // Compute 2^exp_val for small integers without math.h
    float scale = 1.0f;
    if (exp_val >= 0) {
        for (int i = 0; i < exp_val; i++) scale *= 2.0f;
    } else {
        for (int i = 0; i < -exp_val; i++) scale *= 0.5f;
    }

    float result = mant * scale;
    return sign ? -result : result;
}

void spatz_verify(uint32_t avl, uint8_t* act_vec, uint8_t* exp_vec, const float tol) {
    uint8_t exp_fp8, act_fp8;
    float exp_f32, act_f32;
    uint32_t tot_err = 0;

    for (uint32_t i = 0; i < avl; i++) {
        exp_fp8 = exp_vec[i];
        act_fp8 = act_vec[i];
        exp_f32 = fp8_e5m2_to_f32(exp_fp8);
        act_f32 = fp8_e5m2_to_f32(act_fp8);

        float err = exp_f32 - act_f32;
        // if (exp_fp8 != act_fp8) {
        //     printf("%3d | exp: 0x%02x (%.5f), act: 0x%02x (%.5f), err: %.5f\n",
        //            i, exp_fp8, exp_f32, act_fp8, act_f32, err);
        //     tot_err += 1;
        // }
        if (err > tol || err < -tol) {
            printf("%3d | exp: 0x%02x (%.5f), act: 0x%02x (%.5f), err: %.5f\n",
                   i, exp_fp8, exp_f32, act_fp8, act_f32, err);
            tot_err += 1;
        }
    }

    printf("Errors above threshold (E5M2: %3ff): %d out of %d\n", tol, tot_err, avl);
}

#endif