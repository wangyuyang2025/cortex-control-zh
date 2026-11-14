#!/bin/zsh
# 生成翻译层：词典 cczh_dict.tsv + 插件 cczh.dylib（只支持 Apple 芯片）
set -e; cd "${0:A:h}"
python3 -I build_dict.py
clang -arch arm64 -dynamiclib -O2 -o cczh.dylib hook.c
codesign -s - -f cczh.dylib
echo "翻译层已生成"
