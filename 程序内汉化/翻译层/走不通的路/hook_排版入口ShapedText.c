// Cortex Control 翻译层小样：在 JUCE 画文字的三个函数入口，把英文换成词典里的中文。只对 4.1.0 arm64 有效。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <libkern/OSCacheControl.h>
typedef struct { int32_t rc; int32_t pad; size_t alloc; char text[]; } Holder;       // juce::StringHolder
typedef struct Ent { struct Ent *next; char *key; char *slot; char pad; } Ent;                 // slot = &holder->text，等价于一个 juce::String
#define NB 8192
static Ent *tab[NB], *tab2[NB]; static int logfd = -1; static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static uint64_t seen[8192];
#define NCTX 64
static struct { uintptr_t addr; char tag; } ctx[NCTX]; static int nctx;      // 调用者地址 → 场合标记（词典里 #ctx 行）
static unsigned hs(const char *s) { unsigned h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)tolower((unsigned char)*s)) * 16777619u; return h; }
static void load(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return; char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *t = strchr(line, '\t'); if (!t) continue; *t++ = 0; t[strcspn(t, "\n")] = 0;
        if (!strcmp(line, "#ctx")) { char *u = strchr(t, '\t'); if (u && nctx < NCTX) { ctx[nctx].addr = strtoull(t, 0, 16); ctx[nctx].tag = u[1]; nctx++; } continue; }
        size_t n = strlen(t); Holder *h = calloc(1, sizeof(Holder) + n + 8); h->rc = 0x3fffffff; h->alloc = n + 1; memcpy(h->text, t, n);
        unsigned b = hs(line) % NB; int dupk = 0; for (Ent *x = tab[b]; x; x = x->next) if (!strcmp(x->key, line)) { dupk = 1; break; }
        if (dupk) continue;
        Ent *e = calloc(1, sizeof *e); e->key = strdup(line); e->slot = h->text; e->next = tab[b]; tab[b] = e;
    }
    fclose(f);
}
// 返回要交给原函数的 String*：命中就换成我们的，否则原样。lr/fp 用来认「是谁在画这个字」。
static intptr_t g_slide; static uintptr_t g_lo, g_hi;
static char ctx_of(uintptr_t lr, uintptr_t *fp, uintptr_t *chain) {
    uintptr_t c[5] = {lr, 0, 0, 0, 0}; pthread_t me = pthread_self();
    uintptr_t top = (uintptr_t)pthread_get_stackaddr_np(me), bot = top - pthread_get_stacksize_np(me);
    for (int i = 1; i < 5 && (uintptr_t)fp > bot && (uintptr_t)fp < top - 16; i++) { c[i] = fp[1]; uintptr_t *nx = (uintptr_t *)fp[0]; if (nx <= fp) break; fp = nx; }
    char tag = 0;
    for (int i = 0; i < 5; i++) { uintptr_t a = (c[i] >= g_lo && c[i] < g_hi) ? c[i] - g_slide : 0; if (chain) chain[i] = a;
        if (a && !tag) for (int k = 0; k < nctx; k++) if (ctx[k].addr == a) { tag = ctx[k].tag; break; } }
    return tag;
}
static Ent *find(const char *s, char tag) {
    char k[220]; const char *q = s; if (tag) { snprintf(k, sizeof k, "%s@%c", s, tag); q = k; }
    Ent *ci = 0;
    for (Ent *e = tab[hs(q) % NB]; e; e = e->next) { if (!strcmp(e->key, q)) return e; if (!ci && !strcasecmp(e->key, q)) ci = e; }   // 大小写一样的优先
    return ci;
}
void **cc_translate(void **str, uintptr_t lr, uintptr_t *fp) {
    if (!str || !*str) return str;
    const char *s = (const char *)*str; size_t n = strnlen(s, 200); if (n == 0 || n >= 200) return str;
    uintptr_t chain[5]; char tag = ctx_of(lr, fp, logfd >= 0 ? chain : 0);
    Ent *e = tag ? find(s, tag) : 0; if (!e) e = find(s, 0);
    if (logfd >= 0) { uint64_t h = 1469598103934665603ULL; for (const char *p = s; *p; p++) h = (h ^ (unsigned char)*p) * 1099511628211ULL;
        for (int i = 0; i < 5; i++) h = (h ^ chain[i]) * 1099511628211ULL;
        unsigned ix = h & 8191; if (seen[ix] != h) { seen[ix] = h; char o[400]; int k = snprintf(o, sizeof o, "%c\t%lx %lx %lx %lx %lx\t%s\n", e ? 'Y' : 'n', chain[0], chain[1], chain[2], chain[3], chain[4], s); write(logfd, o, k); } }
    if (!e) return str;
    if (e->slot[0] == 2) return str;                                       // 词典写了「这个场合不换」
    if (e->slot[0] != 1) return (void **)&e->slot;
    // 「中文 + 英文原文」：按原文大小写现拼，拼好存起来下次直接用
    unsigned b = (hs(s) * 31 + 7 + (unsigned char)tag) % NB; pthread_mutex_lock(&mu);
    for (Ent *c = tab2[b]; c; c = c->next) if (c->pad == tag && !strcmp(c->key, s)) { pthread_mutex_unlock(&mu); return (void **)&c->slot; }
    size_t zl = strlen(e->slot + 1); Holder *h = calloc(1, sizeof(Holder) + zl + n + 8); h->rc = 0x3fffffff; h->alloc = zl + n + 2;
    memcpy(h->text, e->slot + 1, zl); h->text[zl] = ' '; memcpy(h->text + zl + 1, s, n);
    Ent *c = calloc(1, sizeof *c); c->key = strdup(s); c->slot = h->text; c->pad = tag; c->next = tab2[b]; tab2[b] = c;
    pthread_mutex_unlock(&mu); return (void **)&c->slot;
}
// 排版入口（ShapedText 构造）用：只查「原文@S」。这个函数会把传进来的 String 搬走，所以每次给它一个临时格子。
void **cc_translate_s(void **str) {
    if (!str || !*str) return str;
    const char *s = (const char *)*str; size_t n = strnlen(s, 200); if (n == 0 || n >= 200) return str;
    size_t cn = n; while (cn && (s[cn - 1] == '\n' || s[cn - 1] == '\r' || s[cn - 1] == ' ')) cn--;   // 顶栏那句末尾带换行
    if (!cn) return str;
    char core[204]; memcpy(core, s, cn); core[cn] = 0;
    const char *val = 0; size_t pre = 0;
    Ent *e = find(core, 'S');
    if (logfd >= 0) { uint64_t h = 77; for (const char *p = s; *p; p++) h = (h ^ (unsigned char)*p) * 1099511628211ULL; unsigned ix = h & 8191; if (seen[ix] != h) { seen[ix] = h; char o[300]; int k = snprintf(o, sizeof o, "S%c\t%s\n", e ? 'Y' : 'n', core); write(logfd, o, k); } }
    if (e) { if (e->slot[0] == 1 || e->slot[0] == 2) return str; val = e->slot; }
    else {                                                                 // 「1B 预设名」连成一句：去掉编号再查
        size_t d = 0; while (d < 2 && core[d] >= '0' && core[d] <= '9') d++;
        if (d && core[d] >= 'A' && core[d] <= 'H' && core[d + 1] == ' ' && core[d + 2]) {
            Ent *r = find(core + d + 2, 'S'); if (r && r->slot[0] != 1 && r->slot[0] != 2) { val = r->slot; pre = d + 2; }
        }
    }
    if (!val) return str;
    static __thread char *tmp[8]; static __thread unsigned ti;
    char *out;
    if (!pre && cn == n) out = (char *)val;
    else {                                                                 // 要拼：编号 + 真名 + 原来的结尾；拼好存起来
        unsigned b = (hs(s) * 31 + 7 + 'S') % NB; Ent *c; pthread_mutex_lock(&mu);
        for (c = tab2[b]; c; c = c->next) if (c->pad == 'S' && !strcmp(c->key, s)) break;
        if (!c) { size_t vl = strlen(val), sl = n - cn; Holder *h = calloc(1, sizeof(Holder) + pre + vl + sl + 8); h->rc = 0x3fffffff; h->alloc = pre + vl + sl + 1;
            memcpy(h->text, core, pre); memcpy(h->text + pre, val, vl); memcpy(h->text + pre + vl, s + cn, sl);
            c = calloc(1, sizeof *c); c->key = strdup(s); c->slot = h->text; c->pad = 'S'; c->next = tab2[b]; tab2[b] = c; }
        pthread_mutex_unlock(&mu); out = c->slot;
    }
    Holder *h = (Holder *)(out - sizeof(Holder)); __sync_fetch_and_add(&h->rc, 1);   // 对方用完会减一次
    char **t = &tmp[ti++ & 7]; *t = out; return (void **)t;
}
uintptr_t cc_cont[4];
extern void cc_hook0(void), cc_hook1(void), cc_hook2(void), cc_hook3(void);
__asm__(
".text\n.align 4\n"
"_cc_common:\n"                       // 保存参数寄存器，调 cc_translate(x1)，换掉 x1
"  sub sp, sp, #0xa0\n  stp x0, x1, [sp]\n  stp x2, x3, [sp,#0x10]\n  stp x4, x5, [sp,#0x20]\n  stp x6, x7, [sp,#0x30]\n  stp x8, x30, [sp,#0x40]\n"
"  stp d0, d1, [sp,#0x50]\n  stp d2, d3, [sp,#0x60]\n  stp d4, d5, [sp,#0x70]\n  stp d6, d7, [sp,#0x80]\n  stp x29, x17, [sp,#0x90]\n"
"  mov x0, x1\n  mov x1, x30\n  mov x2, x29\n  bl _cc_translate\n  mov x1, x0\n"
"  ldr x0, [sp]\n  ldp x2, x3, [sp,#0x10]\n  ldp x4, x5, [sp,#0x20]\n  ldp x6, x7, [sp,#0x30]\n  ldp x8, x30, [sp,#0x40]\n"
"  ldp d0, d1, [sp,#0x50]\n  ldp d2, d3, [sp,#0x60]\n  ldp d4, d5, [sp,#0x70]\n  ldp d6, d7, [sp,#0x80]\n  ldp x29, x17, [sp,#0x90]\n  add sp, sp, #0xa0\n  br x17\n"
"_cc_common_s:\n"
"  sub sp, sp, #0xa0\n  stp x0, x1, [sp]\n  stp x2, x3, [sp,#0x10]\n  stp x4, x5, [sp,#0x20]\n  stp x6, x7, [sp,#0x30]\n  stp x8, x30, [sp,#0x40]\n"
"  stp d0, d1, [sp,#0x50]\n  stp d2, d3, [sp,#0x60]\n  stp d4, d5, [sp,#0x70]\n  stp d6, d7, [sp,#0x80]\n  stp x29, x17, [sp,#0x90]\n"
"  mov x0, x1\n  bl _cc_translate_s\n  mov x1, x0\n"
"  ldr x0, [sp]\n  ldp x2, x3, [sp,#0x10]\n  ldp x4, x5, [sp,#0x20]\n  ldp x6, x7, [sp,#0x30]\n  ldp x8, x30, [sp,#0x40]\n"
"  ldp d0, d1, [sp,#0x50]\n  ldp d2, d3, [sp,#0x60]\n  ldp d4, d5, [sp,#0x70]\n  ldp d6, d7, [sp,#0x80]\n  ldp x29, x17, [sp,#0x90]\n  add sp, sp, #0xa0\n  br x17\n"
".globl _cc_hook3\n_cc_hook3:\n  adr x17, 1f\n  b _cc_common_s\n1:\n"   // ShapedText::ShapedText(String, Options) 0x1011e4da4
"  stp x24, x23, [sp, #-0x40]!\n  stp x22, x21, [sp, #0x10]\n  stp x20, x19, [sp, #0x20]\n  stp x29, x30, [sp, #0x30]\n"
"  adrp x16, _cc_cont@PAGE\n  add x16, x16, _cc_cont@PAGEOFF\n  ldr x16, [x16, #24]\n  br x16\n"
".globl _cc_hook0\n_cc_hook0:\n  adr x17, 1f\n  b _cc_common\n1:\n"   // drawText(Rectangle<int>) 0x1011b42d0
"  sub sp, sp, #0x1b0\n  stp d9, d8, [sp, #0x140]\n  stp x28, x27, [sp, #0x150]\n  stp x26, x25, [sp, #0x160]\n"
"  adrp x16, _cc_cont@PAGE\n  add x16, x16, _cc_cont@PAGEOFF\n  ldr x16, [x16]\n  br x16\n"
".globl _cc_hook1\n_cc_hook1:\n  adr x17, 1f\n  b _cc_common\n1:\n"   // drawText(Rectangle<float>) 0x1011b4e48
"  sub sp, sp, #0x1e0\n  stp d11, d10, [sp, #0x160]\n  stp d9, d8, [sp, #0x170]\n  stp x28, x27, [sp, #0x180]\n"
"  adrp x16, _cc_cont@PAGE\n  add x16, x16, _cc_cont@PAGEOFF\n  ldr x16, [x16, #8]\n  br x16\n"
".globl _cc_hook2\n_cc_hook2:\n  adr x17, 1f\n  b _cc_common\n1:\n"   // drawFittedText 0x1011b5904
"  stp d11, d10, [sp, #-0x80]!\n  stp d9, d8, [sp, #0x10]\n  stp x28, x27, [sp, #0x20]\n  stp x26, x25, [sp, #0x30]\n"
"  adrp x16, _cc_cont@PAGE\n  add x16, x16, _cc_cont@PAGEOFF\n  ldr x16, [x16, #16]\n  br x16\n"
);
static const struct { uintptr_t addr; uint32_t expect[4]; void (*hook)(void); } H[4] = {
    {0x1011b42d0, {0xd106c3ff, 0x6d1423e9, 0xa9156ffc, 0xa91667fa}, cc_hook0},
    {0x1011b4e48, {0xd10783ff, 0x6d162beb, 0x6d1723e9, 0xa9186ffc}, cc_hook1},
    {0x1011b5904, {0x6db82beb, 0x6d0123e9, 0xa9026ffc, 0xa90367fa}, cc_hook2},
    {0x1011e4da4, {0xa9bc5ff8, 0xa90157f6, 0xa9024ff4, 0xa9037bfd}, cc_hook3},
};
static void say(const char *m) { if (logfd >= 0) write(logfd, m, strlen(m)); }
__attribute__((constructor)) static void init(void) {
    const char *lp = getenv("CC_ZH_LOG"); if (lp) logfd = open(lp, O_WRONLY | O_CREAT | O_APPEND, 0644);
    const char *dp = getenv("CC_ZH_DICT");
    char auto_path[2048]; Dl_info di;
    if (!dp && dladdr((void *)init, &di) && di.dli_fname) {            // 正式版：词典在 App 的 Resources 里
        snprintf(auto_path, sizeof auto_path, "%s", di.dli_fname); char *sl = strrchr(auto_path, '/');
        if (sl) { snprintf(sl, sizeof auto_path - (sl - auto_path), "/../Resources/cczh_dict.tsv"); dp = auto_path; }
    }
    if (dp) load(dp);
    intptr_t slide = 0; int found = 0;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) { const struct mach_header *h = _dyld_get_image_header(i); if (h->filetype == MH_EXECUTE) { slide = _dyld_get_image_vmaddr_slide(i); found = 1; break; } }
    if (!found) { say("#ERR 找不到主程序\n"); return; }
    g_slide = slide; g_lo = 0x100000000ULL + slide; g_hi = g_lo + 0x3000000;
    for (int i = 0; i < 4; i++) if (memcmp((void *)(H[i].addr + slide), H[i].expect, 16)) { say("#ERR 程序版本不对，不挂\n"); return; }   // 版本不对就什么都不做
    for (int i = 0; i < 4; i++) {
        uintptr_t a = H[i].addr + slide; cc_cont[i] = a + 16;
        uint32_t patch[4] = {0x58000050 /* ldr x16,#8 */, 0xd61f0200 /* br x16 */, 0, 0}; uintptr_t t = (uintptr_t)H[i].hook; memcpy(&patch[2], &t, 8);
        vm_address_t pg = a & ~0x3fffULL; vm_size_t len = ((a + 16 + 0x3fff) & ~0x3fffULL) - pg;
        kern_return_t kr = vm_protect(mach_task_self(), pg, len, 0, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
        if (kr) { say("#ERR vm_protect 写失败\n"); return; }
        memcpy((void *)a, patch, 16);
        kr = vm_protect(mach_task_self(), pg, len, 0, VM_PROT_READ | VM_PROT_EXECUTE);
        if (kr) { say("#ERR vm_protect 恢复失败\n"); return; }
        sys_icache_invalidate((void *)a, 16);
    }
    say("#OK 已挂 4 处\n");
}
