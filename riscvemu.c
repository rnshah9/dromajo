/*
 * RISCV emulator
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
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifndef __APPLE__
#include <linux/if_tun.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include <err.h>

#include "cutils.h"
#include "iomem.h"
#include "virtio.h"
//#include "machine.h"
#ifdef CONFIG_FS_NET
#include "fs_utils.h"
#include "fs_wget.h"
#endif
#ifdef CONFIG_CPU_RISCV
#include "riscv_cpu.h"
#include "validation_events.h"
#endif
#ifdef CONFIG_SLIRP
#include "slirp/libslirp.h"
#endif

typedef struct {
    int stdin_fd;
    int console_esc_state;
    BOOL resize_pending;
} STDIODevice;

static struct termios oldtty;
static int old_fd0_flags;
static STDIODevice *global_stdio_device;

static void term_exit(void)
{
    tcsetattr (0, TCSANOW, &oldtty);
    fcntl(0, F_SETFL, old_fd0_flags);
}

static void term_init(BOOL allow_ctrlc)
{
    struct termios tty;

    memset(&tty, 0, sizeof(tty));
    tcgetattr (0, &tty);
    oldtty = tty;
    old_fd0_flags = fcntl(0, F_GETFL);

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                          |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
    if (!allow_ctrlc)
        tty.c_lflag &= ~ISIG;
    tty.c_cflag &= ~(CSIZE|PARENB);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr (0, TCSANOW, &tty);

    atexit(term_exit);
}

static void console_write(void *opaque, const uint8_t *buf, int len)
{
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

static int console_read(void *opaque, uint8_t *buf, int len)
{
    STDIODevice *s = opaque;

    if (len <= 0)
        return 0;

    int ret = read(s->stdin_fd, buf, len);
    if (ret <= 0)
        return 0;

    int j = 0;
    for (int i = 0; i < ret; i++) {
        uint8_t ch = buf[i];
        if (s->console_esc_state) {
            s->console_esc_state = 0;
            switch (ch) {
            case 'x':
                fprintf(stderr, "Terminated\n");
                exit(0);
            case 'h':
                fprintf(stderr, "\n"
                        "C-b h   print this help\n"
                        "C-b x   exit emulator\n"
                        "C-b C-b send C-b\n");
                break;
            case 1:
                goto output_char;
            default:
                break;
            }
        } else {
            if (ch == 2) { // Change to work with tmux
                s->console_esc_state = 1;
            } else {
            output_char:
                buf[j++] = ch;
            }
        }
    }

    return j;
}

static void term_resize_handler(int sig)
{
    if (global_stdio_device)
        global_stdio_device->resize_pending = TRUE;
}

static void console_get_size(STDIODevice *s, int *pw, int *ph)
{
    struct winsize ws;
    int width, height;
    /* default values */
    width = 80;
    height = 25;
    if (ioctl(s->stdin_fd, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_col >= 4 && ws.ws_row >= 4) {
        width = ws.ws_col;
        height = ws.ws_row;
    }
    *pw = width;
    *ph = height;
}

CharacterDevice *console_init(BOOL allow_ctrlc)
{
    CharacterDevice *dev;
    STDIODevice *s;
    struct sigaction sig;

    term_init(allow_ctrlc);

    dev = mallocz(sizeof(*dev));
    s = mallocz(sizeof(*s));
    s->stdin_fd = 0;
    /* Note: the glibc does not properly tests the return value of
       write() in printf, so some messages on stdout may be lost */
    fcntl(s->stdin_fd, F_SETFL, O_NONBLOCK);

    s->resize_pending = TRUE;
    global_stdio_device = s;

    /* use a signal to get the host terminal resize events */
    sig.sa_handler = term_resize_handler;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = 0;
    sigaction(SIGWINCH, &sig, NULL);

    dev->opaque = s;
    dev->write_data = console_write;
    dev->read_data = console_read;
    return dev;
}

typedef enum {
    BF_MODE_RO,
    BF_MODE_RW,
    BF_MODE_SNAPSHOT,
} BlockDeviceModeEnum;

#define SECTOR_SIZE 512

