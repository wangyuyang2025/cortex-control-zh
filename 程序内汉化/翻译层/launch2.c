#include <unistd.h>
#include <stdlib.h>
int main(int c,char**v){ setenv("DYLD_INSERT_LIBRARIES",v[1],1); setenv("CC_ZH_DICT",v[2],1); setenv("CC_ZH_LOG",v[3],1); if(fork()==0){ setsid(); execv(v[4],v+4);} return 0; }
