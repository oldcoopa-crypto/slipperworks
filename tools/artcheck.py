#!/usr/bin/env python3
# 校验 slipperworks_art.c:每个字符画行数 = h、每行宽 = w、字符都在调色板里。
import re
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else "src/slipperworks_art.c"
PALETTE = set("yor tfplvVGDmBJnbeEcsahHgdxKkw".replace(" ", "") + "k")

text = open(SRC, encoding="utf-8").read()

rows_blocks = {}  # 名字 -> [行字符串]
for m in re.finditer(r'static const char \*const (ROWS_\w+)\[\] = \{(.*?)\};', text, re.S):
    name, body = m.group(1), m.group(2)
    rows_blocks[name] = re.findall(r'"([^"]*)"', body)

arts = []  # (名字, w, h, rows)
for m in re.finditer(r'\{(\d+), (\d+), (ROWS_\w+)\}', text):
    arts.append((m.group(3), int(m.group(1)), int(m.group(2)), m.group(3)))

bad = 0
for name, w, h, rows_name in arts:
    rows = rows_blocks.get(rows_name)
    if rows is None:
        print(f"[缺] {rows_name}: 找不到字符串数组")
        bad += 1
        continue
    if len(rows) != h:
        print(f"[错] {name}({rows_name}): 行数 {len(rows)} != h {h}")
        bad += 1
    for i, r in enumerate(rows):
        if len(r) != w:
            print(f"[错] {name} 第 {i} 行: 宽 {len(r)} != w {w}: {r}")
            bad += 1
        for ch in r:
            if ch != "." and ch not in PALETTE:
                print(f"[错] {name} 第 {i} 行: 未知调色板字符 {ch!r}: {r}")
                bad += 1
                break

if bad:
    sys.exit(1)
print(f"OK: {len(arts)} 张字符画全部通过")
