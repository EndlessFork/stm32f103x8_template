from sys import stdin

loadsize = 0
for l in stdin:
    if ':' in l:
        print l
        continue
    if l.startswith('section'):
        print l.strip(), '      final'
        continue
    if l.startswith('Total'):
        continue
    if len(l) < 9:
        continue
    sn, sz, addr = l.split('0x')
    sz = int(sz, 16)
    addr = int(addr, 16)
    if addr and sz:
        print "%15s%2s %12s %12s" % (sn, hex(sz), hex(addr), hex(addr+sz-1))
        loadsize += sz
if loadsize:
    print "Total load size:", hex(loadsize), '(%d Bytes)' % loadsize
