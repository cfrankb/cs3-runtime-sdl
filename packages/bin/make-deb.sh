#!/bin/bash

set -e
#licensecheck -r ./src > license_report.txt
python3 packages/bin/deb-genc.py


mkdir -p build && cd build
rm -f *.deb
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
make install DESTDIR=package
cpack -G DEB

echo $(pwd)
lintian *.deb
dpkg-deb -I *.deb
