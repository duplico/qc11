"""
Script to generate arrays of RGB values to make rainbows.

Acknowledgment: <http://krazydad.com/tutorials/makecolors.php>
"""

import math

frequency = 0.3

for i in range(21):
    red = math.sin(frequency*i + 0) * 127+128
    green = math.sin(frequency*i + 2) * 127+128
    blue = math.sin(frequency*i+4) * 127+128
    print "{ 0x%x, 0x%x, 0x%x }," % (red/10, green/10, blue/10)
