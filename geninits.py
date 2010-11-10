#!/usr/bin/python
import struct

print '#include "cameras.h"'
print
print "const struct caminit inits[] = {"
for line in open("inits.txt"):
	line = line.replace("\n","")
	if line == "":
		continue
	cmd,tag,cdata,rdata = line.split(",")
	cmd = int(cmd,16)
	tag = int(tag,16)

	cdata = cdata.decode('hex')
	rdata = rdata.decode('hex')

	hcdata = ", ".join(["0x%02x"%ord(x) for x in cdata])
	hrdata = ", ".join(["0x%02x"%ord(x) for x in rdata])

	print "\t{"
	print "\t\t0x%02x, 0x%04x, %d, %d,"%(cmd, tag, len(cdata), len(rdata))
	print "\t\t{%s},"%hcdata
	print "\t\t{%s},"%hrdata
	print "\t},"
print "};"
