
#define TEST_CASE(num, testreg, correct, code...) \
test_ ## num: \
    code;     \
    mov %d15 correct; \
    mov %d14, num;    \
    jne testreg, %d15, fail \


#define TEST_PASSFAIL \
        j pass; \
fail: \
        movh %d13, 0xf000;  \
        addi %d13, %d13, 0x1337; \
        mov.a %a15, %d13;       \
        st.w [%a15], %d14;      \
        debug;                   \
        j fail;                  \
pass: \
        movh %d13, 0xf000;  \
        addi %d13, %d13, 0x1337; \
        mov.a %a15, %d13;       \
        mov %d14, 0;            \
        st.w [%a15], %d14;      \
        debug;                  \
        j pass;


#define TEST_R(insn, num, result, rs1) \
    TEST_CASE(num, %d1, result,        \
    mov %d0, rs1; \
    insn %d1, %d0; \
    )
