#!/bin/sh
# RSSW.ROM: RSS.ROM packed (ZX0) into a 16 KB "WIFI+Telnet" ROM for the MSX Pico+ menu.
# Run from the repo root after ./build.sh:  ZX0=/path/to/zx0 sh tools/build_rssw.sh
set -e
SJ=${SJASMPLUS:-sjasmplus}; ZX0=${ZX0:-zx0}
cd "$(dirname "$0")/../src"
# RSS.ROM up to the end of its code (the rest is FFh padding)
python3 -c "
d=open('../RSS.ROM','rb').read(); e=len(d)
while d[e-1]==0xFF: e-=1
open('rss_img.bin','wb').write(d[:e]); print('RSS image', e, 'bytes')"
$ZX0 -f rss_img.bin rss_img.zx0
$SJ --nologo --msg=war --raw=../RSSW.ROM rssw.asm
rm -f rss_img.bin rss_img.zx0
ls -l ../RSSW.ROM
