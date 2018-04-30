
#define TEST_CASE( testnum, testreg, correctval, code... ) \
test_ ## testnum: \
    code; \
    mov  d15, correctval; \
    mov  d14, testnum; \
    jne d15, d14, fail;

