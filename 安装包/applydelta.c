// 照改动单（cc.delta）把官方程序改成中文版程序。用法：applydelta 官方程序 改动单 输出
// 先核对官方程序的 SHA-256，对不上（版本不同）就不动手；做完再核对成品的 SHA-256。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <CommonCrypto/CommonDigest.h>
static unsigned char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
    unsigned char *b = malloc(*n + 1); if (fread(b, 1, *n, f) != *n) { perror(p); exit(2); }
    fclose(f); return b;
}
static void hex(const unsigned char *b, size_t n, char *out) {
    unsigned char d[32]; CC_SHA256(b, (CC_LONG)n, d);
    for (int i = 0; i < 32; i++) sprintf(out + i * 2, "%02x", d[i]);
}
int main(int c, char **v) {
    if (c != 4) { fprintf(stderr, "用法：applydelta 官方程序 改动单 输出\n"); return 2; }
    size_t sn, dn; unsigned char *s = slurp(v[1], &sn), *d = slurp(v[2], &dn);
    unsigned long long size; char hs[65], ht[65], got[65];
    unsigned char *nl = memchr(d, '\n', dn);
    if (!nl || sscanf((char *)d, "CCZH1 %llu %64s %64s", &size, hs, ht) != 3) { fprintf(stderr, "改动单格式不对\n"); return 2; }
    hex(s, sn, got);
    if (sn != size || strcmp(got, hs)) { fprintf(stderr, "官方程序和这个安装包对不上（版本不同，或者已被改过）\n"); return 3; }
    unsigned char *o = malloc(size), *p = nl + 1, *end = d + dn; size_t w = 0;
    while (p < end) {
        uint64_t a, b;
        if (*p == 'C') { memcpy(&a, p + 1, 8); memcpy(&b, p + 9, 8); p += 17;
            if (a + b > sn || w + b > size) goto bad; memcpy(o + w, s + a, b); w += b; }
        else if (*p == 'D') { memcpy(&b, p + 1, 8); p += 9;
            if (p + b > end || w + b > size) goto bad; memcpy(o + w, p, b); p += b; w += b; }
        else goto bad;
    }
    if (w != size) goto bad;
    hex(o, size, got);
    if (strcmp(got, ht)) { fprintf(stderr, "做出来的程序校验不过\n"); return 4; }
    FILE *f = fopen(v[3], "wb"); if (!f) { perror(v[3]); return 2; }
    if (fwrite(o, 1, size, f) != size) { perror(v[3]); return 2; }
    fclose(f); return 0;
bad: fprintf(stderr, "改动单损坏\n"); return 2;
}
