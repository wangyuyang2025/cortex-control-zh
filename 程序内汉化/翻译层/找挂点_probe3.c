// 小样3（轻量版）：后台线程先在堆里找到目标文字的地址；主线程 malloc 时只拿栈上的值和这几个地址比，不做系统调用
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#define INTERPOSE(n,o) __attribute__((used)) static struct{const void*a;const void*b;} _i_##o __attribute__((section("__DATA,__interpose")))={(const void*)n,(const void*)o};
static const char *T[] = {"Brit Plexi100 Normal", "US TWN Vibrato", "Brit 2203", "Rols Jazz CH120", 0};
static int fd = -1; static uintptr_t mainbase; static __thread int busy; static volatile int on;
#define NA 48
static volatile uintptr_t addrs[NA]; static volatile int na;
static uint64_t seen[4096];
static void scan(void) {
    if (busy || !on || !na || !pthread_main_np()) return; busy = 1;
    uintptr_t *fp = __builtin_frame_address(0); pthread_t me = pthread_self();
    uintptr_t top = (uintptr_t)pthread_get_stackaddr_np(me), bot = top - pthread_get_stacksize_np(me);
    uintptr_t rets[16], fps[16]; int n = 0;
    while (n < 16 && (uintptr_t)fp > bot && (uintptr_t)fp < top - 16) { fps[n] = (uintptr_t)fp; rets[n] = fp[1]; n++; uintptr_t *nx = (uintptr_t*)fp[0]; if (nx <= fp) break; fp = nx; }
    int cnt = na;
    for (int i = 1; i < n; i++) {
        uintptr_t lo = fps[i-1] + 16, hi = fps[i]; if (hi - lo > 2048) lo = hi - 2048;
        for (uintptr_t a = lo; a < hi; a += 8) {
            uintptr_t v = *(uintptr_t*)a; int hit = 0;
            for (int k = 0; k < cnt; k++) if (addrs[k] == v) { hit = 1; break; }
            if (!hit) continue;
            uint64_t h = i; for (int k = 0; k < n && k <= i + 3; k++) h = h * 1099511628211ULL ^ rets[k];
            unsigned ix = h & 4095; if (seen[ix] == h) break; seen[ix] = h;
            char o[900]; int p = snprintf(o, sizeof o, "HIT frame=%d off=%ld :", i, (long)(fps[i] - a));
            for (int k = 0; k < n && k <= i + 3; k++) { uintptr_t r = rets[k]; p += (r >= mainbase && r < mainbase + 0x2d00000) ? snprintf(o + p, sizeof o - p, " %lx", (unsigned long)(r - mainbase + 0x100000000UL)) : snprintf(o + p, sizeof o - p, " -"); }
            o[p++] = '\n'; write(fd, o, p); break;
        }
    }
    busy = 0;
}
static void *my_malloc(size_t n) { void *p = malloc(n); scan(); return p; }
INTERPOSE(my_malloc, malloc)
static void *my_calloc(size_t a, size_t b) { void *p = calloc(a, b); scan(); return p; }
INTERPOSE(my_calloc, calloc)
static void *finder(void *x) {
    busy = 1; char *buf = malloc(1 << 20);
    for (;;) {
        usleep(300000); int e = access("/tmp/cc_probe_on", F_OK) == 0; if (!e) { on = 0; continue; }
        uintptr_t found[NA]; int nf = 0; mach_vm_address_t a = 0x100000000ULL;
        for (;;) { mach_vm_size_t sz = 0; vm_region_basic_info_data_64_t info; mach_msg_type_number_t c = VM_REGION_BASIC_INFO_COUNT_64; mach_port_t obj;
            if (mach_vm_region(mach_task_self(), &a, &sz, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &c, &obj) != KERN_SUCCESS) break;
            if ((info.protection & (VM_PROT_READ | VM_PROT_WRITE)) == (VM_PROT_READ | VM_PROT_WRITE) && sz <= (256u << 20) && a < 0x700000000000ULL)
                for (mach_vm_address_t q = a; q < a + sz && nf < NA; q += (1 << 20) - 256) { mach_vm_size_t want = a + sz - q < (1 << 20) ? a + sz - q : (1 << 20), got = 0;
                    if (mach_vm_read_overwrite(mach_task_self(), q, want, (mach_vm_address_t)buf, &got) != KERN_SUCCESS) break;
                    for (int t = 0; T[t]; t++) { size_t tl = strlen(T[t]) + 1; char *s = buf, *m;
                        while (nf < NA && (m = memmem(s, buf + got - s, T[t], tl))) { uintptr_t ad = q + (m - buf); if (!(ad & 7) && (uintptr_t)m != (uintptr_t)T[t]) found[nf++] = ad; s = m + 1; } } }
            a += sz; }
        for (int i = 0; i < nf; i++) addrs[i] = found[i]; na = nf; on = 1;
        char o[64]; int p = snprintf(o, sizeof o, "#addrs %d\n", nf); write(fd, o, p);
    }
    return 0;
}
__attribute__((constructor)) static void init(void) {
    const char *p = getenv("CC_PROBE_LOG"); fd = open(p ? p : "/tmp/cc_probe3.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    for (uint32_t i = 0; i < _dyld_image_count(); i++) { const struct mach_header *h = _dyld_get_image_header(i); if (h->filetype == MH_EXECUTE) { mainbase = (uintptr_t)h; break; } }
    pthread_t t; pthread_create(&t, 0, finder, 0);
}
