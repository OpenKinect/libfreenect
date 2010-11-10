#!/usr/bin/python

#   libfreenect - an open source Kinect driver
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2 or version 3;
# see:
#  http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
#  http://www.gnu.org/licenses/gpl-3.0.txt

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
