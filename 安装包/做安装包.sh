#!/bin/zsh
# 生成给别人下载的 Mac 安装包（zip）。维护者用；需要 Xcode 命令行工具、python3 + numpy。
# 安装包里没有官方程序：只有翻译层插件、词典、改动单（译文 + 抄写指令）和一个照单改写的小工具。
set -e
HERE="${0:A:h}"; ROOT="${HERE:h}"; Z="$ROOT/程序内汉化"; TL="$Z/翻译层"
SRC="/Applications/Neural DSP/Cortex Control.app"; BIN0="$SRC/Contents/MacOS/Cortex Control"
ZHVER=$(cat "$ROOT/VERSION")
CCVER=$(/usr/bin/plutil -extract CFBundleShortVersionString raw "$SRC/Contents/Info.plist")
NAME="Cortex-Control-zh_v${ZHVER}_CC${CCVER}_mac-arm64"
WORK="${TMPDIR:-/tmp}/cortex-zh-release"; /bin/rm -rf "$WORK"; mkdir -p "$WORK/$NAME/payload"
PAY="$WORK/$NAME/payload"
python3 -I "$Z/merge.py" "$ROOT/对照数据" "$Z/译文.json"
python3 -I "$Z/patch_xml.py" "$BIN0" "$WORK/bin" "$Z/译文.json"
zsh "$TL/build.sh"
python3 -I "$TL/add_dylib.py" "$WORK/bin" "@executable_path/../Frameworks/cczh.dylib"
python3 "$HERE/make_delta.py" "$BIN0" "$WORK/bin" "$PAY/cc.delta"
clang -arch arm64 -O2 -o "$PAY/applydelta" "$HERE/applydelta.c"; codesign -s - -f "$PAY/applydelta"
cp "$TL/cczh.dylib" "$TL/cczh_dict.tsv" "$PAY/"
echo "$ZHVER" > "$PAY/VERSION"; echo "$CCVER" > "$PAY/CC_VERSION"
cp "$HERE/安装.command" "$HERE/使用说明.txt" "$ROOT/LICENSE" "$WORK/$NAME/"
sed -i '' "s/{ZHVER}/$ZHVER/g; s/{CCVER}/$CCVER/g" "$WORK/$NAME/使用说明.txt"
chmod +x "$WORK/$NAME/安装.command"
mkdir -p "$ROOT/发布"; OUT="$ROOT/发布/$NAME.zip"
[ -e "$OUT" ] && /usr/bin/trash "$OUT"
(cd "$WORK" && ditto -c -k --keepParent "$NAME" "$OUT")
shasum -a 256 "$OUT" | awk '{print $1}' > "$OUT.sha256"
echo "安装包：$OUT（$(du -h "$OUT" | cut -f1)）"
