#!/usr/bin/env python3
"""Define o estado do controle de teste. Uso: pad.py [botoes...] [lx=..] [ly=..] [rx=..] [ry=..]
Botoes com nomes do Switch: A B X Y L R ZL ZR Plus Minus L3 R3 Up Down Left Right
Eixos em -1..1, y para cima positivo (como XInput)."""
import sys
bits={'B':0x1000,'A':0x2000,'Y':0x4000,'X':0x8000,'L':0x100,'R':0x200,'Plus':0x10,'Minus':0x20,'L3':0x40,'R3':0x80,
      'Up':1,'Down':2,'Left':4,'Right':8}
b=0;lt=rt=0;ax={'lx':0,'ly':0,'rx':0,'ry':0}
for a in sys.argv[1:]:
    if '=' in a:k,v=a.split('=');ax[k]=int(float(v)*32767)
    elif a=='ZL':lt=255
    elif a=='ZR':rt=255
    else:b|=bits[a]
line = f"{b:x} {lt} {rt} {ax['lx']} {ax['ly']} {ax['rx']} {ax['ry']}\n"
for path in ['/tmp/pad.txt', 'pad.txt', 'build/test_game/pad.txt', 'build/test_game/d2nx/pad.txt']:
    try:
        import os
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, 'w') as f:
            f.write(line)
    except Exception:
        pass
