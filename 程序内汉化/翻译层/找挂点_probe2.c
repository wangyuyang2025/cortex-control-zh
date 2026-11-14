// 小样2：在 malloc/calloc 时看调用栈上有没有指向目标文字的指针，找出「拿着文字去排版」的函数
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#define INTERPOSE(n,o) __attribute__((used)) static struct{const void*a;const void*b;} _i_##o __attribute__((section("__DATA,__interpose")))={(const void*)n,(const void*)o};
static const char *T[] = {"Brit 2203", "CPU ", "PLUGINS", "OVERDRIVE", "\xe9\x80\x9f\xe5\xba\xa6" /*速度*/, "Bogna", 0};
static int fd = -1; static intptr_t slide; static __thread int busy; static volatile int on;
static uint64_t seen[4096];
extern kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t*);
static uintptr_t pc_page[2048]; static char pc_ok[2048];
static int rd(uintptr_t a, void *buf, size_t n) {
    uintptr_t pg = a >> 14; unsigned ix = (pg * 2654435761u) & 2047;
    if (pc_page[ix] == pg && ((a + n - 1) >> 14) == pg) { if (!pc_ok[ix]) return 0; memcpy(buf, (void*)a, n); return 1; }
    char probe; mach_vm_size_t g = 0; int ok = mach_vm_read_overwrite(mach_task_self(), pg << 14, 1, (mach_vm_address_t)&probe, &g) == KERN_SUCCESS;
    pc_ok[ix] = ok; pc_page[ix] = pg; if (!ok) return 0;
 mach_vm_size_t got = 0; return mach_vm_read_overwrite(mach_task_self(), a, n, (mach_vm_address_t)buf, &got) == KERN_SUCCESS && got == n; }
static void scan(void) {
    if (busy || !on) return; busy = 1;
    uintptr_t *fp = __builtin_frame_address(0); pthread_t me = pthread_self();
    uintptr_t top = (uintptr_t)pthread_get_stackaddr_np(me), bot = top - pthread_get_stacksize_np(me);
    uintptr_t rets[40], fps[40]; int n = 0;
    while (n < 40 && (uintptr_t)fp > bot && (uintptr_t)fp < top - 16) { fps[n] = (uintptr_t)fp; rets[n] = fp[1]; n++; uintptr_t *nx = (uintptr_t*)fp[0]; if (nx <= fp) break; fp = nx; }
    for (int i = 1; i < n; i++) {
        uintptr_t lo = fps[i-1] + 16, hi = fps[i]; if (hi - lo > 4096) lo = hi - 4096;
        for (uintptr_t a = lo; a < hi; a += 8) {
            uintptr_t v = *(uintptr_t*)a;
            if (v < 0x100000000ULL || v > 0x7fffffffffffULL || (v & 7)) continue;
            if (v > bot && v < top) continue;
            char b[12];
            if (!rd(v, b, 12)) continue;
            for (int t = 0; T[t]; t++) if (!memcmp(b, T[t], strlen(T[t]))) {
                uint64_t h = t * 1315423911u + i; for (int k = 0; k <= i + 2 && k < n; k++) h = h * 1099511628211ULL ^ rets[k];
                unsigned ix = h & 4095; if (seen[ix] == h) goto next; seen[ix] = h;
                char o[1400]; int p = snprintf(o, sizeof o, "HIT %s frame=%d slot_off=%ld :", T[t], i, (long)(fps[i] - a));
                for (int k = 0; k <= i + 3 && k < n; k++) p += snprintf(o + p, sizeof o - p, " %lx", (unsigned long)(rets[k] - slide));
                o[p++] = '\n'; write(fd, o, p); goto next;
            }
        }
    next:;
    }
    busy = 0;
}
static void *my_malloc(size_t n) { void *p = malloc(n); scan(); return p; }
INTERPOSE(my_malloc, malloc)
static void *my_calloc(size_t a, size_t b) { void *p = calloc(a, b); scan(); return p; }
INTERPOSE(my_calloc, calloc)
static void *starter(void *x) { for(;;){ usleep(200000); int e = access("/tmp/cc_probe_on", F_OK) == 0; if (e != on) { memset(pc_page,0,sizeof pc_page); on = e; } } return 0; }
__attribute__((constructor)) static void init(void) {
    const char *p = getenv("CC_PROBE_LOG"); fd = open(p ? p : "/tmp/cc_probe2.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    slide = _dyld_get_image_vmaddr_slide(0); pthread_t t; pthread_create(&t, 0, starter, 0);
}
