#!/usr/bin/python

#   libfreenect - an open source Kinect driver
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2 or version 3;
# see:
#  http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
#  http://www.gnu.org/licenses/gpl-3.0.txt

import struct, sys

fd = open(sys.argv[2], "w")

count = 0;

print >>fd, '#include "freenect_internal.h"'
print >>fd
print >>fd, "const struct caminit inits[] = {"
for line in open(sys.argv[1]):
	line = line.replace("\n","").replace("\r","")
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

	print >>fd, "\t{"
	print >>fd, "\t\t0x%02x, 0x%04x, %d, %d,"%(cmd, tag, len(cdata), len(rdata))
	print >>fd, "\t\t{%s},"%hcdata
	print >>fd, "\t\t{%s},"%hrdata
	print >>fd, "\t},"
	count += 1
print >>fd, "};"
print >>fd
print >>fd, "const int num_inits = %d;"%count;
print >>fd