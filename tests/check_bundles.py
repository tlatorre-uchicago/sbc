import struct
import sys
from collections import defaultdict

MEGA_BUNDLE_ID = 0xCC

bundles = defaultdict(list)

with open(sys.argv[1]) as f:
    while True:
        mega_bundle = f.read(1444+4)
        if not mega_bundle:
            break
        num, type, num_bundles, xl3 = struct.unpack('>HBBB',mega_bundle[4:9])
        if type != MEGA_BUNDLE_ID:
            sys.exit("type = %i" % type)
        bundles[xl3] += [num]

for i in range(20):
    print 'checking bundles from XL3 %i...' % i,
    try:
        assert bundles[i] == range(1000)
    except AssertionError:
        print 'error'
        print bundles[i]
    else:
        print 'ok'
