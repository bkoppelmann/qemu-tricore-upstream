#include "macros.h"
.text
.global _start
_start:
    TEST_R(abs, 1, 0, 0)

    TEST_PASSFAIL

