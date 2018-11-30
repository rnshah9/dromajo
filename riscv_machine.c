//******************************************************************************
// Copyright (C) 2018, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

/*
 * RISCV machine
 *
 * Copyright (c) 2016-2017 Fabrice Bellard
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <elf.h>

#include "cutils.h"
#include "iomem.h"
#include "riscv_cpu.h"
#include "riscv_machine.h"

/* RISCV machine */

//#define DUMP_UART
//#define DUMP_CLINT
//#define DUMP_HTIF
//#define DUMP_PLIC

#define UART0_BASE_ADDR 0x54000000
// sifive,uart
#define UART0_IRQ 3 // Same as qemu UART0 (qemu has 2 sifive uarts)
enum {
    SIFIVE_UART_TXFIFO        = 0,
    SIFIVE_UART_RXFIFO        = 4,
    SIFIVE_UART_TXCTRL        = 8,
    SIFIVE_UART_TXMARK        = 10,
    SIFIVE_UART_RXCTRL        = 12,
    SIFIVE_UART_RXMARK        = 14,
    SIFIVE_UART_IE            = 16,
    SIFIVE_UART_IP            = 20,
    SIFIVE_UART_DIV           = 24,
    SIFIVE_UART_MAX           = 32
};

enum {
    SIFIVE_UART_IE_TXWM       = 1, /* Transmit watermark interrupt enable */
    SIFIVE_UART_IE_RXWM       = 2  /* Receive watermark interrupt enable */
};

enum {
    SIFIVE_UART_IP_TXWM       = 1, /* Transmit watermark interrupt pending */
    SIFIVE_UART_IP_RXWM       = 2  /* Receive watermark interrupt pending */
};
#define UART0_SIZE      32

#define HTIF_BASE_ADDR 0x40008000
#define IDE_BASE_ADDR  0x40009000
#define VIRTIO_BASE_ADDR 0x40010000
#define VIRTIO_SIZE      0x1000
#define VIRTIO_IRQ       1
#define PLIC_BASE_ADDR 0x40100000
#define PLIC_SIZE      0x00400000
#define FRAMEBUFFER_BASE_ADDR 0x41000000

#define RTC_FREQ 10000000

static uint64_t rtc_get_real_time(RISCVMachine *s)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * RTC_FREQ +
        (ts.tv_nsec / (1000000000 / RTC_FREQ));
}

static uint64_t rtc_get_time(RISCVMachine *m)
{
    uint64_t val;
    if (m->rtc_real_time) {
        assert(0); // Verification should not set RTC or it would not be deterministic
        val = rtc_get_real_time(m) - m->rtc_start_time;
    } else {
        val = riscv_cpu_get_cycles(m->cpu_state) / RTC_FREQ_DIV;
    }
    //    fprintf(stderr, "rtc_time=%" PRId64 "\n", val);
    return val;
}

