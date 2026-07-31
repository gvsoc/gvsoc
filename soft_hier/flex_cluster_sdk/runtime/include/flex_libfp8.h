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

/*
  Format transform
*/
typedef uint8_t fp8_e5m2;

// Decode fp8 E5M2 into float32
float fp8_e5m2_to_f32(fp8_e5m2 val) {
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

fp8_e5m2 f32_to_fp8_e5m2(float x) {
    // Zero shortcut (treat -0.0 the same as +0.0)
    if (x == 0.0f) return 0x00;

    // Sign and absolute value
    uint8_t sign = 0;
    if (x < 0.0f) {
        sign = 1;
        x = -x;
    }

    // Minimum normal for E5M2 with bias 15 is 1.0 * 2^-14
    float min_norm = 1.0f;
    for (int i = 0; i < 14; ++i) min_norm *= 0.5f;

    // Underflow: flush tiny values to zero (no subnormals handled)
    if (x < min_norm) return 0x00;

    // Normalize x to mantissa in [1, 2) and get unbiased exponent
    int exp_val = 0;
    float mant = x;

    if (mant >= 2.0f) {
        while (mant >= 2.0f) { mant *= 0.5f; exp_val++; }
    } else if (mant < 1.0f) {
        while (mant < 1.0f) { mant *= 2.0f; exp_val--; }
    }

    // Quantize mantissa: mant = 1 + frac/4  =>  frac in {0,1,2,3}
    float frac_f = (mant - 1.0f) * 4.0f;
    int frac = (int)(frac_f + 0.5f);  // round to nearest

    // Handle rounding overflow of fraction (e.g., 1.999 -> frac==4)
    if (frac > 3) {
        frac = 0;
        exp_val += 1;
    }

    // Bias = 15
    int exp_biased = exp_val + 15;

    // Underflow after rounding -> flush to zero
    if (exp_biased <= 0) return 0x00;

    // Overflow -> saturate to max finite (exp=31, frac=3)
    if (exp_biased > 31) {
        exp_biased = 31;
        frac = 3;
    }

    // Pack bits: [sign][5-bit exp][2-bit frac]
    uint8_t val = (uint8_t)((sign << 7) | ((exp_biased & 0x1F) << 2) | (frac & 0x03));
    return val;
}

// Decode fp16 (IEEE 754, E5M10) into float32
float fp16_to_f32(uint16_t val) {
    if (val == 0x0000) return 0.0f;

    uint16_t sign = (val >> 15) & 0x1;
    uint16_t exp  = (val >> 10) & 0x1F;    // 5-bit exponent
    uint16_t frac = val & 0x3FF;           // 10-bit mantissa

    if (exp == 0x1F) {
        // Handle Inf / NaN
        float inf_val = 65504.0f; // Max FP16 value
        return sign ? -inf_val : inf_val;
    }

    float mant;
    int exp_val;

    if (exp == 0) {
        // Subnormal number
        exp_val = 1 - 15;
        mant = frac / 1024.0f;
    } else {
        // Normalized number
        exp_val = exp - 15;
        mant = 1.0f + (frac / 1024.0f);
    }

    // Compute 2^exp_val without math.h
    float scale = 1.0f;
    if (exp_val >= 0) {
        for (int i = 0; i < exp_val; i++) scale *= 2.0f;
    } else {
        for (int i = 0; i < -exp_val; i++) scale *= 0.5f;
    }

    float result = mant * scale;
    return sign ? -result : result;
}

inline fp8_e5m2 asm_fp8_e5m2_sqrt(fp8_e5m2 a) {
    float fa = fp8_e5m2_to_f32(a);
    float fb;
    asm volatile (
        "fsqrt.s %0, %1, rne\n"
        : "=f"(fb)        // output in FP register
        : "f"(fa)              // input in FP register
    );
    return f32_to_fp8_e5m2(fb);
}

inline fp8_e5m2 asm_fp8_e5m2_sqrt_fp32(float fa) {
    float fb;
    asm volatile (
        "fsqrt.s %0, %1, rne\n"
        : "=f"(fb)        // output in FP register
        : "f"(fa)              // input in FP register
    );
    return f32_to_fp8_e5m2(fb);
}

/*
  Numerical verification
*/

void spatz_verify(uint32_t avl, uint8_t* act_vec, const uint8_t* exp_vec, const float tol) {
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

    printf("Errors above threshold (E5M2: %.3ff): %d out of %d\n", tol, tot_err, avl);
}

// for widening instruction checks
void spatz_verify_16(uint32_t avl, uint16_t* act_vec, const uint16_t* exp_vec, const float tol) {
    uint16_t exp_fp16, act_fp16;
    float exp_f32, act_f32;
    uint32_t tot_err = 0;

    for (uint32_t i = 0; i < avl; i++) {
        exp_fp16 = exp_vec[i];
        act_fp16 = act_vec[i];
        exp_f32 = fp16_to_f32(exp_fp16);
        act_f32 = fp16_to_f32(act_fp16);

        float err = exp_f32 - act_f32;
        if (err > tol || err < -tol) {
            printf("%3d | exp: 0x%04x (%.5f), act: 0x%04x (%.5f), err: %.5f\n",
                   i, exp_fp16, exp_f32, act_fp16, act_f32, err);
            tot_err += 1;
        }
    }

    printf("Errors above threshold (E5M2: %.3ff): %d out of %d\n", tol, tot_err, avl);
}

#endif