typedef struct BlockDeviceFile {
    FILE *f;
    int64_t nb_sectors;
    BlockDeviceModeEnum mode;
    uint8_t **sector_table;
} BlockDeviceFile;

static int64_t bf_get_sector_count(BlockDevice *bs)
{
    BlockDeviceFile *bf = bs->opaque;
    return bf->nb_sectors;
}

//#define DUMP_BLOCK_READ

static int bf_read_async(BlockDevice *bs,
                         uint64_t sector_num, uint8_t *buf, int n,
                         BlockDeviceCompletionFunc *cb, void *opaque)
{
    BlockDeviceFile *bf = bs->opaque;
    //    fprintf(stderr,"bf_read_async: sector_num=%" PRId64 " n=%d\n", sector_num, n);
#ifdef DUMP_BLOCK_READ
    {
        static FILE *f;
        if (!f)
            f = fopen("/tmp/read_sect.txt", "wb");
        fprintf(f, "%" PRId64 " %d\n", sector_num, n);
    }
#endif
    if (!bf->f)
        return -1;
    if (bf->mode == BF_MODE_SNAPSHOT) {
        int i;
        for(i = 0; i < n; i++) {
            if (!bf->sector_table[sector_num]) {
                fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
                size_t got = fread(buf, 1, SECTOR_SIZE, bf->f);
                assert(got == SECTOR_SIZE);
            } else {
                memcpy(buf, bf->sector_table[sector_num], SECTOR_SIZE);
            }
            sector_num++;
            buf += SECTOR_SIZE;
        }
    } else {
        fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
        size_t got = fread(buf, 1, n * SECTOR_SIZE, bf->f);
        assert(got == n * SECTOR_SIZE);
    }
    /* synchronous read */
    return 0;
}

static int bf_write_async(BlockDevice *bs,
                          uint64_t sector_num, const uint8_t *buf, int n,
                          BlockDeviceCompletionFunc *cb, void *opaque)
{
    BlockDeviceFile *bf = bs->opaque;
    int ret;

    switch(bf->mode) {
    case BF_MODE_RO:
        ret = -1; /* error */
        break;
    case BF_MODE_RW:
        fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
        fwrite(buf, 1, n * SECTOR_SIZE, bf->f);
        ret = 0;
        break;
    case BF_MODE_SNAPSHOT:
        {
            int i;
            if ((sector_num + n) > bf->nb_sectors)
                return -1;
            for(i = 0; i < n; i++) {
                if (!bf->sector_table[sector_num]) {
                    bf->sector_table[sector_num] = malloc(SECTOR_SIZE);
                }
                memcpy(bf->sector_table[sector_num], buf, SECTOR_SIZE);
                sector_num++;
                buf += SECTOR_SIZE;
            }
            ret = 0;
        }
        break;
    default:
        abort();
    }

    return ret;
}

static BlockDevice *block_device_init(const char *filename,
                                      BlockDeviceModeEnum mode)
{
    BlockDevice *bs;
    BlockDeviceFile *bf;
    int64_t file_size;
    FILE *f;
    const char *mode_str;

    if (mode == BF_MODE_RW) {
        mode_str = "r+b";
    } else {
        mode_str = "rb";
    }

    f = fopen(filename, mode_str);
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    file_size = ftello(f);

    bs = mallocz(sizeof(*bs));
    bf = mallocz(sizeof(*bf));

    bf->mode = mode;
    bf->nb_sectors = file_size / 512;
    bf->f = f;

    if (mode == BF_MODE_SNAPSHOT) {
        bf->sector_table = mallocz(sizeof(bf->sector_table[0]) *
                                   bf->nb_sectors);
    }

    bs->opaque = bf;
    bs->get_sector_count = bf_get_sector_count;
    bs->read_async = bf_read_async;
    bs->write_async = bf_write_async;
    return bs;
}

#define MAX_EXEC_CYCLE 1
#define MAX_SLEEP_TIME 10 /* in ms */

#if !defined(__APPLE__)
typedef struct {
    int fd;
    BOOL select_filled;
} TunState;

static void tun_write_packet(EthernetDevice *net,
                             const uint8_t *buf, int len)
{
    TunState *s = net->opaque;
    ssize_t got = write(s->fd, buf, len);
    assert(got == len);
}

