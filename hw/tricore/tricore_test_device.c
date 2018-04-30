/*
 *  Copyright (c) 2018 Bastian Koppelmann Paderborn University
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/tricore/tricore_test_device.h"

static void tricore_test_device_write(void *opaque, hwaddr offset,
                                      uint64_t value, unsigned size)
{
    exit(value);
}

static uint64_t tricore_test_device_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    return 0xdeadbeef;
}

static void tricore_test_device_reset(DeviceState *dev)
{
}

static const MemoryRegionOps tricore_test_device_ops = {
    .read = tricore_test_device_read,
    .write = tricore_test_device_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void tricore_test_device_init(Object *obj)
{
    TriCoreTestDeviceState *s = TRICORE_TEST_DEVICE(obj);
   /* map memory */
    memory_region_init_io(&s->iomem, OBJECT(s), &tricore_test_device_ops, s,
                          "tricore_test_device", 0x3ff);
}

static Property tricore_test_device_properties[] = {
    DEFINE_PROP_END_OF_LIST()
};

static void tricore_test_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = tricore_test_device_properties;
    dc->reset = tricore_test_device_reset;
}

static const TypeInfo tricore_test_device_info = {
    .name          = TYPE_TRICORE_TEST_DEVICE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TriCoreTestDeviceState),
    .instance_init = tricore_test_device_init,
    .class_init    = tricore_test_device_class_init,
};

static void tricore_test_device_register_types(void)
{
    type_register_static(&tricore_test_device_info);
}

type_init(tricore_test_device_register_types)
