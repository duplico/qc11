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
        outcode = "// Auto-generated animation %s\n" % animname
        outcode += "const spriteframe anim_%s[] = {" % animname
        for frame in animation:
            assert len(frame) in [5, 6]
            outcode += "{"
            for row in frame[4::-1]:
                outcode += "0b" + ''.join(map(str,row)) + ', '
            if len(frame) == 6:
                frame_flag = frame[-1]
            else:
                frame_flag = 0
            if frame is animation[-1]:
                frame_flag += 8
            outcode += "%d}," % frame_flag
        outcode += "};\n"
        with open('%s.qsprite' % animname, 'w') as wfile:
            wfile.write(outcode)
        print outcode