static void tun_select_fill(EthernetDevice *net, int *pfd_max,
                            fd_set *rfds, fd_set *wfds, fd_set *efds,
                            int *pdelay)
{
    TunState *s = net->opaque;
    int net_fd = s->fd;

    s->select_filled = net->device_can_write_packet(net);
    if (s->select_filled) {
        FD_SET(net_fd, rfds);
        *pfd_max = max_int(*pfd_max, net_fd);
    }
}

static void tun_select_poll(EthernetDevice *net,
                            fd_set *rfds, fd_set *wfds, fd_set *efds,
                            int select_ret)
{
    TunState *s = net->opaque;
    int net_fd = s->fd;
    uint8_t buf[2048];
    int ret;

    if (select_ret <= 0)
        return;
    if (s->select_filled && FD_ISSET(net_fd, rfds)) {
        ret = read(net_fd, buf, sizeof(buf));
        if (ret > 0)
            net->device_write_packet(net, buf, ret);
    }

}

/* configure with:
# bridge configuration (connect tap0 to bridge interface br0)
   ip link add br0 type bridge
   ip tuntap add dev tap0 mode tap [user x] [group x]
   ip link set tap0 master br0
   ip link set dev br0 up
   ip link set dev tap0 up

# NAT configuration (eth1 is the interface connected to internet)
   ifconfig br0 192.168.3.1
   echo 1 > /proc/sys/net/ipv4/ip_forward
   iptables -D FORWARD 1
   iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE

   In the VM:
   ifconfig eth0 192.168.3.2
   route add -net 0.0.0.0 netmask 0.0.0.0 gw 192.168.3.1
*/
static EthernetDevice *tun_open(const char *ifname)
{
    struct ifreq ifr;
    int fd, ret;
    EthernetDevice *net;
    TunState *s;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: could not open /dev/net/tun\n");
        return NULL;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    pstrcpy(ifr.ifr_name, sizeof(ifr.ifr_name), ifname);
    ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (ret != 0) {
        fprintf(stderr, "Error: could not configure /dev/net/tun\n");
        close(fd);
        return NULL;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);

    net = mallocz(sizeof(*net));
    net->mac_addr[0] = 0x02;
    net->mac_addr[1] = 0x00;
    net->mac_addr[2] = 0x00;
    net->mac_addr[3] = 0x00;
    net->mac_addr[4] = 0x00;
    net->mac_addr[5] = 0x01;
    s = mallocz(sizeof(*s));
    s->fd = fd;
    net->opaque = s;
    net->write_packet = tun_write_packet;
    net->select_fill = tun_select_fill;
    net->select_poll = tun_select_poll;
    return net;
}

#endif /* !__APPLE__*/

#ifdef CONFIG_SLIRP

/*******************************************************/
/* slirp */

static Slirp *slirp_state;

static void slirp_write_packet(EthernetDevice *net,
                               const uint8_t *buf, int len)
{
    Slirp *slirp_state = net->opaque;
    slirp_input(slirp_state, buf, len);
}

int slirp_can_output(void *opaque)
{
    EthernetDevice *net = opaque;
    return net->device_can_write_packet(net);
}

void slirp_output(void *opaque, const uint8_t *pkt, int pkt_len)
{
    EthernetDevice *net = opaque;
    return net->device_write_packet(net, pkt, pkt_len);
}

static void slirp_select_fill1(EthernetDevice *net, int *pfd_max,
                               fd_set *rfds, fd_set *wfds, fd_set *efds,
                               int *pdelay)
{
    Slirp *slirp_state = net->opaque;
    slirp_select_fill(slirp_state, pfd_max, rfds, wfds, efds);
}

static void slirp_select_poll1(EthernetDevice *net,
                               fd_set *rfds, fd_set *wfds, fd_set *efds,
                               int select_ret)
{
    Slirp *slirp_state = net->opaque;
    slirp_select_poll(slirp_state, rfds, wfds, efds, (select_ret <= 0));
}

