import os
import json
import re
import ast

infiles = [f for f in os.listdir('.') if os.path.isfile(f) and f.endswith('.jsprite')]

for f in infiles:
    with open(f, 'r') as jspritefile:
        jscode = jspritefile.read()
    values = re.findall(r'var.*?=\s*(.*?);', jscode, re.DOTALL | re.MULTILINE)
    for value in values:
        animation = ast.literal_eval(value)
        animname = f.split('.')[0]
        outcode = "// Auto-generated animation %s\n" % animname
        outcode += "spriteframe anim_%s[] = {" % animname
        for frame in animation:
            assert len(frame) == 6
            outcode += "{"
            for i in range(8):
                outcode+= "0b000"
                for row in frame[-2::-1]:
                    outcode += str(row[i])
                outcode += ', '
            frame_flag = frame[-1]
            if frame is animation[-1]:
                frame_flag += 8
            outcode += "%d}," % frame_flag
        outcode += "};\n"
        with open('%s.qsprite' % animname, 'w') as wfile:
            wfile.write(outcode)