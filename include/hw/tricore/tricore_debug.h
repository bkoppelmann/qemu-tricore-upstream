#ifndef HW_TRICORE_DEBUG_H
#define HW_TRICORE_DEBUG_H

#include "hw/sysbus.h"
#include "hw/hw.h"

#define TYPE_TRICORE_DEBUG "tricore_debug"
#define TRICORE_DEBUG(obj) \
    OBJECT_CHECK(TriCoreDEBUGState, (obj), TYPE_TRICORE_DEBUG)
#define NUM_CORES 1

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion iomem;

} TriCoreDEBUGState;

#endif

