import sys
import os

def main():
    txtfile = sys.argv[1]
    id = int(sys.argv[2])
    handle = sys.argv[3]
    message = sys.argv[4]
    clobber = False
    if len(sys.argv) > 5 and sys.argv[5] == 'clobber':
        clobber = True
    if len(handle) > 10:
        print "fail on handle length"
        exit(1)
    if len(message) > 16:
        print "fail on message length"
        exit(1)
    
    handle += '\0'*(11-len(handle))
    message += '\0' * (18-len(message))
    
    chars = []
    for i in range(50):
        chars.append(255)
    chars.append(id)
    chars += map(ord, handle)
    chars += map(ord, message)
    chars += [255, 255]
    out = map(lambda a: format(a, 'X').zfill(2), chars)
    contents = "@19d0\nFF FF\n" if clobber else ''
    contents += '@1900\n'
    i=0
    for v in out:
        i+=1
        contents += v
        if i%16:
            contents += ' '
        else:
            contents += '\n'
    if not contents.endswith('\n'):
        contents += '\n'
    
    outfile= [contents]
    skipping = False
    
    for line in open(txtfile, 'r'):
        if line.startswith('@1900'):
            skipping = True
        elif line.startswith('@'):
            skipping = False
        if not skipping:
            outfile.append(line)
        
    with open('temp%d.hex' % id, 'w') as f:
        f.write(''.join(outfile))
    
    cmd = 'MSP430Flasher.exe -n msp430f5308 -m SBW2 -w "temp%d.hex" -v' % id
    if clobber:
        cmd += " -u"
    
    os.system(cmd)

if __name__ == "__main__":
    main()