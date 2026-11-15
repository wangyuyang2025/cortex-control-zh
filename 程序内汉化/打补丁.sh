#!/bin/zsh
# Cortex Control 中文版：从官方 Cortex Control 复制一份，把界面文字换成中文。官方原版不动。
# 官方原版不动。官方升级后重跑一次即可。
set -e
HERE="${0:A:h}"
SRC="/Applications/Neural DSP/Cortex Control.app"
# 装到哪：可用环境变量 CC_ZH_DEST 指定；作者自己的机器上有「MusicATB控制中心」就装那里；否则装到 ~/Applications
if [ -n "$CC_ZH_DEST" ]; then DEST_DIR="$CC_ZH_DEST"
elif [ -d "$HOME/Library/Application Support/MusicATB控制中心/程序" ]; then DEST_DIR="$HOME/Library/Application Support/MusicATB控制中心/程序"
else DEST_DIR="$HOME/Applications"; fi
DEST="$DEST_DIR/Cortex Control 中文.app"
WORK="${TMPDIR:-/tmp}/cortex-zh-patch"

[ -d "$SRC" ] || { echo "找不到官方 Cortex Control：$SRC"; exit 1; }
VER=$(/usr/bin/plutil -extract CFBundleShortVersionString raw "$SRC/Contents/Info.plist")
ZHVER=$(cat "$HERE/../VERSION" 2>/dev/null || echo dev)

python3 -I "$HERE/merge.py" "$HERE/../对照数据" "$HERE/译文.json"

rm -rf "$WORK"; mkdir -p "$WORK"
ditto "$SRC" "$WORK/Cortex Control 中文.app"
BIN="$WORK/Cortex Control 中文.app/Contents/MacOS/Cortex Control"
python3 -I "$HERE/patch_xml.py" "$SRC/Contents/MacOS/Cortex Control" "$BIN" "$HERE/译文.json"
# patch_cstr.py 停用（11-14 实测）：程序把这些字当 ASCII 读，换成中文显示成乱码。要汉化得挂翻译函数，不做。
chmod +x "$BIN"

# 翻译层（11-14）：型号真名、参数名、左侧分类这些程序里现画的字。只认对得上的版本，对不上自动不挂。
TL="$HERE/翻译层"
if [ -f "$TL/hook.c" ]; then
  zsh "$TL/build.sh"
  mkdir -p "$WORK/Cortex Control 中文.app/Contents/Frameworks"
  cp "$TL/cczh.dylib" "$WORK/Cortex Control 中文.app/Contents/Frameworks/cczh.dylib"
  cp "$TL/cczh_dict.tsv" "$WORK/Cortex Control 中文.app/Contents/Resources/cczh_dict.tsv"
  python3 -I "$TL/add_dylib.py" "$BIN" "@executable_path/../Frameworks/cczh.dylib"
fi

# 沿用官方的权限声明（麦克风等），用本机签名
codesign -d --entitlements :- "$SRC" > "$WORK/ent.plist" 2>/dev/null
codesign --force --deep --sign - --entitlements "$WORK/ent.plist" "$WORK/Cortex Control 中文.app"
codesign --verify --strict "$WORK/Cortex Control 中文.app"
/usr/bin/plutil -replace CortexZhVersion -string "$ZHVER" "$WORK/Cortex Control 中文.app/Contents/Info.plist" 2>/dev/null || true
/usr/bin/plutil -replace CortexZhBasedOn -string "$VER" "$WORK/Cortex Control 中文.app/Contents/Info.plist" 2>/dev/null || true
codesign --force --deep --sign - --entitlements "$WORK/ent.plist" "$WORK/Cortex Control 中文.app"

mkdir -p "$DEST_DIR"
rm -rf "$DEST"
ditto "$WORK/Cortex Control 中文.app" "$DEST"
xattr -dr com.apple.quarantine "$DEST" 2>/dev/null || true
echo "装好了：$DEST（中文版 $ZHVER，基于官方 $VER）"
