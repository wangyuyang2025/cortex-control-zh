#!/bin/zsh
# Cortex Control 中文版 安装器。不需要装 Xcode、Python。
# 它把你电脑上已经装好的官方 Cortex Control 复制一份，改成中文；官方原版不动。
HERE="${0:A:h}"; PAY="$HERE/payload"
ZHVER=$(cat "$PAY/VERSION"); NEED=$(cat "$PAY/CC_VERSION")
SRC="/Applications/Neural DSP/Cortex Control.app"
DEST_DIR="${CC_ZH_DEST:-$HOME/Applications}"; DEST="$DEST_DIR/Cortex Control 中文.app"
fail() { echo; echo "✗ 没装成：$1"; echo; echo "按回车关闭"; read; exit 1; }
echo "Cortex Control 中文版 $ZHVER（适用官方 Cortex Control $NEED，Apple 芯片）"; echo
[ "$(uname -m)" = "arm64" ] || fail "只支持 Apple 芯片的 Mac。"
[ -d "$SRC" ] || fail "没找到官方 Cortex Control（$SRC）。请先装官方版。"
VER=$(/usr/bin/plutil -extract CFBundleShortVersionString raw "$SRC/Contents/Info.plist")
[ "$VER" = "$NEED" ] || fail "你装的官方版是 $VER，这个安装包只适用 $NEED。请到项目页找对应版本的安装包。"
pgrep -f "$DEST/Contents/MacOS/" >/dev/null && fail "中文版正开着，请先退出，等六七秒再装。"
xattr -dr com.apple.quarantine "$PAY" 2>/dev/null
WORK=$(mktemp -d); APP="$WORK/Cortex Control 中文.app"
trap '/bin/rm -rf "$WORK"' EXIT
echo "1/3 复制官方版…"; ditto "$SRC" "$APP" || fail "复制失败。"
echo "2/3 改成中文…"
"$PAY/applydelta" "$SRC/Contents/MacOS/Cortex Control" "$PAY/cc.delta" "$APP/Contents/MacOS/Cortex Control" || fail "官方程序和安装包对不上。"
chmod +x "$APP/Contents/MacOS/Cortex Control"
mkdir -p "$APP/Contents/Frameworks"
cp "$PAY/cczh.dylib" "$APP/Contents/Frameworks/cczh.dylib"
cp "$PAY/cczh_dict.tsv" "$APP/Contents/Resources/cczh_dict.tsv"
/usr/bin/plutil -replace CortexZhVersion -string "$ZHVER" "$APP/Contents/Info.plist"
/usr/bin/plutil -replace CortexZhBasedOn -string "$VER" "$APP/Contents/Info.plist"
echo "3/3 本机签名…"
codesign -d --entitlements :- "$SRC" > "$WORK/ent.plist" 2>/dev/null
codesign --force --deep --sign - --entitlements "$WORK/ent.plist" "$APP" 2>/dev/null || fail "签名失败。"
codesign --verify --strict "$APP" || fail "签名校验不过。"
mkdir -p "$DEST_DIR"
[ -e "$DEST" ] && { /usr/bin/trash "$DEST" 2>/dev/null || mv "$DEST" "$HOME/.Trash/Cortex Control 中文 $(date +%H%M%S).app"; }
ditto "$APP" "$DEST" || fail "放到 $DEST_DIR 失败。"
xattr -dr com.apple.quarantine "$DEST" 2>/dev/null
echo; echo "✓ 装好了：$DEST"
echo "  官方原版没动。想退回：直接开官方版，或把中文版拖进废纸篓。"
echo "  关掉 Cortex Control 后等六七秒再开，开太快 Quad Cortex 容易连不上。"
[ -z "$CC_ZH_QUIET" ] && { open -R "$DEST"; echo; echo "按回车关闭"; read; }
exit 0
