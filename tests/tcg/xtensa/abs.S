#include "macros.h"

.text
.global _start
_start:

    movh %d0, 32768
    abs %d1, %d0


    mov %d2, 0
    abs %d3, %d2

fail:
    movh %d13, 0xffff
    ori %d13

