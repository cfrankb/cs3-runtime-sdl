#!/usr/bin/python

import ntpath
import glob
import os
import datetime
import filecmp
import shutil
from pathlib import Path


DEST_BASE = "/home/cfrankb/toolkit/qt/cs3-map-edit/src"
DEST_CODE = DEST_BASE + "/runtime/"
DEST_PIXELS = DEST_BASE + "/data/"
TAB = " " * 4


def is_excluded(f: str):
    excluded = [
        "main.cpp",
        "menuitem.*",
        "assetman*",
        "recorder.*",
        "menu.*",
        "parseargs.*",
        "runtime.*",
    ]
    f = ntpath.basename(f)
    for e in excluded:
        if e.endswith("*"):
            if f.startswith(e[0:-1]):
                return True
        elif f == e:
            return True
    return False


def get_modif(file_path):
    modification_timestamp = os.path.getmtime(file_path)
    modification_datetime = datetime.datetime.fromtimestamp(modification_timestamp)
    return modification_datetime


def sync_code(sync_list):
    paths = ["src", "src/shared"]
    for path in paths:
        files = glob.glob(path + "/*")
        files.sort()
        for f in files:
            if (
                f.endswith(".cpp") or f.endswith(".h") or f.endswith(".inc")
            ) and not is_excluded(f):
                sync_list.append([f, DEST_CODE + "/".join(f.split("/")[1:]), False])


def sync_pixels(sync_list):
    paths = [
        "data/pixels/animz.obl",
        "data/pixels/bosses.obl",
        "data/pixels/tiles.obl",
        "data/pixels/uisheet.png",
    ]
    for f in paths:
        sync_list.append([f, DEST_PIXELS + ntpath.basename(f), False])


def sync_map(sync_list):
    sync_list.append(["tests/in/levels2.mapz", "data/levels.mapz", True])


def process_list(sync_list, action, verbose):
    if action == "copy":
        tfile = open("sync.log", "a")
        now = datetime.datetime.now()
        dt = str(now).split(".")[0]
        tfile.write(f"{dt}\n")

    for u in sync_list:
        f, dpath, backup = u
        m1 = str(get_modif(f))[0:-7]
        if os.path.isfile(dpath):
            m2 = str(get_modif(dpath))[0:-7]
            if not filecmp.cmp(f, dpath):
                s1 = Path(f).stat().st_size
                s2 = Path(dpath).stat().st_size
                diff = s1 - s2
                diff = str(diff) if diff < 0 else "+" + str(diff)
                print(f"[X] {f} {diff}")
            else:
                # pass
                if verbose:
                    print(f"[ ] {f}")
                continue
        else:
            s1 = Path(f).stat().st_size
            print(f"[X] {f} +{s1} [missing]")

        if action != "copy":
            continue
        if backup and os.path.isfile(dpath):
            # levels.mapz.bak_20251015_223435
            suffix = m2.replace("-", "").replace(":", "").replace(" ", "_")
            dpath_bak = f"{dpath}.bak_{suffix}"
            shutil.copy2(dpath, dpath_bak)
            tfile.write(f'{TAB}backup "{dpath}" to "{dpath_bak}"\n')
        shutil.copy2(f, dpath)
        tfile.write(f'{TAB}copy "{f}" to "{dpath}"\n')

    if action == "copy":
        tfile.write("\n")


sync_list = []

sync_code(sync_list=sync_list)
sync_pixels(sync_list=sync_list)
sync_map(sync_list=sync_list)
process_list(sync_list=sync_list, action="copy", verbose=False)