static uint32_t htif_read(void *opaque, uint32_t offset,
                          int size_log2)
{
    RISCVMachine *s = opaque;
    uint32_t val;

    assert(size_log2 == 2);
    switch(offset) {
    case 0:
        val = s->htif_tohost;
        break;
    case 4:
        val = s->htif_tohost >> 32;
        break;
    case 8:
        val = s->htif_fromhost;
        break;
    case 12:
        val = s->htif_fromhost >> 32;
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void htif_handle_cmd(RISCVMachine *s)
{
    uint32_t device, cmd;


    device = s->htif_tohost >> 56;
    cmd = (s->htif_tohost >> 48) & 0xff;
#ifdef DUMP_HTIF
    fprintf(stderr, "htif_handle_cmd: device=%d cmd=%d tohost=0x%016" PRIx64 "\n", device, cmd, s->htif_tohost);
#endif
    if (s->htif_tohost == 1) {
        /* shuthost */
    } else if (device == 1 && cmd == 1) {
        uint8_t buf[1];
        buf[0] = s->htif_tohost & 0xff;
        s->common.console->write_data(s->common.console->opaque, buf, 1);
        s->htif_tohost = 0;
        s->htif_fromhost = ((uint64_t)device << 56) | ((uint64_t)cmd << 48);
    } else if (device == 1 && cmd == 0) {
        /* request keyboard interrupt */
        s->htif_tohost = 0;
    } else {
#ifdef DUMP_HTIF
      // NOTE: This happens all the time wiht firesim, but it is OK :)
      fprintf(stderr, "HTIF: unsupported tohost=0x%016" PRIx64 "\n", s->htif_tohost);
#endif
    }
}

static void htif_write(void *opaque, uint32_t offset, uint32_t val,
                       int size_log2)
{
    RISCVMachine *s = opaque;

    assert(size_log2 == 2);
    switch(offset) {
    case 0:
        s->htif_tohost = (s->htif_tohost & ~0xffffffff) | val;
        // fesvr/Spike, processes commands when this is non-zero
        htif_handle_cmd(s);
        break;
    case 4:
        s->htif_tohost = (s->htif_tohost & 0xffffffff) | ((uint64_t)val << 32);
        htif_handle_cmd(s);
        break;
    case 8:
        s->htif_fromhost = (s->htif_fromhost & ~0xffffffff) | val;
        break;
    case 12:
        s->htif_fromhost = (s->htif_fromhost & 0xffffffff) |
            (uint64_t)val << 32;
        break;
    default:
        break;
    }
}

#if 0
static void htif_poll(RISCVMachine *s)
{
    uint8_t buf[1];
    int ret;

    if (s->htif_fromhost == 0) {
        ret = s->console->read_data(s->console->opaque, buf, 1);
        if (ret == 1) {
            s->htif_fromhost = ((uint64_t)1 << 56) | ((uint64_t)0 << 48) |
                buf[0];
        }
    }
}
#endif

typedef struct SiFiveUARTState {
  CharacterDevice *cs; // Console

    uint32_t irq;
    uint8_t rx_fifo[8];
    unsigned int rx_fifo_len;
    uint32_t ie;
    uint32_t ip;
    uint32_t txctrl;
    uint32_t rxctrl;
    uint32_t div;
} SiFiveUARTState;

static void uart_update_irq(SiFiveUARTState *s)
{
    int cond = 0;
    if ((s->ie & SIFIVE_UART_IE_RXWM) && s->rx_fifo_len) {
        cond = 1;
    }
    if (cond) {
      fprintf(stderr,"uart_update_irq: FIXME we should raise IRQ saying that there is new data\n");
    }
}

static uint32_t uart_read(void *opaque, uint32_t offset, int size_log2)
{
    SiFiveUARTState *s = opaque;

#ifdef DUMP_UART
    fprintf(stderr, "uart_read: offset=%x size_log2=%d\n", offset, size_log2);
#endif
    switch (offset) {
    case SIFIVE_UART_RXFIFO:
        {
            CharacterDevice *cs = s->cs;
            unsigned char r;
            int ret = cs->read_data(cs->opaque, &r, 1);
            if (ret) {
#ifdef DUMP_UART
                fprintf(stderr, "uart_read: val=%x\n", r);
#endif
                return r;
            }
            return 0x80000000;
        }
    case SIFIVE_UART_TXFIFO:
        return 0; /* Should check tx fifo */
    case SIFIVE_UART_IE:
        return s->ie;
    case SIFIVE_UART_IP:
        return s->rx_fifo_len ? SIFIVE_UART_IP_RXWM : 0;
    case SIFIVE_UART_TXCTRL:
        return s->txctrl;
    case SIFIVE_UART_RXCTRL:
        return s->rxctrl;
    case SIFIVE_UART_DIV:
        return s->div;
    }

    fprintf(stderr, "%s: bad read: offset=0x%x\n", __func__, (int)offset);
    return 0;
}

static void uart_write(void *opaque, uint32_t offset, uint32_t val, int size_log2)
{
    SiFiveUARTState *s = opaque;
    CharacterDevice *cs = s->cs;
    unsigned char ch = val;

#ifdef DUMP_UART
    fprintf(stderr, "uart_write: offset=%x val=%x size_log2=%d\n", offset, val, size_log2);
#endif

    switch (offset) {
    case SIFIVE_UART_TXFIFO:
        cs->write_data(cs->opaque, &ch, 1);
        return;
    case SIFIVE_UART_IE:
        s->ie = val;
        uart_update_irq(s);
        return;
    case SIFIVE_UART_TXCTRL:
        s->txctrl = val;
        return;
    case SIFIVE_UART_RXCTRL:
        s->rxctrl = val;
        return;
    case SIFIVE_UART_DIV:
        s->div = val;
        return;
    }

    fprintf(stderr, "%s: bad write: addr=0x%x v=0x%x\n", __func__, (int)offset, (int)val);
}

static uint32_t clint_read(void *opaque, uint32_t offset, int size_log2)
{
    RISCVMachine *m = opaque;
    uint32_t val;

    assert(size_log2 == 2);
    switch(offset) {
    case 0xbff8:
        val = rtc_get_time(m);
        break;
    case 0xbffc:
        val = rtc_get_time(m) >> 32;
        break;
    case 0x4000:
        val = m->timecmp;
        break;
    case 0x4004:
        val = m->timecmp >> 32;
        break;
    default:
        val = 0;
        break;
    }

#ifdef DUMP_CLINT
    fprintf(stderr, "clint_read: offset=%x val=%x\n", offset, val);
#endif

    return val;
}

static void clint_write(void *opaque, uint32_t offset, uint32_t val,
                        int size_log2)
{
    RISCVMachine *m = opaque;

    assert(size_log2 == 2);
    switch(offset) {
    case 0x4000:
        m->timecmp = (m->timecmp & ~0xffffffff) | val;
        riscv_cpu_reset_mip(m->cpu_state, MIP_MTIP);
        break;
    case 0x4004:
        m->timecmp = (m->timecmp & 0xffffffff) | ((uint64_t)val << 32);
        riscv_cpu_reset_mip(m->cpu_state, MIP_MTIP);
        break;
    // WARNING: RISCVEMU assumes same for cpu clock and clint. Create different clocks if needed
    case 0xbff8:
        {
          uint32_t val2 = rtc_get_time(m);
          if (val2 != val) {
            fprintf(stderr, "clint_write: 32b timem=%d rtc_get_time=%d\n", val, val2);
          }
          assert(val2 == val);
        }
        break;
    case 0xbffc:
        {
          uint32_t val2 = rtc_get_time(m) >> 32;
          if (val2 != val) {
            fprintf(stderr, "clint_write: 32h timem=%d rtc_get_time=%d\n", val, val2);
          }
          assert(val2 == val);
        }
        break;
    default:
        break;
    }

#ifdef DUMP_CLINT
    fprintf(stderr, "clint_write: offset=%x val=%x\n", offset, val);
#endif
}

static void plic_update_mip(RISCVMachine *s)
{
    RISCVCPUState *cpu = s->cpu_state;
    uint32_t mask;
    mask = s->plic_pending_irq & ~s->plic_served_irq;
    if (mask) {
        riscv_cpu_set_mip(cpu, MIP_MEIP | MIP_SEIP);
    } else {
        riscv_cpu_reset_mip(cpu, MIP_MEIP | MIP_SEIP);
    }
}

#define PLIC_HART_BASE 0x200000
#define PLIC_HART_SIZE 0x1000

static uint32_t plic_read(void *opaque, uint32_t offset, int size_log2)
{
    RISCVMachine *s = opaque;
    uint32_t val, mask;
    int i;
    assert(size_log2 == 2);
    switch(offset) {
    case PLIC_HART_BASE:
        val = 0;
        break;
    case PLIC_HART_BASE + 4:
        mask = s->plic_pending_irq & ~s->plic_served_irq;
        if (mask != 0) {
            i = ctz32(mask);
            s->plic_served_irq |= 1 << i;
            plic_update_mip(s);
            val = i + 1;
        } else {
            val = 0;
        }
        break;
    default:
        val = 0;
        break;
    }
#ifdef DUMP_PLIC
    fprintf(stderr, "plic_read: offset=%x val=%x\n", offset, val);
#endif

    return val;
}

static void plic_write(void *opaque, uint32_t offset, uint32_t val,
                       int size_log2)
{
    RISCVMachine *s = opaque;

    assert(size_log2 == 2);
    switch(offset) {
    case PLIC_HART_BASE + 4:
        val--;
        if (val < 32) {
            s->plic_served_irq &= ~(1 << val);
            plic_update_mip(s);
        }
        break;
    default:
        break;
    }
#ifdef DUMP_PLIC
    fprintf(stderr, "plic_write: offset=%x val=%x\n", offset, val);
#endif
}

static void plic_set_irq(void *opaque, int irq_num, int state)
{
    RISCVMachine *s = opaque;
    uint32_t mask;

    mask = 1 << (irq_num - 1);
    if (state)
        s->plic_pending_irq |= mask;
    else
        s->plic_pending_irq &= ~mask;
    plic_update_mip(s);
}

static uint8_t *get_ram_ptr(RISCVMachine *s, uint64_t paddr)
{
    PhysMemoryRange *pr = get_phys_mem_range(s->mem_map, paddr);
    if (!pr || !pr->is_ram)
        return NULL;
    return pr->phys_mem + (uintptr_t)(paddr - pr->addr);
}

/* FDT machine description */

#define FDT_MAGIC       0xd00dfeed
#define FDT_VERSION     17

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version; /* <= 17 */
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_reserve_entry {
       uint64_t address;
       uint64_t size;
};

#define FDT_BEGIN_NODE  1
#define FDT_END_NODE    2
#define FDT_PROP        3
#define FDT_NOP         4
#define FDT_END         9

typedef struct {
    uint32_t *tab;
    int tab_len;
    int tab_size;
    int open_node_count;

    char *string_table;
    int string_table_len;
    int string_table_size;
} FDTState;

static FDTState *fdt_init(void)
{
    FDTState *s;
    s = mallocz(sizeof(*s));
    return s;
}

static void fdt_alloc_len(FDTState *s, int len)
{
    int new_size;
    if (unlikely(len > s->tab_size)) {
        new_size = max_int(len, s->tab_size * 3 / 2);
        s->tab = realloc(s->tab, new_size * sizeof(uint32_t));
        s->tab_size = new_size;
    }
}

static void fdt_put32(FDTState *s, int v)
{
    fdt_alloc_len(s, s->tab_len + 1);
    s->tab[s->tab_len++] = cpu_to_be32(v);
}

/* the data is zero padded */
static void fdt_put_data(FDTState *s, const uint8_t *data, int len)
{
    int len1;

    len1 = (len + 3) / 4;
    fdt_alloc_len(s, s->tab_len + len1);
    memcpy(s->tab + s->tab_len, data, len);
    memset((uint8_t *)(s->tab + s->tab_len) + len, 0, -len & 3);
    s->tab_len += len1;
}

static void fdt_begin_node(FDTState *s, const char *name)
{
    fdt_put32(s, FDT_BEGIN_NODE);
    fdt_put_data(s, (uint8_t *)name, strlen(name) + 1);
    s->open_node_count++;
}

static void fdt_begin_node_num(FDTState *s, const char *name, uint64_t n)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s@%" PRIx64, name, n);
    fdt_begin_node(s, buf);
}

static void fdt_end_node(FDTState *s)
{
    fdt_put32(s, FDT_END_NODE);
    s->open_node_count--;
}

static int fdt_get_string_offset(FDTState *s, const char *name)
{
    int pos, new_size, name_size, new_len;

    pos = 0;
    while (pos < s->string_table_len) {
        if (!strcmp(s->string_table + pos, name))
            return pos;
        pos += strlen(s->string_table + pos) + 1;
    }
    /* add a new string */
    name_size = strlen(name) + 1;
    new_len = s->string_table_len + name_size;
    if (new_len > s->string_table_size) {
        new_size = max_int(new_len, s->string_table_size * 3 / 2);
        s->string_table = realloc(s->string_table, new_size);
        s->string_table_size = new_size;
    }
    pos = s->string_table_len;
    memcpy(s->string_table + pos, name, name_size);
    s->string_table_len = new_len;
    return pos;
}

static void fdt_prop(FDTState *s, const char *prop_name,
                     const void *data, int data_len)
{
    fdt_put32(s, FDT_PROP);
    fdt_put32(s, data_len);
    fdt_put32(s, fdt_get_string_offset(s, prop_name));
    fdt_put_data(s, data, data_len);
}

static void fdt_prop_tab_u32(FDTState *s, const char *prop_name,
                             uint32_t *tab, int tab_len)
{
    int i;
    fdt_put32(s, FDT_PROP);
    fdt_put32(s, tab_len * sizeof(uint32_t));
    fdt_put32(s, fdt_get_string_offset(s, prop_name));
    for(i = 0; i < tab_len; i++)
        fdt_put32(s, tab[i]);
}

static void fdt_prop_u32(FDTState *s, const char *prop_name, uint32_t val)
{
    fdt_prop_tab_u32(s, prop_name, &val, 1);
}

static void fdt_prop_tab_u64_2(FDTState *s, const char *prop_name,
                               uint64_t v0, uint64_t v1)
{
    uint32_t tab[4];
    tab[0] = v0 >> 32;
    tab[1] = v0;
    tab[2] = v1 >> 32;
    tab[3] = v1;
    fdt_prop_tab_u32(s, prop_name, tab, 4);
}

static void fdt_prop_str(FDTState *s, const char *prop_name,
                         const char *str)
{
    fdt_prop(s, prop_name, str, strlen(str) + 1);
}

/* NULL terminated string list */
static void fdt_prop_tab_str(FDTState *s, const char *prop_name,
                             ...)
{
    va_list ap;
    int size, str_size;
    char *ptr, *tab;

    va_start(ap, prop_name);
    size = 0;
    for(;;) {
        ptr = va_arg(ap, char *);
        if (!ptr)
            break;
        str_size = strlen(ptr) + 1;
        size += str_size;
    }
    va_end(ap);

    tab = malloc(size);
    va_start(ap, prop_name);
    size = 0;
    for(;;) {
        ptr = va_arg(ap, char *);
        if (!ptr)
            break;
        str_size = strlen(ptr) + 1;
        memcpy(tab + size, ptr, str_size);
        size += str_size;
    }
    va_end(ap);

    fdt_prop(s, prop_name, tab, size);
    free(tab);
}

/* write the FDT to 'dst1'. return the FDT size in bytes */
int fdt_output(FDTState *s, uint8_t *dst)
{
    struct fdt_header *h;
    struct fdt_reserve_entry *re;
    int dt_struct_size;
    int dt_strings_size;
    int pos;

    assert(s->open_node_count == 0);

    fdt_put32(s, FDT_END);

    dt_struct_size = s->tab_len * sizeof(uint32_t);
    dt_strings_size = s->string_table_len;

    h = (struct fdt_header *)dst;
    h->magic = cpu_to_be32(FDT_MAGIC);
    h->version = cpu_to_be32(FDT_VERSION);
    h->last_comp_version = cpu_to_be32(16);
    h->boot_cpuid_phys = cpu_to_be32(0);
    h->size_dt_strings = cpu_to_be32(dt_strings_size);
    h->size_dt_struct = cpu_to_be32(dt_struct_size);

    pos = sizeof(struct fdt_header);

    h->off_dt_struct = cpu_to_be32(pos);
    memcpy(dst + pos, s->tab, dt_struct_size);
    pos += dt_struct_size;

    /* align to 8 */
    while ((pos & 7) != 0) {
        dst[pos++] = 0;
    }
    h->off_mem_rsvmap = cpu_to_be32(pos);
    re = (struct fdt_reserve_entry *)(dst + pos);
    re->address = 0; /* no reserved entry */
    re->size = 0;
    pos += sizeof(struct fdt_reserve_entry);

    h->off_dt_strings = cpu_to_be32(pos);
    memcpy(dst + pos, s->string_table, dt_strings_size);
    pos += dt_strings_size;

    /* align to 8, just in case */
    while ((pos & 7) != 0) {
        dst[pos++] = 0;
    }

    h->totalsize = cpu_to_be32(pos);
    return pos;
}

void fdt_end(FDTState *s)
{
    free(s->tab);
    free(s->string_table);
    free(s);
}

static int riscv_build_fdt(RISCVMachine *m, uint8_t *dst, const char *cmd_line)
{
    FDTState *s;
    int size, max_xlen, i, cur_phandle, intc_phandle, plic_phandle;
    char isa_string[128], *q;
    uint32_t misa;
    uint32_t tab[4];
    FBDevice *fb_dev;

    s = fdt_init();

    cur_phandle = 1;

    fdt_begin_node(s, "");
    fdt_prop_u32(s, "#address-cells", 2);
    fdt_prop_u32(s, "#size-cells", 2);
    fdt_prop_str(s, "compatible", "ucbbar,riscvemu-bar_dev");
    fdt_prop_str(s, "model", "ucbbar,riscvemu-bare");

    /* CPU list */
    fdt_begin_node(s, "cpus");
    fdt_prop_u32(s, "#address-cells", 1);
    fdt_prop_u32(s, "#size-cells", 0);
    fdt_prop_u32(s, "timebase-frequency", RTC_FREQ);

    /* cpu */
    fdt_begin_node_num(s, "cpu", 0);
    fdt_prop_str(s, "device_type", "cpu");
    fdt_prop_u32(s, "reg", 0);
    fdt_prop_str(s, "status", "okay");
    fdt_prop_str(s, "compatible", "riscv");

    max_xlen = 64;
    misa = riscv_cpu_get_misa(m->cpu_state);
    q = isa_string;
    q += snprintf(isa_string, sizeof(isa_string), "rv%d", max_xlen);
    for (i = 0; i < 26; i++) {
        if (misa & (1 << i))
            *q++ = 'a' + i;
    }
    *q = '\0';
    fdt_prop_str(s, "riscv,isa", isa_string);

    fdt_prop_str(s, "mmu-type", max_xlen <= 32 ? "sv32" : "sv48");
    fdt_prop_u32(s, "clock-frequency", 2000000000);

    fdt_begin_node(s, "interrupt-controller");
    fdt_prop_u32(s, "#interrupt-cells", 1);
    fdt_prop(s, "interrupt-controller", NULL, 0);
    fdt_prop_str(s, "compatible", "riscv,cpu-intc");
    intc_phandle = cur_phandle++;
    fdt_prop_u32(s, "phandle", intc_phandle);
    fdt_end_node(s); /* interrupt-controller */

    fdt_end_node(s); /* cpu */

    fdt_end_node(s); /* cpus */

    fdt_begin_node_num(s, "memory", RAM_BASE_ADDR);
    fdt_prop_str(s, "device_type", "memory");
    tab[0] = (uint64_t)RAM_BASE_ADDR >> 32;
    tab[1] = RAM_BASE_ADDR;
    tab[2] = m->ram_size >> 32;
    tab[3] = m->ram_size;
    fdt_prop_tab_u32(s, "reg", tab, 4);

    fdt_end_node(s); /* memory */

    fdt_begin_node(s, "soc");
    fdt_prop_u32(s, "#address-cells", 2);
    fdt_prop_u32(s, "#size-cells", 2);
    fdt_prop_tab_str(s, "compatible",
                     "ucbbar,riscvemu-bar-soc", "simple-bus", NULL);
    fdt_prop(s, "ranges", NULL, 0);

    fdt_begin_node_num(s, "clint", CLINT_BASE_ADDR);
    fdt_prop_str(s, "compatible", "riscv,clint0");

    tab[0] = intc_phandle;
    tab[1] = 3; /* M IPI irq */
    tab[2] = intc_phandle;
    tab[3] = 7; /* M timer irq */
    fdt_prop_tab_u32(s, "interrupts-extended", tab, 4);

    fdt_prop_tab_u64_2(s, "reg", CLINT_BASE_ADDR, CLINT_SIZE);

    fdt_end_node(s); /* clint */

    fdt_begin_node_num(s, "plic", PLIC_BASE_ADDR);
    fdt_prop_u32(s, "#interrupt-cells", 1);
    fdt_prop(s, "interrupt-controller", NULL, 0);
    fdt_prop_str(s, "compatible", "riscv,plic0");
    fdt_prop_u32(s, "riscv,ndev", 31);
    fdt_prop_tab_u64_2(s, "reg", PLIC_BASE_ADDR, PLIC_SIZE);

    tab[0] = intc_phandle;
    tab[1] = 9; /* S ext irq */
    tab[2] = intc_phandle;
    tab[3] = 11; /* M ext irq */
    fdt_prop_tab_u32(s, "interrupts-extended", tab, 4);

    plic_phandle = cur_phandle++;
    fdt_prop_u32(s, "phandle", plic_phandle);

    fdt_end_node(s); /* plic */

    for(i = 0; i < m->virtio_count; i++) {
        fdt_begin_node_num(s, "virtio", VIRTIO_BASE_ADDR + i * VIRTIO_SIZE);
        fdt_prop_str(s, "compatible", "virtio,mmio");
        fdt_prop_tab_u64_2(s, "reg", VIRTIO_BASE_ADDR + i * VIRTIO_SIZE,
                           VIRTIO_SIZE);
        tab[0] = plic_phandle;
        tab[1] = VIRTIO_IRQ + i;
        fdt_prop_tab_u32(s, "interrupts-extended", tab, 2);
        fdt_end_node(s); /* virtio */
    }

    // UART
    fdt_begin_node_num(s, "uart", UART0_BASE_ADDR);
    fdt_prop_str(s, "compatible", "sifive,uart0");
    fdt_prop_tab_u64_2(s, "reg", UART0_BASE_ADDR, UART0_SIZE);
    fdt_end_node(s); /* uart */

    fb_dev = m->common.fb_dev;
    if (fb_dev) {
        fdt_begin_node_num(s, "framebuffer", FRAMEBUFFER_BASE_ADDR);
        fdt_prop_str(s, "compatible", "simple-framebuffer");
        fdt_prop_tab_u64_2(s, "reg", FRAMEBUFFER_BASE_ADDR, fb_dev->fb_size);
        fdt_prop_u32(s, "width", fb_dev->width);
        fdt_prop_u32(s, "height", fb_dev->height);
        fdt_prop_u32(s, "stride", fb_dev->stride);
        fdt_prop_str(s, "format", "a8r8g8b8");
        fdt_end_node(s); /* framebuffer */
    }

    fdt_end_node(s); /* soc */

    fdt_begin_node(s, "chosen");
    fdt_prop_str(s, "bootargs", cmd_line ? cmd_line : "");

    fdt_end_node(s); /* chosen */

    fdt_end_node(s); /* / */

    size = fdt_output(s, dst);
#if 0
    {
        FILE *f;
        f = fopen("/tmp/riscvemu.dtb", "wb");
        fwrite(dst, 1, size, f);
        fclose(f);
    }
#endif
    fdt_end(s);
    return size;
}

static void load_elf_image(RISCVMachine *s, const void *image, size_t image_len,
                           uint64_t *entry_point)
{
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)image;
    const Elf64_Phdr *ph = image + ehdr->e_phoff;

    *entry_point = ehdr->e_entry;

    for (int i = 0; i < ehdr->e_phnum; ++i, ++ph)
        if (ph->p_type == PT_LOAD) {
            size_t rounded_size = ph->p_memsz;
            rounded_size = (rounded_size + DEVRAM_PAGE_SIZE - 1) & ~(DEVRAM_PAGE_SIZE - 1);
            cpu_register_ram(s->mem_map, ph->p_vaddr, rounded_size, 0);
            memcpy(get_ram_ptr(s, ph->p_vaddr), image + ph->p_offset, ph->p_filesz);
        }
}

