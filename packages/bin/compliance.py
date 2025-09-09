import glob
import json
import os


with open("HISTORY") as sfile:
    version = sfile.read().split("\n")[0]

COMPLIANCE_HEAD = f"""
# Open Source Compliance Report

This document outlines all third-party components used in this project, along with their licenses, sources, and attribution requirements.

---

## 🧩 Project Metadata

- **Project Name**: Creepspread III
- **Version**: {version}
- **Maintainer**: Francois Blanchette
- **License**: GPL3

---
"""

config = {}
section = ""
with open("data/game.cfg") as sfile:
    for line in sfile:
        line = line.strip().split("#", 1)[0]
        if not line:
            continue
        if line[0] == "[":
            section = line[1:-1]
        elif section:
            if section not in config:
                config[section] = {} if section == "config" else []
            if section == "config":
                a = line.split(" ", 1)
                key = a[0]
                val = a[1].strip().replace('"', "")
                config[section][key] = val
            else:
                config[section].append(line.strip())

music_used = config["musics"] + [
    config["config"]["theme"],
    config["config"]["gameover_theme"],
]


musics = []
for file in glob.glob("data/musics/*"):
    basename = os.path.basename(file)
    musics.append(
        {
            "name": f"{basename}",
            "author": "",
            "version": "",
            "license": "",
            "SPDX_ID": "",
            "source": {"name": "", "url": ""},
            "attribution": "Yes",
        },
    )


tfile = open("techdocs/OPEN_SOURCE_COMPLIANCE.md", "w")
tfile.write(COMPLIANCE_HEAD.strip() + "\n")
tfile.write("## 📦 Third-Party Components\n\n")
columns = [
    "",
    "Component Name",
    "Version",
    "License",
    "SPDX ID",
    "Source URL",
    "Attribution Required",
    "",
]
tfile.write("|".join(columns) + "\n")
separators = ["-" if x else "" for x in columns]
tfile.write("|".join(separators) + "\n")

tfile_tp = open("techdocs/THIRD_PARTY.md", "w")
tfile_tp.write("# Third-Party Libraries\n\n")
tfile_ml = open("techdocs/MUSIC_LICENSES.md", "w")
tfile_ml.write("# # Music Licensing\n\n")
tfile_al = open("techdocs/ASSET_LICENSES.md", "w")
tfile_al.write("# Asset Licensing\n\n")

with open("packages/data/components.json", "r") as file:
    data = json.load(file)
    for item in data:
        source = f"[{item['source']['name']}]({item['source']['url']})"
        row = [
            "",
            item["name"],
            item["version"],
            item["license"],
            item["SPDX_ID"],
            source,
            item["attribution"],
            "",
        ]
        tfile.write("|".join(row) + "\n")
        tfile_tp.write(f"- {item['name']} ({item['license']})\n")


with open("packages/data/musics.json", "r") as file:
    data = json.load(file)
    musics = set([item["name"] for item in data])
    files = set([os.path.basename(file) for file in glob.glob("data/musics/*")])
    for file in files:
        basename = os.path.basename(file)
        if basename not in musics:
            data.append(
                {
                    "name": f"{basename}",
                    "author": "",
                    "version": "",
                    "license": "",
                    "SPDX_ID": "",
                    "source": {"name": "", "url": ""},
                    "attribution": "",
                },
            )
            print(f"missing attribution: {file}")
    removed = []
    for item in data:
        if item["name"] not in files:
            removed.append(item["name"])
            print(f"attribution for non-existent: {item['name']}")
    data = sorted(data, key=lambda k: k["name"].lower(), reverse=False)
    for item in data:
        if item["name"] in removed:
            continue
        if item["name"] not in music_used:
            print(f"not referenced in config: {item['name']}")
        source = f"[{item['source']['name']}]({item['source']['url']})"
        row = [
            "",
            item["name"],
            item["version"],
            item["license"],
            item["SPDX_ID"],
            source,
            item["attribution"],
            "",
        ]
        tfile.write("|".join(row) + "\n")
        tfile_tp.write(f"- {item['name']} ({item['license']})\n")
        tfile_ml.write(
            f"""
            ## {item['name']} by {item['author']}
            - License: {item['license']}
            - Source: [{item['source']['name']}]({item['source']['url']})
            - Attribution: “{item['name']}” by {item['author']}, licensed under {item['license']}
            """.strip().replace(
                " " * 12, ""
            )
            + "\n\n"
        )

with open("packages/data/musics-renewed.json", "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=4)


with open("packages/data/assets.json", "r") as file:
    data = json.load(file)
    for item in data:
        source = f"[{item['source']['name']}]({item['source']['url']})"
        row = [
            "",
            item["name"],
            item["version"],
            item["license"],
            item["SPDX_ID"],
            source,
            item["attribution"],
            "",
        ]
        tfile.write("|".join(row) + "\n")
        tfile_tp.write(f"- {item['name']} ({item['license']})\n")
        tfile_al.write(
            f"""
            ## {item['name']} by {item['author']}
            - License: {item['license']}
            - Source: [{item['source']['name']}]({item['source']['url']})
            - Attribution: “{item['name']}” by {item['author']}, licensed under {item['license']}
            """.strip().replace(
                " " * 12, ""
            )
            + "\n\n"
        )
