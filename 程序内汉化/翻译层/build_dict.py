# 生成翻译层词典 cczh_dict.tsv：键（小写英文原文）\t 显示文字。开头是 \x01 的表示「中文 + 英文原文」。
# 型号：只写真名；同一台机器的不同通道带上通道英文名（Clean / Lead…）；仍重名的再带 QC 原名。
import json, re, os, collections
H = os.path.dirname(os.path.abspath(__file__)); D = os.path.join(H, '..', '..', '对照数据')
dyn = json.load(open(D + '/Windows补丁译文/动态文字.json'))
mod = json.load(open(D + '/Windows补丁译文/型号说明.json'))
dev = json.load(open(D + '/devices.json'))
out = {}
for k, v in dyn.items():
    zh = v.split('\n')[0].strip()
    if not zh or '\t' in k or '\n' in k or not re.search(r'[A-Za-z]', k): continue   # 纯数字键是型号编号，不当文字换
    short = len(k) <= 24 and not re.search(r'[.?!。]$', k)
    out[k.lower()] = ('\x01' + zh) if (short and not re.search(r'[A-Za-z]{3,}', zh)) else zh
real = {}
for x in dev:
    if x.get('model') and x.get('orig'): real[x['model']] = [re.sub(r'\s+', ' ', x['orig'].strip()), '']
for k, v in mod.items():
    m = re.search(r'原型：(.+)', v); ver = re.search(r'版本：(.+)', v)
    if m:
        ve = ''
        if ver:                                   # 通道/档位只留英文词：「清音（Clean）；100W」→「Clean 100W」
            ve = ' '.join(w for w in re.findall(r'[A-Za-z0-9][A-Za-z0-9/+\-]*', ver.group(1)) if not w.isdigit())
        real[k] = [re.sub(r'\s+', ' ', m.group(1).strip()), ve]
def join(o, ve):
    extra = [w for w in ve.split() if w.lower() not in o.lower().split()]
    return o + (' ' + ' '.join(extra) if extra else '')
disp = {k: join(o, ve) for k, (o, ve) in real.items()}
# 真名仍然相同的（同一台机器的几个通道/档位）：补上 QC 名里这一组各不相同的那几个词；还分不开才带整个 QC 名
grp = collections.defaultdict(list)
for k, v in disp.items(): grp[v].append(k)
dup = 0
for v, ks in grp.items():
    if len(ks) < 2: continue
    common = set.intersection(*[set(k.split()) for k in ks])
    for k in ks:
        tail = ' '.join(w for w in k.split() if w not in common)
        disp[k] = v + ' ' + tail if tail else v
cnt = collections.Counter(disp.values())
for k in disp:
    if cnt[disp[k]] > 1: disp[k] += ' · ' + k; dup += 1
for k in disp: out.pop(k.lower(), None)                      # Windows 词典里混着型号名的条目：去掉，免得预设名被换
# 真名在三个场合换：M 型号列表、L 通用列表（预设列表）、T 文字标签（预设大标题、参数面板标题）
# 出厂预设的名字和型号名写法略有出入，自动补几种变体：「Plexi 100」→「Plexi100」、「CA 」→「California 」
def variants(k):
    vs = {k}
    vs.add(re.sub(r'([A-Za-z]) (\d)', r'\1\2', k))
    for v in list(vs):
        if v.startswith('CA '): vs.add('California ' + v[3:])
    return vs
for k, v in disp.items():
    for kk in variants(k):
        for t in 'MLTS': out.setdefault(kk.lower() + '@' + t, v)
ov = os.path.join(H, '词典覆盖.txt')          # 人工规则，优先级最高（改词改这里）
ctx = []; exact = {}
if os.path.exists(ov):
    for l in open(ov, encoding='utf-8'):
        l = l.rstrip('\n')
        if l.startswith('@ctx|'): p = l.split('|'); ctx.append((p[1], p[2])); continue
        if l.startswith('#') or '|' not in l: continue
        a, b = l.split('|', 1); exact[a] = b; out.pop(a.lower(), None) if a.lower() == a else None
lines = [f'#ctx\t{a}\t{t}\n' for a, t in ctx] + [f'{k}\t{v}\n' for k, v in exact.items()] + [f'{k}\t{v}\n' for k, v in out.items() if k not in exact]
open(os.path.join(H, 'cczh_dict.tsv'), 'w', encoding='utf-8').write(''.join(lines))
print('词典', len(out), '条；型号', len(disp), '个，其中重名补了 QC 名的', dup)