/* Return non-zero on failure */
static int copy_kernel(RISCVMachine *s, const void *buf, size_t buf_len,
                        const char *cmd_line, bool is_elf)
{
    uint32_t fdt_addr;
    uint8_t *ram_ptr;
    uint32_t *q;
    uint64_t entry_point;

    if (buf_len > s->ram_size) {
        vm_error("Kernel too big\n");
        return 1;
    }

    if (is_elf) {
        load_elf_image(s, buf, buf_len, &entry_point);
        if (entry_point != 0x80000000) {
            fprintf(stderr, "RISCVEMU current requires a 0x80000000 starting "
                    "address, image assumes 0x%0lx\n", entry_point);
            return 1;
        }
    }
    else
        memcpy(get_ram_ptr(s, RAM_BASE_ADDR), buf, buf_len);

    ram_ptr = get_ram_ptr(s, ROM_BASE_ADDR);

    fdt_addr = (BOOT_BASE_ADDR-ROM_BASE_ADDR) + 10 * 4;
    riscv_build_fdt(s, ram_ptr + fdt_addr, cmd_line);

    /* jump_addr = 0x80000000 */

    q = (uint32_t *)(ram_ptr + (BOOT_BASE_ADDR-ROM_BASE_ADDR));
    q[0] = 0xf1402573; // csrr    a0, mhartid
    q[1] = 0x00000597; // auipc   a1, 0x0      = BOOT_BASE_ADDR + 3*4
    q[2] = 0x02458593; // addi    a1, a1, 28   = BOOT_BASE_ADDR + 10*4
    q[3] = 0x0010041b; // addiw   s0, zero, 1
    q[4] = 0x01f41413; // slli    s0, s0, 31
    q[5] = 0x7b141073; // csrw    dpc, s0
    q[6] = 0x60300413; // li      s0, 1539
    q[7] = 0x7b041073; // csrw    dcsr, s0
    q[8] = 0x7b200073; // dret

    return 0;
}

