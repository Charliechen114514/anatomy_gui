#!/usr/bin/env python3
"""Build tutorial CMake projects — used by both CI and local development.

Usage:
    python build_anatomy_gui_for_tutorials.py                      # build all for current platform
    python build_anatomy_gui_for_tutorials.py --target win32        # build only native_win32
    python build_anatomy_gui_for_tutorials.py --target anatomy-stage1 --target anatomy-stage2
"""

import argparse
import platform
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parent

TARGETS = {
    "win32": {
        "source": ROOT / "native_win32",
        "platform": "windows",
    },
    "graphics": {
        "source": ROOT / "graphics",
        "platform": "windows",
    },
    "anatomy-stage1": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage1",
        "platform": "linux",
    },
    "anatomy-stage2": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage2",
        "platform": "linux",
    },
    "anatomy-stage3": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage3",
        "platform": "linux",
    },
    "anatomy-stage4": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage4",
        "platform": "linux",
    },
    "anatomy-stage5": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage5",
        "platform": "linux",
    },
    "anatomy-stage6": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage6",
        "platform": "linux",
    },
    "anatomy-stage7": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage7",
        "platform": "linux",
    },
    "anatomy-stage8": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage8",
        "platform": "linux",
    },
    "anatomy-stage9": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage9",
        "platform": "linux",
    },
    "anatomy-stage10": {
        "source": ROOT / "anatomy_gui_for_tutorials" / "stage10",
        "platform": "linux",
    },
}

VALID_TARGETS = list(TARGETS.keys())


def current_platform() -> str:
    return "windows" if platform.system() == "Windows" else "linux"


def build_target(name: str, cfg: dict) -> tuple[str, bool]:
    source = cfg["source"]
    build_dir = source / "build_ci"

    if not (source / "CMakeLists.txt").exists():
        print(f"  [{name}] SKIP — no CMakeLists.txt in {source}")
        return name, True

    if build_dir.exists():
        shutil.rmtree(build_dir)

    print(f"  [{name}] Configuring ({source})")
    ret = subprocess.run(
        ["cmake", "-B", str(build_dir), "-S", str(source)],
    )
    if ret.returncode != 0:
        print(f"  [{name}] CMake configure FAILED")
        return name, False

    print(f"  [{name}] Building")
    ret = subprocess.run(
        ["cmake", "--build", str(build_dir), "--config", "Release", "--parallel"],
    )
    if ret.returncode != 0:
        print(f"  [{name}] Build FAILED")
        return name, False

    print(f"  [{name}] OK")
    return name, True


def main() -> None:
    parser = argparse.ArgumentParser(description="Build tutorial CMake projects")
    parser.add_argument(
        "--target",
        choices=VALID_TARGETS + ["all"],
        default=None,
        action="append",
        help="Which target to build (repeatable; default: all for current platform)",
    )
    args = parser.parse_args()

    plat = current_platform()
    targets = args.target or ["all"]

    if "all" in targets:
        selected = [n for n, c in TARGETS.items() if c["platform"] == plat]
    else:
        selected = targets

    if not selected:
        print(f"No targets available on {plat}")
        sys.exit(0)

    print("=" * 50)
    print(f" Building: {', '.join(selected)}  (platform: {plat})")
    print("=" * 50)

    failed: list[str] = []

    with ThreadPoolExecutor(max_workers=len(selected)) as pool:
        futures = {
            pool.submit(build_target, name, TARGETS[name]): name
            for name in selected
            if TARGETS[name]["platform"] == plat
        }
        for future in as_completed(futures):
            name, ok = future.result()
            if not ok:
                failed.append(name)

    print()
    print("=" * 50)
    if not failed:
        print(f" ALL PASSED — {len(selected)} targets built")
    else:
        print(f" FAILED ({len(failed)}): {', '.join(failed)}")
    print("=" * 50)

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
