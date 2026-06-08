#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把技能六边形图标与气泡图标从“中文目录”烘焙成可被 Qt 资源系统使用的 ASCII 副本。

为什么需要它：
  Windows 中文区域 + Ninja 在 .qrc 里遇到非 ASCII 路径会报
  “FindFirstFileExA(...???...)”而构建失败。因此把资源都放到 ASCII 路径下。

它做了什么（与游戏内显示一致）：
  - 六边形：graph/六边形/<页>/<编号>.png
        裁掉透明边 -> 顺时针旋转 90°（变成平顶六边形）-> 存到 assets/hex/<0|1|2>/<编号>.png
  - 气泡：  graph/气泡/Biohazard_bonus_icon.png -> assets/bubble/red.png
            graph/气泡/Nda_bonus_icon.png       -> assets/bubble/orange.png

改动了 graph/ 下的原图后，重新运行本脚本即可刷新；若新增了编号，记得在 res.qrc 里补上对应 <file>。

用法： python tools/bake_skill_assets.py
依赖： Pillow（pip install pillow）
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGES = {0: "传播途径", 1: "发病症状", 2: "特殊能力"}  # 页索引 -> 中文目录名


def bake_hex():
    n = 0
    for pg, folder in PAGES.items():
        src_dir = os.path.join(ROOT, "graph", "六边形", folder)
        dst_dir = os.path.join(ROOT, "assets", "hex", str(pg))
        os.makedirs(dst_dir, exist_ok=True)
        for fn in os.listdir(src_dir):
            if not fn.lower().endswith(".png"):
                continue
            im = Image.open(os.path.join(src_dir, fn)).convert("RGBA")
            bbox = im.split()[3].getbbox()      # 透明边裁剪
            if bbox:
                im = im.crop(bbox)
            im = im.rotate(-90, expand=True)    # 顺时针 90°
            im.save(os.path.join(dst_dir, fn))
            n += 1
    print(f"hex icons baked: {n}")


def bake_bubbles():
    pairs = [("Biohazard_bonus_icon.png", "red.png"),
             ("Nda_bonus_icon.png", "orange.png")]
    dst_dir = os.path.join(ROOT, "assets", "bubble")
    os.makedirs(dst_dir, exist_ok=True)
    for src, dst in pairs:
        Image.open(os.path.join(ROOT, "graph", "气泡", src)).convert("RGBA") \
            .save(os.path.join(dst_dir, dst))
    print(f"bubble icons baked: {len(pairs)}")


def _flood_clear_white(im, thresh=242):
    """把与边界相连的近白色像素设为透明（HUD 框外白底），保留内部白色文字/图标。"""
    from collections import deque
    w, h = im.size
    px = im.load()

    def is_white(x, y):
        r, g, b, a = px[x, y]
        return r >= thresh and g >= thresh and b >= thresh
    visited = bytearray(w * h)
    dq = deque()

    def push(x, y):
        if 0 <= x < w and 0 <= y < h and not visited[y * w + x] and is_white(x, y):
            visited[y * w + x] = 1
            dq.append((x, y))
    for x in range(w):
        push(x, 0); push(x, h - 1)
    for y in range(h):
        push(0, y); push(w - 1, y)
    while dq:
        x, y = dq.popleft()
        r, g, b, a = px[x, y]
        px[x, y] = (r, g, b, 0)
        push(x + 1, y); push(x - 1, y); push(x, y + 1); push(x, y - 1)
    return im


def bake_mainui():
    """主页面 HUD 图标（中文目录 -> ASCII 副本，供 qrc 使用）。
       disease/cure/news：本就带透明，裁掉透明边即可。
       world.jpg：白底 -> 把框外白色键透明（保留框内白字），再裁到内容包围盒，存为 world.png。"""
    src = os.path.join(ROOT, "graph", "主页面")
    dst = os.path.join(ROOT, "assets", "mainui")
    os.makedirs(dst, exist_ok=True)
    for name in ["disease.png", "cure.png", "news.png"]:
        im = Image.open(os.path.join(src, name)).convert("RGBA")
        bbox = im.split()[3].getbbox()
        if bbox:
            im = im.crop(bbox)
        im.save(os.path.join(dst, name))
    world = Image.open(os.path.join(src, "world.jpg")).convert("RGBA")
    world = _flood_clear_white(world)
    bbox = world.split()[3].getbbox()
    if bbox:
        world = world.crop(bbox)
    world.save(os.path.join(dst, "world.png"))
    print("mainui icons baked: 4")


if __name__ == "__main__":
    bake_hex()
    bake_bubbles()
    bake_mainui()
    print("done.")