static void riscv_flush_tlb_write_range(void *opaque, uint8_t *ram_addr,
                                        size_t ram_size)
{
    RISCVMachine *s = opaque;
    riscv_cpu_flush_tlb_write_range_ram(s->cpu_state, ram_addr, ram_size);
}

void virt_machine_set_defaults(VirtMachineParams *p)
{
    memset(p, 0, sizeof *p);
}

VirtMachine *virt_machine_init(const VirtMachineParams *p)
{
    RISCVMachine *s;
    VIRTIODevice *blk_dev;
    int irq_num, i;
    VIRTIOBusDef vbus_s, *vbus = &vbus_s;

    s = mallocz(sizeof(*s));

    s->ram_size = p->ram_size;
    s->mem_map = phys_mem_map_init();
    /* needed to handle the RAM dirty bits */
    s->mem_map->opaque = s;
    s->mem_map->flush_tlb_write_range = riscv_flush_tlb_write_range;
    s->common.maxinsns = p->maxinsns;
    s->common.snapshot_load_name = p->snapshot_load_name;

    s->cpu_state = riscv_cpu_init(s->mem_map, p->validation_terminate_event);

    /* RAM */
    cpu_register_ram(s->mem_map, RAM_BASE_ADDR, p->ram_size, 0);
    cpu_register_ram(s->mem_map, ROM_BASE_ADDR, ROM_SIZE, 0);

    s->rtc_real_time = p->rtc_real_time;
    if (p->rtc_real_time) {
        s->rtc_start_time = rtc_get_real_time(s);
    }

    SiFiveUARTState *uart = (SiFiveUARTState *)calloc(sizeof *uart, 1);
    uart->irq = UART0_IRQ;
    uart->cs = p->console;

    cpu_register_device(s->mem_map, UART0_BASE_ADDR, UART0_SIZE, uart,
                        uart_read, uart_write, DEVIO_SIZE32);

    cpu_register_device(s->mem_map, CLINT_BASE_ADDR, CLINT_SIZE, s,
                        clint_read, clint_write, DEVIO_SIZE32);
    cpu_register_device(s->mem_map, PLIC_BASE_ADDR, PLIC_SIZE, s,
                        plic_read, plic_write, DEVIO_SIZE32);
    for(i = 1; i < 32; i++) {
        irq_init(&s->plic_irq[i], plic_set_irq, s, i);
    }
    s->htif_tohost_addr=p->htif_base_addr;

    cpu_register_device(s->mem_map,
                        p->htif_base_addr ? p->htif_base_addr : HTIF_BASE_ADDR,
                        16, s, htif_read, htif_write, DEVIO_SIZE32);
    s->common.console = p->console;

    memset(vbus, 0, sizeof(*vbus));
    vbus->mem_map = s->mem_map;
    vbus->addr = VIRTIO_BASE_ADDR;
    irq_num = VIRTIO_IRQ;

    /* virtio console */
    if (p->console) {
        vbus->irq = &s->plic_irq[irq_num];
        s->common.console_dev = virtio_console_init(vbus, p->console);
        vbus->addr += VIRTIO_SIZE;
        irq_num++;
        s->virtio_count++;
    }

    /* virtio net device */
    for(i = 0; i < p->eth_count; i++) {
        vbus->irq = &s->plic_irq[irq_num];
        virtio_net_init(vbus, p->tab_eth[i].net);
        s->common.net = p->tab_eth[i].net;
        vbus->addr += VIRTIO_SIZE;
        irq_num++;
        s->virtio_count++;
    }

    /* virtio block device */
    for(i = 0; i < p->drive_count; i++) {
        vbus->irq = &s->plic_irq[irq_num];
        blk_dev = virtio_block_init(vbus, p->tab_drive[i].block_dev);
        (void)blk_dev;
        vbus->addr += VIRTIO_SIZE;
        irq_num++;
        s->virtio_count++;
    }

    /* virtio filesystem */
    for (i = 0; i < p->fs_count; i++) {
        VIRTIODevice *fs_dev;
        vbus->irq = &s->plic_irq[irq_num];
        fs_dev = virtio_9p_init(vbus, p->tab_fs[i].fs_dev,
                                p->tab_fs[i].tag);
        (void)fs_dev;
        //        virtio_set_debug(fs_dev, VIRTIO_DEBUG_9P);
        vbus->addr += VIRTIO_SIZE;
        irq_num++;
        s->virtio_count++;
    }

    if (p->input_device) {
        if (!strcmp(p->input_device, "virtio")) {
            vbus->irq = &s->plic_irq[irq_num];
            s->keyboard_dev = virtio_input_init(vbus,
                                                VIRTIO_INPUT_TYPE_KEYBOARD);
            vbus->addr += VIRTIO_SIZE;
            irq_num++;
            s->virtio_count++;

            vbus->irq = &s->plic_irq[irq_num];
            s->mouse_dev = virtio_input_init(vbus,
                                             VIRTIO_INPUT_TYPE_TABLET);
            vbus->addr += VIRTIO_SIZE;
            irq_num++;
            s->virtio_count++;
        } else {
            vm_error("unsupported input device: %s\n", p->input_device);
            exit(1);
        }
    }

    int failure = 0;
    if (p->elf_image)
        failure = copy_kernel(s, p->elf_image, p->elf_image_size, p->cmdline, true);
    else if (!p->files[VM_FILE_BIOS].buf) {
        vm_error("No bios found");
        return NULL;
    } else
        failure = copy_kernel(s, p->files[VM_FILE_BIOS].buf,
                              p->files[VM_FILE_BIOS].len, p->cmdline, false);

    if (failure) {
        free(s);
        return NULL;
    }

    return (VirtMachine *)s;
}

