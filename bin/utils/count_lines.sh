#/bin/bash
find src  -type f -print0 | xargs -0 wc -l
cloc src