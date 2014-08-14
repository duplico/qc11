"""
sprite_to_c.py

Script to translate Jonathan's sprites into C literals.

(c) 2014 George Louthan
3-clause BSD license; see license.md.

"""
import os
import json
import re
import ast

infiles = [f for f in os.listdir('.') if os.path.isfile(f) and '.' not in f]

for f in infiles:
    with open(f, 'r') as jspritefile:
        jscode = jspritefile.read()
    values = re.findall(r'var.*?=\s*(.*?);', jscode, re.DOTALL | re.MULTILINE)
    for value in values:
        animation = ast.literal_eval(value)
        animname = f.split('.')[0]
        # outcode = "// Auto-generated animation %s\n" % animname
        outcode = ""
        if len(animation[0][0]) == 8:
            outcode += "const spriteframe anim_%s[] = {" % animname
        else:
            outcode += "const fullframe anim_%s[] = {" % animname
        frame_flag = 0
        framecode = ""
        for i in range(len(animation)):
            frame = animation[i]
            
            assert len(frame) in [5, 6]
            framecode = "{"
            for row in frame[4::-1]:
                framecode += "0b" + ''.join(map(str,row)) + ', '
            
            if frame is animation[-1]:
                frame_flag += 128
                framecode += "%d}," % frame_flag
                outcode += framecode
                frame_flag = 0
                # if this is the last frame, flush. frame_flag += 128.
                # flush
            elif frame == animation[i+1]:
                # wait and let last frame flush.
                frame_flag += 1
            else:
                framecode += "%d}," % frame_flag
                outcode += framecode
                frame_flag = 0
                # if this is the last frame of its value, flush. frame_flag=0 after.
            
            
        outcode += "};\n"
        with open('%s.qsprite' % animname, 'w') as wfile:
            wfile.write(outcode)
            pass
        print outcode