void virt_machine_end(VirtMachine *s1)
{
    RISCVMachine *s = (RISCVMachine *)s1;

    if (s1->snapshot_save_name)
        virt_machine_serialize(s1, s1->snapshot_save_name);

    /* XXX: stop all */
    riscv_cpu_end(s->cpu_state);
    phys_mem_map_end(s->mem_map);
    free(s);
}

void virt_machine_serialize(VirtMachine *s1, const char *dump_name)
{
    RISCVMachine *m = (RISCVMachine *)s1;
    RISCVCPUState *s = m->cpu_state;

    fprintf(stderr, "plic: %x %x timecmp=%llx\n", m->plic_pending_irq, m->plic_served_irq, (unsigned long long)m->timecmp);

    riscv_cpu_serialize(s, m, dump_name);
}

void virt_machine_deserialize(VirtMachine *s1, const char *dump_name)
{
    RISCVMachine *m = (RISCVMachine *)s1;
    RISCVCPUState *s = m->cpu_state;

    riscv_cpu_deserialize(s, dump_name);
}

int virt_machine_get_sleep_duration(VirtMachine *s1, int ms_delay)
{
    RISCVMachine *m = (RISCVMachine *)s1;
    RISCVCPUState *s = m->cpu_state;
    int64_t ms_delay1;

    /* wait for an event: the only asynchronous event is the RTC timer */
    if (!(riscv_cpu_get_mip(s) & MIP_MTIP) && rtc_get_time(m)>0) {
        ms_delay1 = m->timecmp - rtc_get_time(m);
        if (ms_delay1 <= 0) {
            riscv_cpu_set_mip(s, MIP_MTIP);
            ms_delay = 0;
        } else {
            /* convert delay to ms */
            ms_delay1 = ms_delay1 / (RTC_FREQ / 1000);
            if (ms_delay1 < ms_delay)
                ms_delay = ms_delay1;
        }
    }

    if (!riscv_cpu_get_power_down(s))
        ms_delay = 0;

    return ms_delay;
}

