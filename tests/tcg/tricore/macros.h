#define REG_RS1 %d0
#define REG_RS2 %d1
#define REG_CALC_RESULT %d1
#define REG_CALC_PSW %d2
#define REG_CORRECT_PSW %d3
#define REG_TEMP_LI %d10
#define REG_TEMP %d11
#define REG_TEST_NUM %d14
#define REG_CORRECT_RESULT %d15
#define REG_DEV_ADDR %a15

#define TEST_CASE(num, testreg, correct, code...) \
test_ ## num: \
    code;     \
    LI(REG_CORRECT_RESULT, correct) \
    mov REG_TEST_NUM, num;    \
    jne testreg, REG_CORRECT_RESULT, fail \


#define TEST_CASE_PSW(num, testreg, correct, correct_psw, code...) \
test_ ## num: \
    code;     \
    LI(REG_CORRECT_RESULT, correct) \
    mov REG_TEST_NUM, num;    \
    jne testreg, REG_CORRECT_RESULT, fail; \
    mfcr REG_CALC_PSW, $psw;       \
    LI(REG_CORRECT_PSW, correct_psw)      \
    mov REG_TEST_NUM, num;        \
    jne REG_CALC_PSW, REG_CORRECT_PSW, fail;   \


#define LI(reg, val) \
    mov.u reg, lo:val; \
    movh REG_TEMP_LI, hi:val; \
    or reg, reg, REG_TEMP_LI; \

#define TEST_PASSFAIL \
        j pass; \
fail: \
        LI(REG_TEMP, 0xf0001337)          \
        mov.a REG_DEV_ADDR, REG_TEMP;     \
        st.w [REG_DEV_ADDR], REG_TEST_NUM;\
        debug;                            \
        j fail;                           \
pass: \
        LI(REG_TEMP, 0xf0001337)          \
        mov.a REG_DEV_ADDR, REG_TEMP;     \
        mov REG_TEST_NUM, 0;              \
        st.w [REG_DEV_ADDR], REG_TEST_NUM;\
        debug;                            \
        j pass;                           \


#define TEST_R(insn, num, result, rs1) \
    TEST_CASE(num, REG_CALC_RESULT, result,        \
    LI(REG_RS1, rs1); \
    insn REG_CALC_RESULT, REG_RS1; \
    )

#define TEST_RR(insn, num, result, rs1, rs2) \
    TEST_CASE(num, REG_CALC_RESULT, result,        \
    LI(REG_RS1, rs1); \
    LI(REG_RS2, rs2); \
    insn REG_CALC_RESULT, REG_RS1, REG_RS2; \
    )

#define TEST_R_PSW(insn, num, result, psw, rs1) \
    TEST_CASE_PSW(num, REG_CALC_RESULT, result, psw,         \
    LI(REG_RS1, rs1); \
    rstv;         \
    insn REG_CORRECT_RESULT, REG_RS1; \
    )

