#!/usr/bin/env python3
"""Generate MCU fonts: CJK semibold labels and IBM Plex Mono instrument numerals."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--font',required=True,help='Noto Sans SC Semibold TTF')
p.add_argument('--numeral-font',required=True,help='IBM Plex Mono Semibold TTF')
p.add_argument('--converter',required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
chars=(root/'assets/fonts/characters.txt').read_text()
for suffix,size,cjk in [('',16,True),('_small',12,True),('_medium',18,False),('_title',20,False),('_number',34,False),('_clock',26,False),('_token',24,False)]:
    name='usage_font'+suffix;out=root/'main'/f'{name}.c'
    command=['node',a.converter,'--font',a.font if cjk else a.numeral_font,'--size',str(size),'--bpp','2','--format','lvgl','--range','0x20-0x7e','--no-compress','--no-prefilter','--output',str(out),'--lv-font-name',name]
    if cjk:command+=['--symbols',chars]
    subprocess.run(command,check=True)
    text=out.read_text();start=text.index(' * Opts:');end=text.index('\n',start)
    text=text[:start]+f' * {"Noto Sans SC" if cjk else "IBM Plex Mono"} Semibold subset, SIL OFL 1.1; see assets/fonts/.'+text[end:]
    out.write_text(text.replace('#include "lvgl/lvgl.h"','#include "lvgl.h"').rstrip()+'\n')