BOOL virt_machine_interp(VirtMachine *s1, int max_exec_cycle)
{
    RISCVMachine *s = (RISCVMachine *)s1;
    riscv_cpu_interp(s->cpu_state, max_exec_cycle);

    return !riscv_terminated(s->cpu_state) &&
        s->htif_tohost == 0 && s1->maxinsns > 0;
}

void virt_machine_set_pc(VirtMachine *m, uint64_t pc)
{
    RISCVMachine *s = (RISCVMachine *)m;
    riscv_set_pc(s->cpu_state,pc);
}

void virt_machine_set_reg(VirtMachine *m, int rn, uint64_t val)
{
    RISCVMachine *s = (RISCVMachine *)m;
    riscv_set_reg(s->cpu_state,rn,val);
}

uint64_t virt_machine_get_pc(VirtMachine *m)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_get_pc(s->cpu_state);
}

uint64_t virt_machine_get_reg(VirtMachine *m, int rn)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_get_reg(s->cpu_state,rn);
}

uint64_t virt_machine_get_fpreg(VirtMachine *m, int rn)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_get_fpreg(s->cpu_state,rn);
}

void virt_machine_dump_regs(VirtMachine *m)
{
    RISCVMachine *s = (RISCVMachine *)m;
    riscv_dump_regs(s->cpu_state);
}

