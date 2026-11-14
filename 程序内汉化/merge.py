# 合并译文：优先用 Windows 补丁的中文，没有的用我们自己写的；短标签里有行业词时保留英文原文
import json, sys, html, re
D = sys.argv[1]
ui = json.load(open(D + '/ui_strings.json'))
win = json.load(open(D + '/Windows补丁译文/界面文字.json'))
mine = {}
for l in open(D + '/ui_zh.txt'):
    if l.startswith('#') or not l.strip(): continue
    i, s, t = l.rstrip('\n').split('|', 2); mine[int(i)] = (s, t)

TERMS = r'\b(preset|presets|scene|scenes|setlist|grid|neural capture|captures?|ir|irs|impulse responses?|stomp|gig view|splitters?|mixers?|looper|expression|bypass|cab|cabs|amp|amps|eq|gate|cortex cloud|coros|hybrid|latch|latching|momentary|send|return|phantom|impedance|ground lift|frfr|xlr|tempo|tuner|footswitch|doubler)\b'

def final(en, zh):
    short = len(en) <= 32 and '\n' not in en and not re.search(r'[.?!]$', en.strip())
    if short and re.search(TERMS, en, re.I) and not re.search(r'[A-Za-z]{3,}', zh):
        return zh + ' ' + en          # 例：预设库名称 Setlist Name
    return zh

over = {}
import os
op = os.path.join(os.path.dirname(os.path.abspath(__file__)), '译文覆盖.txt')
if os.path.exists(op):
    for l in open(op):
        if l.startswith('#') or '|' not in l: continue
        a, b = l.rstrip('\n').split('|', 1); over[a] = b
NARROW = {'bottomBarView'}   # 底栏按钮窄，只放中文

out = []
for i, x in enumerate(ui):
    raw = x['en']
    en = html.unescape(raw).replace('&#10;', '\n')
    s, t = mine[i]
    if s == 'x': continue
    w = win.get(en) or win.get(raw)
    zh = (w if w else t).replace('&#10;', '\n')
    if not zh: continue
    if en in over:
        z, src = over[en], 'over'
    elif set(x['ctx']) & NARROW:
        z, src = zh, 'win' if w else 'mine'
    else:
        z, src = final(en, zh), 'win' if w else 'mine'
    out.append({'i': i, 'raw': raw, 'en': en, 'zh': z, 'src': src})
json.dump(out, open(sys.argv[2], 'w'), ensure_ascii=False, indent=1)
print('译文', len(out), '其中 Windows', sum(o['src'] == 'win' for o in out))
for o in out:
    if o['i'] in (16, 18, 82, 97, 207, 212, 236, 314, 441, 497, 498, 59): print(o['en'][:30], '→', o['zh'][:40])
