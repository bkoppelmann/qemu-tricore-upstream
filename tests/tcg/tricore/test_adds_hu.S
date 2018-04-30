#include "macros.h"
.text
.global _start
_start:
    TEST_RR_PSW(adds.hu, 1, 0xfffffffa, 0x00000b80, 0xfffffffa, 0x0)
    TEST_RR_PSW(adds.h , 2, 0xfffeffe9, 0x00000b80, 0xffffffea, 0xffffffff)

    TEST_PASSFAIL