int virt_machine_read_insn(VirtMachine *m, uint32_t *pmem_addend, uint64_t addr)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_read_insn(s->cpu_state, pmem_addend, addr);
}

int virt_machine_read_u64(VirtMachine *m, uint64_t *data, uint64_t addr)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_read_u64(s->cpu_state,data,addr);
}

uint64_t virt_machine_read_htif_tohost(VirtMachine *m)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return s->htif_tohost;
}

void virt_machine_write_htif_tohost(VirtMachine *m, uint64_t tohost_value)
{
    RISCVMachine *s = (RISCVMachine *)m;
    s->htif_tohost = tohost_value;
}

uint64_t virt_machine_read_htif_tohost_addr(VirtMachine *m)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return s->htif_tohost_addr;
}

void virt_machine_repair_csr(VirtMachine *m, uint32_t reg_num, uint64_t csr_num, uint64_t csr_val)
{
    RISCVMachine *s = (RISCVMachine *)m;
    riscv_repair_csr(s->cpu_state,reg_num,csr_num,csr_val);
}

int virt_machine_repair_load(VirtMachine *m,uint32_t reg_num,uint64_t reg_val)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_repair_load(s->cpu_state,reg_num,reg_val,s->htif_tohost_addr,&s->htif_tohost,&s->htif_fromhost);
}

