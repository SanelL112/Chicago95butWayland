#!/usr/bin/env python3
"""
generate_wallpapers.py - Generate authentic Windows 95 era wallpapers,
focusing on Setup.bmp and its hardware/silicon collage siblings.
"""
import os
import shutil
from PIL import Image
import numpy as np

WALLPAPER_DIRS = [
    os.path.expanduser("~/.local/share/backgrounds/chicago95/Wallpaper"),
    "/home/sanellathiya/chicago-wayland/themes/backgrounds/Wallpaper"
]

def ensure_dirs():
    for d in WALLPAPER_DIRS:
        os.makedirs(d, exist_ok=True)

def save_image(img: Image.Image, filename: str):
    for d in WALLPAPER_DIRS:
        out_path = os.path.join(d, filename)
        img.save(out_path, format="PNG", optimize=True)
        print(f"Saved: {out_path} ({img.size[0]}x{img.size[1]})")

def build_1bit_colorways(src_path: str):
    """
    Takes 1-bit Setup.png (640x480, (0,0,0) and (0,0,255))
    and remaps into classic retro monitor and Windows 95 palettes.
    """
    im = Image.open(src_path).convert("RGB")
    arr = np.array(im)
    is_blue = (arr[:, :, 2] > 100) & (arr[:, :, 0] < 50) & (arr[:, :, 1] < 50)

    colorways = [
        ("Setup (Original Blue).png", (0, 0, 0), (0, 0, 255)),
        ("Setup (Windows 95 Teal).png", (0, 0, 0), (0, 128, 128)),
        ("Setup (Phosphor Amber).png", (10, 8, 0), (255, 176, 0)),
        ("Setup (Cyber Green).png", (0, 17, 0), (0, 255, 102)),
        ("Setup (Midnight Monochrome).png", (18, 18, 18), (192, 192, 192)),
        ("Setup (Rose Burgundy).png", (20, 0, 8), (176, 32, 64)),
        ("Setup (Electric Cyan).png", (0, 18, 26), (0, 229, 255)),
    ]

    for fname, bg_col, fg_col in colorways:
        out = np.zeros_like(arr)
        out[~is_blue] = bg_col
        out[is_blue] = fg_col
        out_img = Image.fromarray(out)
        save_image(out_img, fname)

def build_hires_colorways(src_path: str):
    """
    Takes 1280x960 multitone Setup.png (blue channel 0..255)
    and smoothly maps intensity to target hues.
    """
    im = Image.open(src_path).convert("RGB")
    arr = np.array(im)
    blue_norm = arr[:, :, 2].astype(np.float32) / 255.0

    colorways = [
        ("Setup Hi-Res (Original Blue).png", (0, 0, 255)),
        ("Setup Hi-Res (Windows 95 Teal).png", (0, 128, 128)),
        ("Setup Hi-Res (Phosphor Amber).png", (255, 176, 0)),
        ("Setup Hi-Res (Cyber Green).png", (0, 255, 102)),
    ]

    for fname, (tr, tg, tb) in colorways:
        out = np.zeros_like(arr)
        out[:, :, 0] = (blue_norm * tr).astype(np.uint8)
        out[:, :, 1] = (blue_norm * tg).astype(np.uint8)
        out[:, :, 2] = (blue_norm * tb).astype(np.uint8)
        out_img = Image.fromarray(out)
        save_image(out_img, fname)

def build_widescreen_editions(src_path: str):
    """
    Takes 1280x960 multitone Setup.png and creates 1920x1080 native canvas
    with seamless centered presentation.
    """
    im = Image.open(src_path).convert("RGB")
    arr = np.array(im)
    blue_norm = arr[:, :, 2].astype(np.float32) / 255.0

    colorways = [
        ("Setup Widescreen (Original Blue).png", (0, 0, 0), (0, 0, 255)),
        ("Setup Widescreen (Windows 95 Teal).png", (0, 0, 0), (0, 128, 128)),
        ("Setup Widescreen (Phosphor Amber).png", (10, 8, 0), (255, 176, 0)),
        ("Setup Widescreen (Cyber Green).png", (0, 17, 0), (0, 255, 102)),
    ]

    W, H = 1920, 1080
    orig_w, orig_h = 1280, 960
    ox = (W - orig_w) // 2
    oy = (H - orig_h) // 2

    for fname, bg_col, (tr, tg, tb) in colorways:
        canvas = np.zeros((H, W, 3), dtype=np.uint8)
        canvas[:, :] = bg_col

        content = np.zeros((orig_h, orig_w, 3), dtype=np.uint8)
        content[:, :, 0] = (blue_norm * tr).astype(np.uint8)
        content[:, :, 1] = (blue_norm * tg).astype(np.uint8)
        content[:, :, 2] = (blue_norm * tb).astype(np.uint8)

        canvas[oy:oy+orig_h, ox:ox+orig_w] = content
        save_image(Image.fromarray(canvas), fname)

def build_inside_computer(src_path: str):
    """
    Build authentic Microsoft Plus! 95 'Inside Your Computer' and
    convert into Setup-style 1-bit dithered siblings.
    """
    im = Image.open(src_path).convert("RGB")
    # 1. Authentic full color
    save_image(im, "Inside Your Computer.png")

    # 2. Setup-style dithered siblings
    gray = im.convert("L")
    dithered = gray.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
    darr = np.array(dithered) # True/False

    dither_colorways = [
        ("Inside Your Computer (Setup Blue).png", (0, 0, 0), (0, 0, 255)),
        ("Inside Your Computer (Setup Teal).png", (0, 0, 0), (0, 128, 128)),
        ("Inside Your Computer (Setup Amber).png", (10, 8, 0), (255, 176, 0)),
        ("Inside Your Computer (Setup Green).png", (0, 17, 0), (0, 255, 102)),
    ]

    for fname, bg_col, fg_col in dither_colorways:
        out = np.zeros((im.size[1], im.size[0], 3), dtype=np.uint8)
        out[~darr] = bg_col
        out[darr] = fg_col
        save_image(Image.fromarray(out), fname)

def copy_classics():
    """
    Copies authentic downloaded classics (Clouds, Winlogo, Forest, Gold Weave).
    """
    classics = [
        ("/tmp/Clouds.png", "Clouds.png"),
        ("/tmp/WIN95.png", "Winlogo.png"),
        ("/tmp/Forest.png", "Forest.png"),
        ("/tmp/Gold_Weave.png", "Gold Weave.png"),
    ]
    for src, dst_name in classics:
        if os.path.isfile(src):
            im = Image.open(src)
            save_image(im, dst_name)

def main():
    ensure_dirs()
    setup_640 = "/home/sanellathiya/.local/share/backgrounds/chicago95/Wallpaper/Setup.png"
    setup_1280 = "/home/sanellathiya/.gemini/antigravity-cli/brain/527f856f-c916-4ff5-b484-efa968eae6ba/scratch/upstream/Chicago95/Extras/setup.png"
    inside_clean = "/tmp/inside_your_computer_clean.png"

    print("Building 1-bit Setup colorways...")
    build_1bit_colorways(setup_640)

    print("Building Hi-Res Setup colorways...")
    build_hires_colorways(setup_1280)

    print("Building Widescreen 1080p Setup editions...")
    build_widescreen_editions(setup_1280)

    print("Building Inside Your Computer & Setup-dithered siblings...")
    build_inside_computer(inside_clean)

    print("Copying classic Windows 95 era wallpapers...")
    copy_classics()

    print("\nAll Windows 95 wallpapers generated and installed successfully!")

if __name__ == "__main__":
    main()
