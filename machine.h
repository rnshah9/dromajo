/*
 * VM definitions
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

#include <stdint.h>

#include "json.h"

typedef struct FBDevice FBDevice;

typedef void SimpleFBDrawFunc(FBDevice *fb_dev, void *opaque,
                              int x, int y, int w, int h);

struct FBDevice {
    /* the following is set by the device */
    int width;
    int height;
    int stride; /* current stride in bytes */
    uint8_t *fb_data; /* current pointer to the pixel data */
    int fb_size; /* frame buffer memory size (info only) */
    void *device_opaque;
    void (*refresh)(struct FBDevice *fb_dev,
                    SimpleFBDrawFunc *redraw_func, void *opaque);
};

#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>

#include "virtio.h"


#define MAX_DRIVE_DEVICE 4
#define MAX_FS_DEVICE 4
#define MAX_ETH_DEVICE 1

#define VM_CONFIG_VERSION 1

typedef enum {
    VM_FILE_BIOS,
    VM_FILE_VGA_BIOS,
    VM_FILE_KERNEL,

    VM_FILE_COUNT,
} VMFileTypeEnum;

typedef struct {
    char *filename;
    uint8_t *buf;
    int len;
} VMFileEntry;

typedef struct {
    char *device;
    char *filename;
    BlockDevice *block_dev;
} VMDriveEntry;

typedef struct {
    char *device;
    char *tag; /* 9p mount tag */
    char *filename;
    FSDevice *fs_dev;
} VMFSEntry;

typedef struct {
    char *driver;
    char *ifname;
    EthernetDevice *net;
} VMEthEntry;

typedef struct {
    char *cfg_filename;
    uint64_t ram_size;
    BOOL rtc_real_time;
    BOOL rtc_local_time;
    char *display_device; /* NULL means no display */
    int64_t width, height; /* graphic width & height */
    CharacterDevice *console;
    VMDriveEntry tab_drive[MAX_DRIVE_DEVICE];
    int drive_count;
    VMFSEntry tab_fs[MAX_FS_DEVICE];
    int fs_count;
    VMEthEntry tab_eth[MAX_ETH_DEVICE];
    int eth_count;
    uint64_t htif_base_addr;

    char *cmdline; /* bios or kernel command line */
    BOOL accel_enable; /* enable acceleration (KVM) */
    char *input_device; /* NULL means no input */

    /* kernel, bios and other auxiliary files */
    VMFileEntry files[VM_FILE_COUNT];

    /* validation terminate event */
    const char* validation_terminate_event;

    void  *elf_image;
    size_t elf_image_size;

    /* maximum increment of instructions to execute */
    uint64_t maxinsns;

    /* snaphot load file */
    const char *snapshot_load_name;
} VirtMachineParams;

typedef struct VirtMachine {
    /* network */
    EthernetDevice *net;
    /* console */
    VIRTIODevice *console_dev;
    CharacterDevice *console;
    /* graphics */
    FBDevice *fb_dev;

    const char *snapshot_load_name;
    const char *snapshot_save_name;
    const char *terminate_event;
    uint64_t    maxinsns;
    uint64_t    trace;
} VirtMachine;

void __attribute__((format(printf, 1, 2))) vm_error(const char *fmt, ...);
int vm_get_int(JSONValue obj, const char *name, int64_t *pval);

const char *virt_machine_get_name(void);
void virt_machine_set_defaults(VirtMachineParams *p);
void virt_machine_load_config_file(VirtMachineParams *p,
                                   const char *filename,
                                   void (*start_cb)(void *opaque),
                                   void *opaque);
void vm_add_cmdline(VirtMachineParams *p, const char *cmdline);
char *get_file_path(const char *base_filename, const char *filename);
void virt_machine_free_config(VirtMachineParams *p);
VirtMachine *virt_machine_init(const VirtMachineParams *p);
int virt_machine_get_sleep_duration(VirtMachine *s, int delay);
BOOL virt_machine_interp(VirtMachine *s, int max_exec_cycle);
BOOL vm_mouse_is_absolute(VirtMachine *s);
void vm_send_mouse_event(VirtMachine *s1, int dx, int dy, int dz,
                         unsigned int buttons);
void vm_send_key_event(VirtMachine *s1, BOOL is_down, uint16_t key_code);

/* gui */
void sdl_refresh(VirtMachine *m);
void sdl_init(int width, int height);

/* simplefb.c */
typedef struct SimpleFBState SimpleFBState;
SimpleFBState *simplefb_init(PhysMemoryMap *map, uint64_t phys_addr,
                             FBDevice *fb_dev, int width, int height);
void simplefb_refresh(FBDevice *fb_dev,
                      SimpleFBDrawFunc *redraw_func, void *opaque,
                      PhysMemoryRange *mem_range,
                      int fb_page_count);

/* vga.c */
typedef struct VGAState VGAState;
VGAState *pci_vga_init(PCIBus *bus, FBDevice *fb_dev,
                       int width, int height,
                       const uint8_t *vga_rom_buf, int vga_rom_size);

/* block_net.c */
BlockDevice *block_device_init_http(const char *url,
                                    int max_cache_size_kb,
                                    void (*start_cb)(void *opaque),
                                    void *start_opaque);
#ifdef __cplusplus
extern "C" {
#endif
void virt_machine_end(VirtMachine *s);
VirtMachine *virt_machine_main(int argc, char **argv);
void         virt_machine_serialize(VirtMachine *m, const char *dump_name);
void         virt_machine_deserialize(VirtMachine *m, const char *dump_name);
BOOL         virt_machine_run(VirtMachine *m);
void         virt_machine_dump_regs(VirtMachine *m);
int          virt_machine_read_insn(VirtMachine *m, uint32_t *insn, uint64_t addr);
uint64_t     virt_machine_get_pc(VirtMachine *m);
void         virt_machine_set_pc(VirtMachine *m, uint64_t pc);
uint64_t     virt_machine_get_reg(VirtMachine *m, int rn);
uint64_t     virt_machine_get_fpreg(VirtMachine *m, int rn);
uint64_t     virt_machine_read_htif_tohost(VirtMachine *m);
uint64_t     virt_machine_get_instret(VirtMachine *m);
int          virt_machine_get_priv_level(VirtMachine *m);
int          virt_machine_get_most_recently_written_reg(VirtMachine *m, uint64_t *instret_ts);
int          virt_machine_get_most_recently_written_fp_reg(VirtMachine *m, uint64_t *instret_ts);
#ifdef __cplusplus
} // extern C
#endif

#endif
