/*
 * QEMU RISC-V Generic Board Support
 *
 * Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 * This provides a RISC-V Board with the following devices:
 *
 * 0) HTIF Syscall Proxy
 * 1) HTIF Console
 * 2) HTIF Block Device
 *
 * These are created by htif_mm_init below.
 *
 * The following "Shim" devices allow support for interrupts triggered by the
 * processor itself (writes to the MIP/SIP CSRs):
 *
 * softint0 - SSIP
 * softint1 - STIP
 * softint2 - MSIP
 *
 * These are created by softint_mm_init below.
 *
 * This board currently uses a hardcoded devicetree that indicates one hart
 * and 2048 MB of memory.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/char/serial.h"
#include "cpu.h"
#include "hw/riscv/softint.h"
#include "hw/riscv/htif/htif.h"
#include "hw/riscv/htif/frontend.h"
#include "hw/block/fdc.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/i2c/smbus.h"
#include "block/block.h"
#include "hw/block/flash.h"
#include "block/block_int.h" /* move later */
#include "hw/riscv/riscv.h"
#include "hw/riscv/cpudevs.h"
#include "hw/pci/pci.h"
#include "sysemu/char.h"
#include "sysemu/sysemu.h"
#include "sysemu/arch_init.h"
#include "qemu/log.h"
#include "hw/riscv/bios.h"
#include "hw/ide.h"
#include "hw/loader.h"
#include "elf.h"
#include "hw/timer/mc146818rtc.h"
#include "hw/timer/i8254.h"
#include "sysemu/blockdev.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"             /* SysBusDevice */
#include "qemu/host-utils.h"
#include "sysemu/qtest.h"
#include "qemu/error-report.h"
#include "hw/empty_slot.h"
#include "qemu/error-report.h"
#include "sysemu/block-backend.h"

#define TYPE_RISCV_BOARD "riscv-board"
#define RISCV_BOARD(obj) OBJECT_CHECK(BoardState, (obj), TYPE_RISCV_BOARD)

/* TODO: don't hardcode, don't rely on items past 0x408 to be init zero */
#define DEVTREE_LEN 65944
char devtree[DEVTREE_LEN] = {
0xd0,
0x0d, 0xfe, 0xed, 0x00, 0x00, 0x01, 0x98, 0x00, 0x00, 0x00,
0x38, 0x00, 0x00, 0x01, 0x58, 0x00, 0x00, 0x00, 0x28, 0x00,
0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01, 0x20, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
0x0f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00,
0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1b, 0x53, 0x70, 0x69,
0x6b, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x6d,
0x65, 0x6d, 0x6f, 0x72, 0x79, 0x40, 0x30, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00,
0x00, 0x00, 0x21, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x00,
0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x00,
0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x63, 0x70, 0x75,
0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00,
0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
0x01, 0x63, 0x70, 0x75, 0x40, 0x38, 0x30, 0x30, 0x30, 0x31,
0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x21, 0x63,
0x70, 0x75, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
0x06, 0x00, 0x00, 0x00, 0x31, 0x72, 0x69, 0x73, 0x63, 0x76,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
0x0b, 0x00, 0x00, 0x00, 0x3c, 0x72, 0x76, 0x36, 0x34, 0x69,
0x6d, 0x61, 0x66, 0x64, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,
0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2d, 0x00,
0x00, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00,
0x00, 0x00, 0x09, 0x23, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73,
0x73, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00, 0x23, 0x73,
0x69, 0x7a, 0x65, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00,
0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x00, 0x64, 0x65, 0x76, 0x69,
0x63, 0x65, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x00, 0x72, 0x65,
0x67, 0x00, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x74, 0x69, 0x62,
0x6c, 0x65, 0x00, 0x69, 0x73, 0x61, 0x00,
};



typedef struct {
    SysBusDevice parent_obj;
} BoardState;

static struct _loaderparams {
    int ram_size;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
} loaderparams;

uint64_t identity_translate(void *opaque, uint64_t addr)
{
    return addr;
}

static int64_t load_kernel(void)
{
    int64_t kernel_entry, kernel_high;
    int big_endian;
    big_endian = 0;

    if (load_elf(loaderparams.kernel_filename, identity_translate, NULL,
                 (uint64_t *)&kernel_entry, NULL, (uint64_t *)&kernel_high,
                 big_endian, EM_RISCV, 1, 0) < 0) {
        fprintf(stderr, "qemu: could not load kernel '%s'\n",
                loaderparams.kernel_filename);
        exit(1);
    }
    return kernel_entry;
}

static void main_cpu_reset(void *opaque)
{
    RISCVCPU *cpu = opaque;
    cpu_reset(CPU(cpu));
}

/* hack for now to set memory size without implementing devicetree generation
 * modifies memory size and addresses */