static EthernetDevice *slirp_open(void)
{
    EthernetDevice *net;
    struct in_addr net_addr  = { .s_addr = htonl(0x0a000200) }; /* 10.0.2.0 */
    struct in_addr mask = { .s_addr = htonl(0xffffff00) }; /* 255.255.255.0 */
    struct in_addr host = { .s_addr = htonl(0x0a000202) }; /* 10.0.2.2 */
    struct in_addr dhcp = { .s_addr = htonl(0x0a00020f) }; /* 10.0.2.15 */
    struct in_addr dns  = { .s_addr = htonl(0x0a000203) }; /* 10.0.2.3 */
    const char *bootfile = NULL;
    const char *vhostname = NULL;
    int restricted = 0;

    if (slirp_state) {
        fprintf(stderr, "Only a single slirp instance is allowed\n");
        return NULL;
    }
    net = mallocz(sizeof(*net));

    slirp_state = slirp_init(restricted, net_addr, mask, host, vhostname,
                             "", bootfile, dhcp, dns, net);

    net->mac_addr[0] = 0x02;
    net->mac_addr[1] = 0x00;
    net->mac_addr[2] = 0x00;
    net->mac_addr[3] = 0x00;
    net->mac_addr[4] = 0x00;
    net->mac_addr[5] = 0x01;
    net->opaque = slirp_state;
    net->write_packet = slirp_write_packet;
    net->select_fill = slirp_select_fill1;
    net->select_poll = slirp_select_poll1;

    return net;
}

#endif /* CONFIG_SLIRP */

BOOL virt_machine_run(VirtMachine *m)
{
    fd_set rfds, wfds, efds;
    int fd_max, ret, delay;
    struct timeval tv;
    int stdin_fd;

    delay = virt_machine_get_sleep_duration(m, MAX_SLEEP_TIME);

    /* wait for an event */
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    fd_max = -1;

    if (m->console_dev && virtio_console_can_write_data(m->console_dev)) {
        STDIODevice *s = m->console->opaque;
        stdin_fd = s->stdin_fd;
        FD_SET(stdin_fd, &rfds);
        fd_max = stdin_fd;

        if (s->resize_pending) {
            int width, height;
            console_get_size(s, &width, &height);
            virtio_console_resize_event(m->console_dev, width, height);
            s->resize_pending = FALSE;
        }
    }

    if (m->net) {
        m->net->select_fill(m->net, &fd_max, &rfds, &wfds, &efds, &delay);
    }
#ifdef CONFIG_FS_NET
    fs_net_set_fdset(&fd_max, &rfds, &wfds, &efds, &delay);
#endif
    tv.tv_sec = delay / 1000;
    tv.tv_usec = delay % 1000;
    ret = select(fd_max + 1, &rfds, &wfds, &efds, &tv);
    if (m->net) {
        m->net->select_poll(m->net, &rfds, &wfds, &efds, ret);
    }
    if (ret > 0) {
        if (m->console_dev && FD_ISSET(stdin_fd, &rfds)) {
            uint8_t buf[128];
            int ret, len;
            len = virtio_console_get_write_len(m->console_dev);
            len = min_int(len, sizeof(buf));
            ret = m->console->read_data(m->console->opaque, buf, len);
            if (ret > 0) {
                virtio_console_write_data(m->console_dev, buf, ret);
            }
        }
    }

    return virt_machine_interp(m, MAX_EXEC_CYCLE);
}

void help(void)
{
    fprintf(stderr,"riscvemu version " CONFIG_VERSION ", Copyright (c) 2016-2017 Fabrice Bellard\n"
           "                             Copyright (c) 2018 Esperanto Technologies\n"
           "usage: riscvemu [options] config_file\n"
           "options are:\n"
           "-m ram_size       set the RAM size in MB\n"
           "-rw               allow write access to the disk image (default=snapshot)\n"
           "-ctrlc            the C-c key stops the emulator instead of being sent to the\n"
           "                  emulated software\n"
           "-append cmdline   append cmdline to the kernel command line\n"
           "\n"
           "Console keys:\n"
           "Press C-b x to exit the emulator, C-b h to get some help.\n");
    exit(1);
}

