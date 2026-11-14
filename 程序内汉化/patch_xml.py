# Cortex Control 程序内汉化：只改程序里内置的界面布局（XML）文字，不碰程序代码。
# 每份布局改完后长度必须和原来一模一样：译文变长的部分，靠压缩这份布局里的缩进空白腾出来。
# 用法：python3 patch_xml.py 原程序 输出程序 译文.json
import json, re, sys
import xml.etree.ElementTree as ET

src, dst, tr_path = sys.argv[1:4]
data = bytearray(open(src, 'rb').read())
table = {}
for t in json.load(open(tr_path)):
    table[t['raw']] = t['zh']

# 11-14 补：设置页左栏（enumValue name）和 stringProperty default 文字
import os
NAV, DEF = {}, {}
_ep = os.path.join(os.path.dirname(os.path.abspath(__file__)), '补充译文.txt')
if os.path.exists(_ep):
    for l in open(_ep, encoding='utf-8'):
        if l.startswith('#') or l.count('|') < 2: continue
        k, en, zh = l.rstrip('\n').split('|', 2)
        (NAV if k == 'nav' else DEF)[en] = zh.replace('\\n', '\n')


def esc(s):
    s = s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')
    return s.replace('\n', '&#10;')


ATTR = re.compile(rb'(\b(?:text|tooltip)=")([^"]*)(")')
WS = re.compile(rb'\s{2,}')
DEF_RE = re.compile(rb'(id="\w+(?:Text|Message)"\s+)(default=")([^"]*)(")')
NAV_BLOCK = re.compile(rb'id="selectedSettingsSection".*?</enumProperty>', re.S)
NAV_NAME = re.compile(rb'(<enumValue\b[^>]*?\bname=")([^"]*)(")')


def squeeze(blob, need):
    """在引号外面把连续空白缩成一个，直到腾出 need 个字节"""
    out, inq, i, freed = bytearray(), False, 0, 0
    while i < len(blob):
        c = blob[i]
        if c == 0x22:
            inq = not inq
        if not inq and freed < need:
            m = WS.match(blob, i)
            if m:
                run = m.end() - m.start()
                keep = max(1, run - (need - freed))
                out += blob[i:i + keep]
                freed += run - keep
                i = m.end()
                continue
        out.append(c)
        i += 1
    return bytes(out), freed


def parses(b):
    try:
        ET.fromstring(b.decode('utf-8').split('?>', 1)[-1] if b.lstrip().startswith(b'<?xml') else b.decode('utf-8'))
        return True
    except Exception:
        return False


changed_attrs = blobs = skipped = 0
for run in list(re.finditer(rb'[^\x00]{200,}', data)):
    # 只动布局本身：从第一个「<」到最后一个「>」，前后可能挨着别的二进制数据，不能碰
    seg = run.group()
    a = seg.find(b'<?xml')
    a = a if a >= 0 else seg.find(b'<')
    z = seg.rfind(b'>') + 1
    if a < 0 or z <= a:
        continue
    start, end = run.start() + a, run.start() + z
    blob = bytes(data[start:end])
    if not (b'text="' in blob or b'tooltip="' in blob or b'default="' in blob or b'<enumValue' in blob):
        continue
    hits = 0

    def rep(a):
        global hits
        raw = a.group(2).decode('utf-8', 'replace')
        zh = table.get(raw)
        if not zh:
            return a.group(0)
        hits += 1
        return a.group(1) + esc(zh).encode('utf-8') + a.group(3)

    new = ATTR.sub(rep, blob)

    def rep_def(a):
        global hits
        zh = DEF.get(a.group(3).decode('utf-8', 'replace'))
        if not zh:
            return a.group(0)
        hits += 1
        return a.group(1) + a.group(2) + esc(zh).encode('utf-8') + a.group(4)
    new = DEF_RE.sub(rep_def, new)

    def rep_nav(blk):
        def one(a):
            global hits
            zh = NAV.get(a.group(2).decode('utf-8', 'replace'))
            if not zh:
                return a.group(0)
            hits += 1
            return a.group(1) + esc(zh).encode('utf-8') + a.group(3)
        return NAV_NAME.sub(one, blk.group(0))
    new = NAV_BLOCK.sub(rep_nav, new)
    if hits == 0:
        continue
    if len(new) > len(blob):
        new, freed = squeeze(new, len(new) - len(blob))
    if len(new) > len(blob):
        skipped += 1          # 腾不出空间：这份布局保持英文
        continue
    pad = len(blob) - len(new)
    k = new.find(b'?>')
    new = new[:k + 2] + b' ' * pad + new[k + 2:] if k >= 0 else b' ' * pad + new
    assert len(new) == len(blob)
    if parses(blob) and not parses(new):
        skipped += 1
        continue
    data[start:end] = new
    changed_attrs += hits
    blobs += 1

open(dst, 'wb').write(data)
print(f'改了 {blobs} 份布局、{changed_attrs} 处文字；放不下或校验不过而保留英文的布局 {skipped} 份')
