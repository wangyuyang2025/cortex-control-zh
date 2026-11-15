#!/usr/bin/env python3
"""比较官方程序和改好的程序，生成「改动单」cc.delta。
改动单里只有两种指令：从官方程序某处抄一段（COPY）、写入一段我们自己的字节（DATA）。
这样安装包里不含官方程序的内容，只含译文和几十个字节的加载指令。
格式：一行文本头 `CCZH1 <大小> <官方sha256> <成品sha256>\n`，后面是指令：
  'C' + u64 源偏移 + u64 长度    |   'D' + u64 长度 + 字节    （小端）
用法：make_delta.py 官方程序 改好的程序 输出"""
import sys, re, struct, hashlib, difflib
o = open(sys.argv[1], 'rb').read(); n = open(sys.argv[2], 'rb').read()
assert len(o) == len(n), "两个文件大小不同"
# 找出有差异的区段（相隔不到 256 字节的并成一段）
import numpy as np
d = np.flatnonzero(np.frombuffer(o, np.uint8) != np.frombuffer(n, np.uint8))
regions = []
if len(d):
    s = p = int(d[0])
    for x in d[1:]:
        x = int(x)
        if x - p > 256: regions.append((s, p + 1)); s = x
        p = x
    regions.append((s, p + 1))
TOK = re.compile(rb'[A-Za-z0-9_.:#-]+|[ \t\r\n]+|[^A-Za-z0-9_.:#\s-]')
ops = []  # ('C', off, len) / ('D', bytes)
def copy(off, ln):
    if ln <= 0: return
    if ops and ops[-1][0] == 'C' and ops[-1][1] + ops[-1][2] == off: ops[-1] = ('C', ops[-1][1], ops[-1][2] + ln)
    else: ops.append(('C', off, ln))
def data(b):
    if not b: return
    if ops and ops[-1][0] == 'D': ops[-1] = ('D', ops[-1][1] + b)
    else: ops.append(('D', b))
pos = 0
for s, e in regions:
    copy(pos, s - pos)
    a = [(m.start() + s, m.group()) for m in TOK.finditer(o[s:e])]
    b = [m.group() for m in TOK.finditer(n[s:e])]
    sm = difflib.SequenceMatcher(None, [t for _, t in a], b, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            ln = sum(len(t) for _, t in a[i1:i2])
            if ln >= 24: copy(a[i1][0], ln)       # 太短的相同片段直接写字节，省指令
            else: data(b''.join(b[j1:j2]))
        else: data(b''.join(b[j1:j2]))
    pos = e
copy(pos, len(o) - pos)
# 自检：照改动单重做一遍，必须和成品一模一样
out = bytearray()
for op in ops: out += o[op[1]:op[1] + op[2]] if op[0] == 'C' else op[1]
assert bytes(out) == n, "改动单重做结果和成品不一致"
h = lambda x: hashlib.sha256(x).hexdigest()
with open(sys.argv[3], 'wb') as f:
    f.write(f"CCZH1 {len(o)} {h(o)} {h(n)}\n".encode())
    for op in ops:
        if op[0] == 'C': f.write(b'C' + struct.pack('<QQ', op[1], op[2]))
        else: f.write(b'D' + struct.pack('<Q', len(op[1])) + op[1])
nd = sum(len(op[1]) for op in ops if op[0] == 'D')
print(f"改动单：{len(regions)} 处改动区，{len(ops)} 条指令，自带字节 {nd}（占成品 {nd/len(n)*100:.3f}%）")