void launch_alternate_executable(char **argv)
{
    char filename[1024];
    char new_exename[64];
    const char *p, *exename;
    int len;

    snprintf(new_exename, sizeof(new_exename), "riscvemu64");
    exename = argv[0];
    p = strrchr(exename, '/');
    if (p) {
        len = p - exename + 1;
    } else {
        len = 0;
    }
    if (len + strlen(new_exename) > sizeof(filename) - 1) {
        fprintf(stderr, "%s: filename too long\n", exename);
        exit(1);
    }
    memcpy(filename, exename, len);
    filename[len] = '\0';
    strcat(filename, new_exename);
    argv[0] = filename;

    if (execvp(argv[0], argv) < 0) {
        perror(argv[0]);
        exit(1);
    }
}

#ifdef CONFIG_FS_NET
static BOOL net_completed;

static void net_start_cb(void *arg)
{
    net_completed = TRUE;
}

static BOOL net_poll_cb(void *arg)
{
    return net_completed;
}

#endif

// extern int optind;  XXX Do I need to declare this?

static void usage(const char *prog, const char *msg)
{
    fprintf(stderr,
            "error: %s\n"
            "usage: %s [--load snapshot_name] [--save snapshot_name] [--maxinsns N] "
            "[--memory_size MB] config\n"
            "       --load resumes a previously saved snapshot\n"
            "       --save saves a snapshot upon exit\n"
            "       --maxinsns terminates execution after a number of instructions\n"
            "       --terminate-event name of the validate event to terminate execution\n"
            "       --trace start trace dump after a number of instructions. Trace disabled by default\n"
            "       --memory_size sets the memory size in MiB (default 256 MiB)\n",
            msg, prog);

    exit(EXIT_FAILURE);
}

VirtMachine *virt_machine_main(int argc, char **argv)
{
    const char *prog               = argv[0];
    const char *snapshot_load_name = 0;
    const char *snapshot_save_name = 0;
    const char *path               = NULL;
    const char *cmdline            = NULL;
    const char *terminate_event    = NULL;
    uint64_t    maxinsns           = 0;
    uint64_t    trace              = UINT64_MAX;
    long        memory_size_override   = 0;

    optind = 0;

    for (;;) {
        int option_index = 0;
        static struct option long_options[] = {
            {"load",    required_argument, 0,  'l' },
            {"save",    required_argument, 0,  's' },
            {"maxinsns",required_argument, 0,  'm' },
            {"terminate-event", required_argument, 0, 'e'},
            {"trace   ",required_argument, 0,  't' },
            {"memory_size", required_argument, 0,  'M' },
            {0,         0,                 0,  0 }
        };

        int c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'l':
            if (snapshot_load_name)
                usage(prog, "already had a snapshot to load");
            snapshot_load_name = strdup(optarg);
            break;

        case 's':
            if (snapshot_save_name)
                usage(prog, "already had a snapshot to save");
            snapshot_save_name = strdup(optarg);
            break;

        case 'm':
            if (maxinsns)
                usage(prog, "already had a max instructions");
            maxinsns = (uint64_t) atoll(optarg);
            break;

        case 'e':
            if (terminate_event) {
                usage(prog, "already had a terminate event");
            }
            for (int i = 0; i < countof(validation_events); ++i) {
                if (validation_events[i].terminate
                    && strcmp(validation_events[i].name, optarg) != 0)
                {
                    fprintf(stderr, "Unknown terminate event\n");
                    fprintf(stderr, "Valid termination events: \n");
                    for (int j = 0; j < countof(validation_events); ++j) {
                        if (validation_events[j].terminate) {
                            fprintf(stderr, "\t%s\n",
                                    validation_events[j].name);
                        }
                    }
                    usage(prog, "unknown terminate event");
                }
            }
            terminate_event = strdup(optarg);
            break;

        case 't':
            if (trace != UINT64_MAX)
                usage(prog, "already had a trace set");
            trace = (uint64_t) atoll(optarg);
            break;

        case 'M':
            memory_size_override = atoi(optarg);
            break;

        default:
            usage(prog, "I'm not having this argument");
        }
    }

    if (optind >= argc)
        usage(prog, "missing config file");
    else
        path = argv[optind++];

    if (optind < argc)
        usage(prog, "too many arguments");

    assert(path);
    VirtMachine *s;
    int ram_size = -1, accel_enable = -1;
    BlockDeviceModeEnum drive_mode = BF_MODE_SNAPSHOT;
    VirtMachineParams p_s, *p = &p_s;

    virt_machine_set_defaults(p);
