#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/tricore/tricore_debug.h"

static void tricore_debug_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    exit(1);

}

static uint64_t tricore_debug_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0xdeadbeef;
}

static void tricore_debug_reset(DeviceState *dev)
{
}

static const MemoryRegionOps tricore_debug_ops = {
    .read = tricore_debug_read,
    .write = tricore_debug_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void tricore_debug_init(Object *obj)
{
    TriCoreDEBUGState *s = TRICORE_DEBUG(obj);
   /* map memory */
    memory_region_init_io(&s->iomem, OBJECT(s), &tricore_debug_ops, s,
                          "tricore_debug", 0x3ff);
}

static Property tricore_debug_properties[] = {
    DEFINE_PROP_END_OF_LIST()
};

static void tricore_debug_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = tricore_debug_properties;
    dc->reset = tricore_debug_reset;
}

static const TypeInfo tricore_debug_info = {
    .name          = TYPE_TRICORE_DEBUG,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TriCoreDEBUGState),
    .instance_init = tricore_debug_init,
    .class_init    = tricore_debug_class_init,
};

static void tricore_debug_register_types(void)
{
    type_register_static(&tricore_debug_info);
}

type_init(tricore_debug_register_types)
