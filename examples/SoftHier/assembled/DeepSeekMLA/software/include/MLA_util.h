#ifndef _MLA_UTIL_H_
#define _MLA_UTIL_H_

#define REPEAT_1(x) x
#define REPEAT_2(x) REPEAT_1(x) x
#define REPEAT_4(x) REPEAT_2(x) REPEAT_2(x)
#define REPEAT_8(x) REPEAT_4(x) REPEAT_4(x)
#define REPEAT_16(x) REPEAT_8(x) REPEAT_8(x)
#define REPEAT_32(x) REPEAT_16(x) REPEAT_16(x)
#define REPEAT_64(x) REPEAT_32(x) REPEAT_32(x)
#define REPEAT_128(x) REPEAT_64(x) REPEAT_64(x)


inline void MLA_vector_rowmax_MV_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src, uint32_t V_dst){
    uint32_t vl;
    uint16_t sqrt_dim = 0x2DAB;
    asm volatile("fld fa5, (%0)" ::"r"(&sqrt_dim));
    asm volatile("li a7, 0":::"a7");
    //load V_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_src));
    //Scale and Redmax for each row of M_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfmul.vf v8, v8, fa5");\
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x1e88b057):"a7") /*vfredmax.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedMax(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_dst));
}


inline void MLA_vector_EXP_MV(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfsub.vf v8, v8, fa5");\
            asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}

inline void MLA_vector_EXP_VV_V(uint32_t vector_length, uint32_t Vr_src, uint32_t V_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(Vr_src));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(V_src));
    asm volatile("vfsub.vv v8, v0, v8");
    asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(V_dst));
}


inline void MLA_vector_rowsum_M_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("li a7, 0":::"a7");
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vmv.v.x v0, %0" ::"r"(0));
    //Redsum for each row of M_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x0688b057):"a7") /*vfredsum.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedSum(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_dst));
}

//l = e-*-lr + l
inline void MLA_vector_update_l(uint32_t vector_length, uint32_t lr, uint32_t e, uint32_t l){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(lr));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(e));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(l));
    asm volatile("vfmul.vv v8, v8, v0");
    asm volatile("vfadd.vv v16, v16, v8");
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(l));
}

void MLA_vector_M_div_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfdiv.vf v8, v8, fa5");\
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}

#endif