int virt_machine_repair_store(VirtMachine *m, uint32_t reg_num, uint32_t funct3)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_repair_store(s->cpu_state, reg_num, funct3);
}

const char *virt_machine_get_name(void)
{
    return "riscv64";
}

void vm_send_key_event(VirtMachine *s1, BOOL is_down,
                       uint16_t key_code)
{
    RISCVMachine *s = (RISCVMachine *)s1;
    if (s->keyboard_dev) {
        virtio_input_send_key_event(s->keyboard_dev, is_down, key_code);
    }
}

BOOL vm_mouse_is_absolute(VirtMachine *s)
{
    return TRUE;
}

void vm_send_mouse_event(VirtMachine *s1, int dx, int dy, int dz,
                        unsigned int buttons)
{
    RISCVMachine *s = (RISCVMachine *)s1;
    if (s->mouse_dev) {
        virtio_input_send_mouse_event(s->mouse_dev, dx, dy, dz, buttons);
    }
}

uint64_t virt_machine_get_instret(VirtMachine *m)
{
    RISCVMachine *s = (RISCVMachine *)m;
    return riscv_cpu_get_cycles(s->cpu_state);
}

int virt_machine_get_priv_level(VirtMachine *m)
{
    return riscv_get_priv_level(((RISCVMachine *)m)->cpu_state);
}

int virt_machine_get_most_recently_written_reg(VirtMachine *m, uint64_t *instret_ts)
{
    return riscv_get_most_recently_written_reg(((RISCVMachine *)m)->cpu_state, instret_ts);
}

int virt_machine_get_most_recently_written_fp_reg(VirtMachine *m, uint64_t *instret_ts)
{
    return riscv_get_most_recently_written_fp_reg(((RISCVMachine *)m)->cpu_state, instret_ts);
}