static void set_devtree_memsize(uint64_t memsize)
{

    uint64_t addr1 = memsize | 0x1000;

    if (memsize <= 0x1000) {
        fprintf(stderr, "Warning: Insufficient memsize for bbl. If you are not "
                        "using bbl, you may disregard this message\n");
    }

    int i;
    for (i = 0; i < 8; i++) {
        devtree[179 - i] = (memsize >> (i * 8)) & 0xFF;
        devtree[327 - i] = (addr1 >> (i * 8)) & 0xFF;
    }
}

static void riscv_board_init(MachineState *args)
{
    ram_addr_t ram_size = args->ram_size;
    const char *cpu_model = args->cpu_model;
    const char *kernel_filename = args->kernel_filename;
    const char *kernel_cmdline = args->kernel_cmdline;
    const char *initrd_filename = args->initrd_filename;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    RISCVCPU *cpu;
    CPURISCVState *env;
    int i;

    DriveInfo *htifbd_drive;
    const char *htifbd_fname; /* htif block device filename */

    /* Make sure the first 3 serial ports are associated with a device. */
    for (i = 0; i < 3; i++) {
        if (!serial_hds[i]) {
            char label[32];
            snprintf(label, sizeof(label), "serial%d", i);
            serial_hds[i] = qemu_chr_new(label, "null", NULL);
        }
    }

    /* init CPUs */
    if (cpu_model == NULL) {
        cpu_model = "riscv-generic";
    }

    for (i = 0; i < smp_cpus; i++) {
        cpu = cpu_riscv_init(cpu_model);
        if (cpu == NULL) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(1);
        }
        env = &cpu->env;

        /* Init internal devices */
        cpu_riscv_irq_init_cpu(env);
        cpu_riscv_clock_init(env);
        qemu_register_reset(main_cpu_reset, cpu);
    }
    cpu = RISCV_CPU(first_cpu);
    env = &cpu->env;

    /* register system main memory (actual RAM) */
    memory_region_init_ram(main_mem, NULL, "riscv_board.ram",
                           ram_size + DEVTREE_LEN, &error_fatal);
    /* for CSR_MIOBASE */
    env->memsize = ram_size;
    vmstate_register_ram_global(main_mem);
    memory_region_add_subregion(system_memory, 0x0, main_mem);

    if (kernel_filename) {
        loaderparams.ram_size = ram_size;
        loaderparams.kernel_filename = kernel_filename;
        loaderparams.kernel_cmdline = kernel_cmdline;
        loaderparams.initrd_filename = initrd_filename;
        load_kernel();
    }

    /* TODO: still necessary?
       write memory amount in MiB to 0x0
       stl_p(memory_region_get_ram_ptr(main_mem), ram_size >> 20); */

    /* set memory size in devicetree */
    set_devtree_memsize(ram_size);

    /* copy in the devtree */
    int q;
    for (q = 0; q < DEVTREE_LEN; q++) {
        stb_p(memory_region_get_ram_ptr(main_mem) + ram_size + q, devtree[q]);
    }

    /* add serial device 0x3f8-0x3ff
       serial_mm_init(system_memory, 0xF0000400, 0, env->irq[5], 1843200/16,
             serial_hds[0], DEVICE_NATIVE_ENDIAN); */

    /* setup HTIF Block Device if one is specified as -hda FILENAME */
    htifbd_drive = drive_get_by_index(IF_IDE, 0);
    if (NULL == htifbd_drive) {
        htifbd_fname = NULL;
    } else {
        htifbd_fname = blk_bs(blk_by_legacy_dinfo(htifbd_drive))->filename;
        /* get rid of orphaned drive warning, until htif uses the
           real blockdev */
        htifbd_drive->is_default = true;
    }

    /* add htif device at 0xFFFFFFFFF0000000 */
    htif_mm_init(system_memory, 0xFFFFFFFFF0000000L, env->irq[4], main_mem,
            htifbd_fname, kernel_cmdline, env, serial_hds[0]);

    /* Softint "devices" for cleaner handling of CPU-triggered interrupts */
    softint_mm_init(system_memory, 0xFFFFFFFFF0000020L, env->irq[1], main_mem,
            env, "SSIP");
    softint_mm_init(system_memory, 0xFFFFFFFFF0000040L, env->irq[2], main_mem,
            env, "STIP");
    softint_mm_init(system_memory, 0xFFFFFFFFF0000060L, env->irq[3], main_mem,
            env, "MSIP");

    /* TODO: VIRTIO */
}

static void riscv_board_machine_init(MachineClass *mc)
{
    mc->desc = "RISC-V Generic Board";
    mc->init = riscv_board_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("riscv", riscv_board_machine_init)