#ifdef CONFIG_FS_NET
    fs_wget_init();
#endif
    virt_machine_load_config_file(p, path, NULL, NULL);
    if (memory_size_override)
        p->ram_size = memory_size_override << 20;

#ifdef CONFIG_FS_NET
    fs_net_event_loop(NULL, NULL);
#endif

    /* override some config parameters */

    if (ram_size > 0) {
        p->ram_size = ram_size << 20;
    }
    if (accel_enable != -1)
        p->accel_enable = accel_enable;

    if (cmdline) {
        vm_add_cmdline(p, cmdline);
    }

    /* open the files & devices */
    for (int i = 0; i < p->drive_count; i++) {
        BlockDevice *drive;
        char *fname;
        fname = get_file_path(p->cfg_filename, p->tab_drive[i].filename);
#ifdef CONFIG_FS_NET
        if (is_url(fname)) {
            net_completed = FALSE;
            drive = block_device_init_http(fname, 128 * 1024,
                                           net_start_cb, NULL);
            /* wait until the drive is initialized */
            fs_net_event_loop(net_poll_cb, NULL);
        } else
#endif
        {
            drive = block_device_init(fname, drive_mode);
        }
        free(fname);
        p->tab_drive[i].block_dev = drive;
    }

    for (int i = 0; i < p->fs_count; i++) {
        FSDevice *fs;
        const char *path;
        path = p->tab_fs[i].filename;
#ifdef CONFIG_FS_NET
        if (is_url(path)) {
            fs = fs_net_init(path, NULL, NULL);
            if (!fs)
                exit(1);
            fs_net_event_loop(NULL, NULL);
        } else
#endif
        {
#if defined(__APPLE__)
            fprintf(stderr, "Filesystem access not supported yet\n");
            exit(1);
#else
            char *fname;
            fname = get_file_path(p->cfg_filename, path);
            fs = fs_disk_init(fname);
            if (!fs) {
                fprintf(stderr, "%s: must be a directory\n", fname);
                exit(1);
            }
            free(fname);
#endif
        }
        p->tab_fs[i].fs_dev = fs;
    }

    for (int i = 0; i < p->eth_count; i++) {
#ifdef CONFIG_SLIRP
        if (!strcmp(p->tab_eth[i].driver, "user")) {
            p->tab_eth[i].net = slirp_open();
            if (!p->tab_eth[i].net)
                exit(1);
        } else
#endif
#if !defined(__APPLE__)
        if (!strcmp(p->tab_eth[i].driver, "tap")) {
            p->tab_eth[i].net = tun_open(p->tab_eth[i].ifname);
            if (!p->tab_eth[i].net)
                exit(1);
        } else
#endif
        {
            fprintf(stderr, "Unsupported network driver '%s'\n",
                    p->tab_eth[i].driver);
            exit(1);
        }
    }

    p->console = console_init(TRUE);

    p->rtc_real_time = FALSE;
    p->validation_terminate_event = terminate_event;

    s = virt_machine_init(p);

    // Overwrite the value specified in the configuration file
    if (snapshot_load_name) {
        s->snapshot_load_name = snapshot_load_name;
    }
    s->snapshot_save_name = snapshot_save_name;
    s->trace              = trace;
    // Allow the command option argument to overwrite the value
    // specified in the configuration file
    if (maxinsns > 0) {
        s->maxinsns = maxinsns;
    }
    // If not value is specified in the configuration or the command line
    // then run indefinitely
    if (s->maxinsns == 0)
        s->maxinsns = UINT64_MAX;

    virt_machine_free_config(p);

    if (s->net)
        s->net->device_set_carrier(s->net, TRUE);

    if (s->snapshot_load_name)
        virt_machine_deserialize(s, s->snapshot_load_name);

    return s;
}
