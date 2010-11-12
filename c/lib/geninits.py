import struct, sys

fd = open(sys.argv[2], "w")

count = 0;

fd.write('#include "cameras.h"')
fd.write("\n");
fd.write("const struct caminit inits[] = {")
for line in open(sys.argv[1]):
    line = line.replace("\n","")
    if line == "":
        continue
    if line[0] == "#":
        continue
    cmd,tag,cdata,rdata = line.split(",")
    cmd = int(cmd,16)
    tag = int(tag,16)

    cdata = cdata.decode('hex')
    rdata = rdata.decode('hex')

    hcdata = ", ".join(["0x%02x"%ord(x) for x in cdata])
    hrdata = ", ".join(["0x%02x"%ord(x) for x in rdata])

    fd.write("\t{")
    fd.write("\t\t0x%02x, 0x%04x, %d, %d,"%(cmd, tag, len(cdata), len(rdata)))
    fd.write("\t\t{%s},"%hcdata)
    fd.write("\t\t{%s},"%hrdata)
    fd.write("\t},")
    count += 1
fd.write("};")
fd.write("\n");
fd.write("const int num_inits = %d;"%count);
fd.write("\n");