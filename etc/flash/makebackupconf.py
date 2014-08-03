import sys
import os

def main():
    id = int(sys.argv[1])
    handle = sys.argv[2]
    message = sys.argv[3]
    
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
    contents = ''
    contents += '@1900\n'
    i=0
    for v in out:
        i+=1
        contents += v
        if i%16:
            contents += ' '
        else:
            contents += '\n'
    contents += '\nq\n'
    
    with open('conf%d.hex' % id, 'w') as f:
        f.write(contents)

if __name__ == "__main__":
    main()