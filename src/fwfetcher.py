#!/usr/bin/env python

from __future__ import absolute_import, division, print_function

import hashlib
import io
import os
import struct
import sys
import time
import zipfile

try:
    # Python 3
    from urllib.request import Request, URLError, urlopen
except ImportError:
    # Python 2
    from urllib2 import Request, URLError, urlopen

# fwfetcher.py - a program to extract the Kinect audio firmware from an Xbox360
# system update.  This program includes substantial portions of extract360.py,
# which is copyright Rene Ladan and others as noted below and provided under
# the BSD 2-clause license.

"""Program to extract typical XBox 360 files.
   It can handle LIVE/PIRS, CON (partially), FMIM, and XUIZ files.

   What about CRA (aka .arc) files?  (Dead Rising demo)

   Copyright (c) 2007, 2008, Rene Ladan <r.c.ladan@gmail.com>, 2-claused BSD
   license. Portions from various contributors as mentioned in-source.

   Note that it dumps UTF-16 characters in text strings as-is.
"""


# Minor compatibility shim
if sys.version_info[0] < 3:
    input = raw_input
    range = xrange

###############################################################################

def check_size(fsize, minsize):
    """Ensure that the filesize is at least minsize bytes.

       @param fsize the filesize
       @param minsize the minimal file size
       @return fsize >= minsize
    """

    if fsize < minsize:
        print("Input file too small: %i instead of at least %i bytes." % (
            fsize, minsize))
    return fsize >= minsize

###############################################################################

def nice_open_file(filename):
    """Checks if the output file with the given name already exists,
       and if so, asks for overwrite permission.

       @param filename name of the output file to open
       @return overwrite permission
    """

    if os.path.isfile(filename):
        print(filename, "already exists, overwrite? (y/n)", end=' ')
        answer = input("")
        return len(answer) > 0 and answer[0] in ["Y", "y"]
    else:
        return True

###############################################################################

def nice_open_dir(dirname):
    """Checks if the output directory with the given name already exists,
       and if so, asks for overwrite permission.  This means that any file
       in that directory might be overwritten.

       @param dirname name of the output directory to open
       @return overwrite permission
    """

    if os.path.isdir(dirname):
        print(dirname, "already exists, ok to overwrite files in it? (y/n)",
              end=' ')
        answer = input("")
        return len(answer) > 0 and answer[0] in ["Y", "y"]
    else:
        return True

###############################################################################

def do_mkdir(dirname):
    """Version of os.mkdir() which does not throw an exception if the directory
       already exists.

       @param dirname name of the directory to create
    """

    try:
        os.mkdir(dirname)
    except OSError as e:
        if e.errno == 17:
            pass  # directory already exists

###############################################################################

def strip_blanks(instring):
    """Strip the leading and trailing blanks from the input string.
       Blanks are: 0x00 (only trailing) space \t \n \r \v \f 0xFF

       @param instring the input string
       @return stripped version of instring
    """

    rstr = instring.rstrip(b"\0 \t\n\r\v\f\377")
    return rstr.lstrip(b" \t\n\r\v\f\377")

###############################################################################

def open_info_file(infile):
    """Open the informational text file.
       The name is based on that of the input file.

       @param infile pointer to the input file
       @return pointer to the informational text file or None if there was no
               overwrite permission
    """

    txtname = os.path.basename(infile.name) + ".txt"
    if nice_open_file(txtname):
        print("Writing information file", txtname)
        txtfile = open(txtname, "w")
        return txtfile
    else:
        return None

###############################################################################

def dump_png(infile, pnglen, maxlen, pngid):
    """Dump the embedded PNG file from the archive file to an output file.

       @param infile pointer to the archive file
       @param pnglen size of the PNG file in bytes
       @param maxlen maximum size of the PNG file in bytes
       @param pngid indicates if this is the first or second PNG file.
    """

    # dump PNG icon
    if pnglen <= maxlen:
        outname = os.path.basename(infile.name) + "_" + pngid + ".png"
        if nice_open_file(outname):
            buf = infile.read(pnglen)
            print("Writing PNG file", outname)
            with open(outname, "wb") as outfile:
                print(buf, end=' ', file=outfile)
    else:
        print("PNG image %s too large (%i instead of maximal %i bytes), "
              "file not written." % (pngid, pnglen, maxlen))

###############################################################################

def dump_info(infile, txtfile, what):
    """Dumps the 9 information strings from the input file.

       @param infile pointer to the input file
       @param txtfile pointer to the resulting text file
       @param what indicates if the information consists of titles or
              descriptions
    """

    print("\n", what, ":", file=txtfile)
    for i in range(9):
        info = strip_blanks(infile.read(0x100))
        if len(info) > 0:
            print(lang[i], ":", info, file=txtfile)

###############################################################################

def mstime(intime):
    """Convert the time given in Microsoft format to a normal time tuple.

       @param intime the time in Microsoft format
       @return the time tuple
    """

    num_d = (intime & 0xFFFF0000) >> 16
    num_t = intime & 0x0000FFFF
    # format below is : year, month, day, hour, minute, second,
    #                   weekday (Monday), yearday (unused), DST flag (guess)
    return ((num_d >> 9) + 1980, (num_d >> 5) & 0x0F, num_d & 0x1F,
            (num_t & 0xFFFF) >> 11, (num_t >> 5) & 0x3F, (num_t & 0x1F) * 2,
            0, 0, -1)

###############################################################################

def do_utime(targetname, atime, mtime):
    """Set the access and update date/time of the target.
       Taken from tarfile.py (builtin lib)

       @param targetname name of the target
       @param atime the desired access date/time
       @param mtime the desired update date/time
    """

    if not hasattr(os, "utime"):
        return
    if not (sys.platform == "win32" and os.path.isdir(targetname)):
        # Using utime() on directories is not allowed on Win32 according to
        # msdn.microsoft.com
        os.utime(targetname,
                 (time.mktime(mstime(atime)), time.mktime(mstime(mtime))))

###############################################################################

def check_sha1(sha1, entry, infile, start, end):
    """Check the SHA1 value of the specified range of the input file.

       @param sha1 the reported SHA1 value
       @param entry the id of the hash
       @param infile the input file to check
       @param start the start position
       @param end the end position
       @return string reporting if the hash is correct
    """

    infile.seek(start)
    found_sha1 = hashlib.sha1(infile.read(end - start))
    found_digest = found_sha1.digest()
    # SHA1 hashes are 20 bytes (160 bits) long
    ret = "SHA1 " + hex(entry) + " "
    if found_digest == sha1:
        return ret + "ok (" + found_sha1.hexdigest() + ")"
    else:
        hexdig = ""
        for i in sha1:
            if ord(i) < 10:
                val = "0"
            else:
                val = ""
            val += hex(ord(i))[2:]
            hexdig += val
        return ret + "wrong (should be " + hexdig + " actual " + \
            found_sha1.hexdigest() + ")"

###############################################################################

def get_cluster(startclust, offset):
    """get the real starting cluster"""
    rst = 0
    # BEGIN wxPirs
    while startclust >= 170:
        startclust //= 170
        rst += (startclust + 1) * offset
    # END wxPirs
    return rst

###############################################################################

def fill_directory(infile, txtfile, contents, firstclust, makedir, start,
                   offset):
    """Fill the directory structure with the files contained in the archive.

       @param infile pointer to the archive
       @param txtfile pointer to the resulting information text file
       @param contents contains the directory information
       @param firstclust address of the starting cluster of the first file in
              infile (in 4kB blocks, minus start bytes)
       @param makedir flag if directory should be filled, useful if only return
              is wanted
       @param start start of directory data
       @param offset increment for calculating real starting cluster
    """

    # dictionary which holds the directory structure,
    # patch 0xFFFF is the 'root' directory.
    paths = {0xFFFF: ""}

    oldpathind = 0xFFFF  # initial path, speed up file/dir creation

    for i in range(0x1000 * firstclust // 64):
        cur = contents[i * 64:(i + 1) * 64]
        if ord(cur[40:41]) == 0:
            # if filename length is zero, we're done
            break
        (outname, namelen, clustsize1, val1, clustsize2, val2, startclust,
            val3) = struct.unpack("<40sBHBHBHB", cur[0:50])
        # sizes and starting cluster are 24 bits long
        clustsize1 += val1 << 16
        clustsize2 += val2 << 16
        startclust += val3 << 16
        (pathind, filelen, dati1, dati2) = struct.unpack(">HLLL", cur[50:64])

        if not makedir:
            continue

        nlen = namelen & ~0xC0
        if nlen < 1 or nlen > 40:
            print("Filename length (%i) out of range, skipping file." % nlen)
            continue
        outname = outname[0:nlen]  # strip trailing 0x00 from filename

        if txtfile is not None:
            if namelen & 0x80 == 0x80:
                print("Directory", end=' ', file=txtfile)
            else:
                print("File", end=' ', file=txtfile)
            print("name:", outname, file=txtfile)
            if namelen & 0x40 == 0x40:
                print("Bit 6 of namelen is set.", file=txtfile)

        if clustsize1 != clustsize2:
            print("Cluster sizes don't match (%i != %i), skipping file." % (
                clustsize1, clustsize2))
            continue
        if startclust < 1 and namelen & 0x80 == 0:
            print("Starting cluster must be 1 or greater, skipping file.")
            continue
        if filelen > 0x1000 * clustsize1:
            print("File length (%i) is greater than the size in clusters "
                  "(%i), skipping file." % (filelen, clustsize1))
            continue

        if pathind != oldpathind:
            # working directory changed
            for _ in range(paths[oldpathind].count("/")):
                os.chdir("..")  # go back to root directory
            os.chdir(paths[pathind])
            oldpathind = pathind
        if namelen & 0x80 == 0x80:
            # this is a directory
            paths[i] = paths[pathind] + outname + "/"
            do_mkdir(outname)
        else:
            # this is a file
            # space between files is set to 0x00
            adstart = startclust * 0x1000 + start
            if txtfile is not None:
                print("Starting: advertized", hex(adstart), file=txtfile)

            # block reading algorithm originally from wxPirs
            buf = b""
            while filelen > 0:
                realstart = adstart + get_cluster(startclust, offset)
                infile.seek(realstart)
                buf += infile.read(min(0x1000, filelen))
                startclust += 1
                adstart += 0x1000
                filelen -= 0x1000
            with open(outname, "wb") as outfile:
                outfile.write(buf)

        do_utime(outname, dati2, dati1)

    # pop directory
    for _ in range(paths[oldpathind].count("/")):
        os.chdir("..")

###############################################################################

def write_common_part(infile, txtfile, png2stop, start):
    """Writes out the common part of PIRS/LIVE and CON files.

       @param infile pointer to the PIRS/LIVE or CON file
       @param txtfile pointer to the resulting text file
       @param png2stop location where the second PNG image stops
                  (PIRS/LIVE : 0xB000, CON : 0xA000)
       @param start start of directory data, from wxPirs
    """

    infile.seek(0x32C)
    # xbox180 : SHA1 hash of 0x0344-0xB000,
    # CON : 0x0344 - 0xA000 (i.e. png2stop)
    mhash = infile.read(20)
    (mentry_id, content_type) = struct.unpack(">LL", infile.read(8))

    if txtfile is not None:
        print("\nMaster SHA1 hash :",
              check_sha1(mhash, mentry_id, infile, 0x0344, png2stop),
              file=txtfile)
        print("\nContent type", hex(content_type), ":", end=' ', file=txtfile)
        # content type list partially from V1kt0R
        # su20076000_00000000 has type 0x000b0000,
        # i.e. "Full game demo" & "Theme" ...
        if content_type == 0:
            print("(no type)", file=txtfile)
        elif content_type & 0x00000001:
            print("Game save", file=txtfile)
        elif content_type & 0x00000002:
            print("Game add-on", file=txtfile)
        elif content_type & 0x00030000:
            print("Theme", file=txtfile)
        elif content_type & 0x00090000:
            print("Video clip", file=txtfile)
        elif content_type & 0x000C0000:
            print("Game trailer", file=txtfile)
        elif content_type & 0x000D0000:
            print("XBox Live Arcade", file=txtfile)
        elif content_type & 0x00010000:
            print("Gamer profile", file=txtfile)
        elif content_type & 0x00020000:
            print("Gamer picture", file=txtfile)
        elif content_type & 0x00040000:
            print("System update", file=txtfile)
        elif content_type & 0x00080000:
            print("Full game demo", file=txtfile)
        else:
            print("(unknown)", file=txtfile)

        print("\nDirectory data at (hex)", hex(start), file=txtfile)
        infile.seek(0x410)
        dump_info(infile, txtfile, "Titles")
        dump_info(infile, txtfile, "Descriptions")
        print("\nPublisher:", strip_blanks(infile.read(0x80)), "\n",
              file=txtfile)
        print("\nFilename:", strip_blanks(infile.read(0x80)), "\n",
              file=txtfile)
    infile.seek(0x1710)
    (val1, png1len, png2len) = struct.unpack(">HLL", infile.read(10))
    if txtfile is not None:
        print("Value:", val1, file=txtfile)

    if png1len > 0:
        dump_png(infile, png1len, 0x571A - 0x171A, "1")

    if png2len > 0:
        infile.seek(0x571A)
        dump_png(infile, png2len, png2stop - 0x571A, "2")

    # entries are 64 bytes long
    # unused entries are set to 0x00
    infile.seek(start + 0x2F)
    (firstclust, ) = struct.unpack("<H", infile.read(2))

    infile.seek(start)
    buf = infile.read(0x1000 * firstclust)

    outname = os.path.basename(infile.name) + ".dir"
    makedir = nice_open_dir(outname)
    if makedir:
        print("Creating and filling content directory", outname)
        do_mkdir(outname)
        os.chdir(outname)
    if png2stop == 0xB000 and start == 0xC000:
        offset = 0x1000
    else:
        offset = 0x2000
    fill_directory(infile, txtfile, buf, firstclust, makedir, start, offset)

    # table of SHA1 hashes of payload
    if txtfile is not None:
        print(file=txtfile)
        infile.seek(png2stop)
        buf = infile.read(start - png2stop)
        numempty = 0
        for i in range(len(buf) // 24):
            entry = buf[i * 24: i * 24 + 24]
            if entry.count(b"\0") < 24:
                if numempty > 0:
                    print("\nEmpty entries:", numempty, file=txtfile)
                    numempty = 0
                print("Hash (hex):", end=' ', file=txtfile)
                for j in range(20):
                    print(hex(ord(entry[j:j + 1])), end=' ', file=txtfile)
                (j, ) = struct.unpack(">L", entry[20:24])
                print("\nEntry id:", hex(j), file=txtfile)
            else:
                numempty += 1

        print("\nTrailing data (hex):", end=' ', file=txtfile)
        for i in range(len(buf) - (len(buf) % 24), len(buf)):
            print(hex(ord(buf[i:i + 1])), end=' ', file=txtfile)
        print(file=txtfile)

        txtfile.close()

###############################################################################

def handle_live_pirs(infile, fsize):
    """LIVE and PIRS files are archive files.
       They contain a certificate, payload, SHA1 checksums,
       2 icons, textual information, and the files themselves.

       @param infile pointer to the archive file
       @param fsize size of infile
    """

    print("Handling LIVE/PIRS file.")

    if not check_size(fsize, 0xD000):
        return

    txtfile = open_info_file(infile)
    if txtfile is not None:
        print("Certificate (hex):", end=' ', file=txtfile)
        cert = infile.read(0x100)
        for i in range(len(cert)):
            print(hex(ord(cert[i:i + 1])), end=' ', file=txtfile)

        print("\n\nData (hex):", end=' ', file=txtfile)
        data = infile.read(0x228)
        for i in range(len(data)):
            print(hex(ord(data[i:i + 1])), end=' ', file=txtfile)
        print(file=txtfile)

    # BEGIN wxPirs
    infile.seek(0xC032)  # originally 4 bytes at 0xC030
    (pathind, ) = struct.unpack(">H", infile.read(2))
    if pathind == 0xFFFF:
        start = 0xC000
    else:
        start = 0xD000
    # END wxPirs
    write_common_part(infile, txtfile, 0xB000, start)

###############################################################################

# End of code taken from extract360.py.

def urlopen_timeout_retry(request, attempts = 5):
    import socket

    last_error = None
    for attempt in range(attempts):
        try:
            return urlopen(request)
        except URLError as e:
            if isinstance(e.reason, socket.timeout):
                print("Timeout! ", e)
            else: raise
            last_error = e
        except socket.timeout as e:
            print("Timeout! ", e)
            last_error = e
    if last_error:
        raise last_error

def getFileOrURL(filename, url):
    # Check if a file named filename exists on disk.
    # If so, return its contents.  If not, download it, save it, and return its
    # contents.
    try:
        with open(filename, 'rb') as f:
            print("Found", filename, "cached on disk, using local copy")
            retval = f.read()
        return retval
    except IOError:
        pass

    print("Downloading", filename, "from", url)
    request = Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    response = urlopen_timeout_retry(request)

    print("Reading response...")
    retval = response.read()
    # Save downloaded file to disk
    with open(filename, "wb") as f:
        f.write(retval)
    print("done, saved to", filename)
    return retval

def extractPirsFromZip(systemupdate):
    print("Extracting $SystemUpdate/FFFE07DF00000001 from system update "
          "file...")
    updatefile = io.BytesIO(systemupdate)
    with zipfile.ZipFile(updatefile) as z:
        # print(z.namelist())
        pirs = z.open("$SystemUpdate/FFFE07DF00000001").read()
    print("done.")
    return pirs

###############################################################################
# --uac: Microsoft's audio firmware (UAC.BIN, installed as UACFirmware.bin)
#
# It is fetched from Microsoft's own Kinect for Windows Runtime v1.8 installer
# on the user's machine; libfreenect does not redistribute it. The installer
# is a WiX Burn bundle: its payload is the second cabinet in the file, which
# holds the MSI packages; the x64 drivers MSI ("a0") holds a cabinet with the
# file "UACFirmware". All cabinets use MSZIP (deflate), so only the standard
# library is needed. The installer is never run.

UAC_RUNTIME_URL = ("https://download.microsoft.com/download/e/c/5/"
                   "ec50686b-82f4-4dbf-a922-980183b214e6/KinectRuntime-v1.8-Setup.exe")
UAC_RUNTIME_SHA256 = "f4d4143fb0f0a8d276889c077bfc8af42bfe99c128cadab5e316bf015a9858e9"
UAC_FIRMWARE_SIZE = 145920
UAC_FIRMWARE_SHA256 = "4467ae36ad378c58477432729d74eed0f9d45d35213f4430a781d90f64cea3f9"


def cab_extract(cab, wanted):
    """Return the contents of file `wanted` from cabinet bytes `cab` (MSZIP or stored)."""
    import zlib
    if cab[:4] != b"MSCF":
        raise ValueError("not a cabinet")
    coff_files, = struct.unpack_from("<I", cab, 16)
    n_folders, n_files, flags = struct.unpack_from("<HHH", cab, 26)
    pos = 36
    cb_folder_res = cb_data_res = 0
    if flags & 4:  # reserve fields present
        cb_header_res, cb_folder_res, cb_data_res = struct.unpack_from("<HBB", cab, pos)
        pos += 4 + cb_header_res
    for flag in (1, 2):  # previous / next cabinet names: two strings each
        if flags & flag:
            for _ in range(2):
                pos = cab.index(b"\0", pos) + 1
    folders = []
    for _ in range(n_folders):
        start, n_data, comp = struct.unpack_from("<IHH", cab, pos)
        folders.append((start, n_data, comp & 0x0F))
        pos += 8 + cb_folder_res
    entry = None
    pos = coff_files
    for _ in range(n_files):
        size, offset, ifolder = struct.unpack_from("<IIH", cab, pos)
        end = cab.index(b"\0", pos + 16)
        name = cab[pos + 16:end].decode("latin-1")
        if name.lower() == wanted.lower():
            entry = (size, offset, ifolder)
        pos = end + 1
    if entry is None:
        raise KeyError(wanted)
    size, offset, ifolder = entry
    start, n_data, comp = folders[ifolder]
    if comp not in (0, 1):
        raise ValueError("unsupported cabinet compression %d" % comp)
    out = bytearray()
    pos = start
    for _ in range(n_data):
        cb_data, cb_uncomp = struct.unpack_from("<HH", cab, pos + 4)
        block = cab[pos + 8 + cb_data_res:pos + 8 + cb_data_res + cb_data]
        pos += 8 + cb_data_res + cb_data
        if comp == 0:
            out += block
        else:
            # MSZIP: "CK" + a raw deflate block whose window is the folder's previous output
            if block[:2] != b"CK":
                raise ValueError("bad MSZIP block")
            d = zlib.decompressobj(-15, zdict=bytes(out[-32768:]))
            out += d.decompress(block[2:]) + d.flush()
        if len(out) >= offset + size:
            break
    return bytes(out[offset:offset + size])


def cfb_cabinet_streams(data):
    """Yield the streams of an OLE compound file (an MSI) that are cabinets."""
    if data[:8] != b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
        raise ValueError("not an MSI")
    ssz = 1 << struct.unpack_from("<H", data, 30)[0]
    n_fat, dir_start = struct.unpack_from("<II", data, 44)
    cutoff, = struct.unpack_from("<I", data, 56)
    difat_start, n_difat = struct.unpack_from("<II", data, 68)

    def sector(s):
        return data[(s + 1) * ssz:(s + 2) * ssz]

    difat = list(struct.unpack_from("<109I", data, 76))
    s = difat_start
    for _ in range(n_difat):
        words = struct.unpack("<%dI" % (ssz // 4), sector(s))
        difat += words[:-1]
        s = words[-1]
    fat = []
    for s in difat[:n_fat]:
        fat += struct.unpack("<%dI" % (ssz // 4), sector(s))

    def chain(s, size=None):
        out = bytearray()
        for _ in range(len(fat)):
            if s >= 0xFFFFFFFA:
                break
            out += sector(s)
            s = fat[s]
        return bytes(out if size is None else out[:size])

    directory = chain(dir_start)
    for i in range(0, len(directory), 128):
        e = directory[i:i + 128]
        if e[66:67] == b"\x02":  # stream
            start, size = struct.unpack_from("<IQ", e, 116)
            if size >= cutoff:
                stream = chain(start, size)
                if stream[:4] == b"MSCF":
                    yield stream


def find_bundle_cabinet(installer):
    """The Burn bundle's payload: the second cabinet in the file."""
    offsets = []
    i = installer.find(b"MSCF\0\0\0\0")
    while i >= 0 and len(offsets) < 2:
        offsets.append(i)
        i = installer.find(b"MSCF\0\0\0\0", i + 1)
    if len(offsets) < 2:
        raise ValueError("the installer has no payload cabinet")
    start = offsets[1]
    size, = struct.unpack_from("<I", installer, start + 8)
    return installer[start:start + size]


def read_runtime_installer(installer_path, what):
    """The Runtime installer's bytes: read from INSTALLER, or downloaded from Microsoft."""
    if sys.version_info < (3, 3):
        sys.exit("this needs Python 3.3 or newer")
    if installer_path:
        print("Reading", installer_path)
        with open(installer_path, "rb") as f:
            installer = f.read()
    else:
        print("Microsoft's licence terms for the Kinect for Windows Runtime apply to %s." % what)
        print("Downloading Microsoft's Kinect for Windows Runtime v1.8 (111 MB) from download.microsoft.com")
        response = urlopen_timeout_retry(Request(UAC_RUNTIME_URL, headers={"User-Agent": "libfreenect-fwfetcher"}))
        installer = response.read()
    if hashlib.sha256(installer).hexdigest() != UAC_RUNTIME_SHA256:
        print("Note: the installer differs from the known one (Microsoft re-signs its downloads);"
              " what is extracted from it is checked instead.")
    return installer


def runtime_file(installer, msi_name, file_name):
    """File `file_name` of MSI `msi_name` in the Runtime bundle, or None."""
    msi = cab_extract(find_bundle_cabinet(installer), msi_name)
    for cab in cfb_cabinet_streams(msi):
        try:
            return cab_extract(cab, file_name)
        except KeyError:
            pass
    return None


def fetch_uac_firmware(target, installer_path=None):
    installer = read_runtime_installer(installer_path, "this firmware")
    import zlib
    firmware = None
    try:
        firmware = runtime_file(installer, "a0", "UACFirmware")
    except (ValueError, KeyError, IndexError, struct.error, zlib.error) as e:
        sys.exit("error: the installer could not be unpacked (%s: %s); not installing anything"
                 % (type(e).__name__, e))
    if firmware is None:
        sys.exit("error: the installer does not contain UACFirmware")
    if len(firmware) != UAC_FIRMWARE_SIZE or hashlib.sha256(firmware).hexdigest() != UAC_FIRMWARE_SHA256:
        sys.exit("error: the extracted firmware does not match the known UAC.BIN; not installing it")
    with open(target, "wb") as f:
        f.write(firmware)
    print("Wrote", target, "(%d bytes, SHA-256 verified)" % len(firmware))


###############################################################################
# --audio-weights: Microsoft's beamformer weights for libfreenect's audio DSP
#
# KinectAudio10.dll (x64, in the Runtime's MSI "a1") holds, for the Kinect's
# array, 11 beams x 256 frequencies x 4 microphones of complex weights and
# steering vectors (reference §40.6.3). They are read from the user's own
# download and written to kinect_beam_weights.bin, the file
# freenect_dsp_load_sdk_weights() loads; libfreenect does not contain them.

AUDIO_WEIGHTS_FILE = "kinect_beam_weights.bin"
KA10_WEIGHTSET_VA = 0x180070098   # the "Kinect" beam set in KinectAudio10.dll v1.8 x64
AUDIO_WEIGHTS_SIZE = 24 + 11 * 8 + 2 * 11 * 256 * 4 * 8 + 4   # 180340
AUDIO_WEIGHTS_SHA256 = "a56ddba443684ba9588dc44e21cc499fd4b51bafc592fff086e4b40d4250e5e7"


class PE(object):
    """Just enough of a PE32+ image to read data at virtual addresses."""

    def __init__(self, data):
        self.d = data
        if data[:2] != b"MZ":
            raise ValueError("not a PE image")
        pe, = struct.unpack_from("<I", data, 0x3C)
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("not a PE image")
        machine, nsec = struct.unpack_from("<HH", data, pe + 4)
        optsize, = struct.unpack_from("<H", data, pe + 20)
        opt = pe + 24
        if machine != 0x8664 or struct.unpack_from("<H", data, opt)[0] != 0x20B:
            raise ValueError("not an x64 image")
        self.base, = struct.unpack_from("<Q", data, opt + 24)
        self.sections = []
        for i in range(nsec):
            s = opt + optsize + 40 * i
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, s + 8)
            self.sections.append((va, max(vsize, rawsize), rawptr, rawsize))

    def read(self, va, n):
        rva = va - self.base
        for sva, size, ptr, raw in self.sections:
            if sva <= rva and rva + n <= sva + size:
                o = rva - sva
                if o + n > raw:
                    raise ValueError("data outside the file")
                return self.d[ptr + o:ptr + o + n]
        raise ValueError("address 0x%x not in the image" % va)


def beam_weights_file(dll):
    """kinect_beam_weights.bin (layout in src/audio_dsp.c) from KinectAudio10.dll."""
    import math
    import zlib
    pe = PE(dll)
    w = KA10_WEIGHTSET_VA
    nm, nf, nb = struct.unpack("<iii", pe.read(w + 8, 12))
    if (nm, nf, nb) != (4, 256, 11):
        raise ValueError("unexpected beam set shape %r" % ((nm, nf, nb),))
    beams_va, freqs_va, weights_va, steer_va = struct.unpack("<QQQQ", pe.read(w + 0x18, 32))
    angles = []
    for b in range(nb):
        az, el, rng = struct.unpack("<ddd", pe.read(beams_va + 24 * b, 24))
        if abs(az - math.radians(-50 + 10 * b)) > 0.01 or el != 0.0 or abs(rng - 1.0) > 1e-6:
            raise ValueError("unexpected beam %d (%g, %g, %g)" % (b, az, el, rng))
        angles.append(az)
    freqs = struct.unpack("<%df" % nf, pe.read(freqs_va, 4 * nf))
    if any(abs(freqs[k] - 31.25 * k) > 1e-3 for k in range(nf)):
        raise ValueError("unexpected frequency grid")
    table = nb * nf * nm * 8
    out = b"FNBW" + struct.pack("<IIIII", 1, nm, nf, nb, 0) + struct.pack("<%dd" % nb, *angles)
    out += pe.read(weights_va, table) + pe.read(steer_va, table)
    out += struct.pack("<I", zlib.crc32(out) & 0xFFFFFFFF)
    return out


def fetch_audio_weights(target, installer_path=None, dll_path=None):
    import zlib
    try:
        if dll_path:
            print("Reading", dll_path)
            with open(dll_path, "rb") as f:
                dll = f.read()
        else:
            installer = read_runtime_installer(installer_path, "these beamformer weights")
            dll = runtime_file(installer, "a1", "FILE_x64_KinectAudio10.dll")
            if dll is None:
                sys.exit("error: the installer does not contain KinectAudio10.dll")
        data = beam_weights_file(dll)
    except (ValueError, KeyError, IndexError, struct.error, zlib.error) as e:
        sys.exit("error: the weights could not be extracted (%s: %s); not installing anything"
                 % (type(e).__name__, e))
    if len(data) != AUDIO_WEIGHTS_SIZE or (AUDIO_WEIGHTS_SHA256 and
                                          hashlib.sha256(data).hexdigest() != AUDIO_WEIGHTS_SHA256):
        sys.exit("error: the extracted weights do not match the known ones; not installing them")
    with open(target, "wb") as f:
        f.write(data)
    print("Wrote", target, "(%d bytes, SHA-256 verified)" % len(data))


def data_target(output, user, filename):
    """Where a fetched file goes. OUTPUT is a file if it ends in .bin and a
    directory otherwise (created if needed); --user means
    $XDG_DATA_HOME/libfreenect (~/.local/share/libfreenect), or on macOS
    ~/Library/Application Support/libfreenect."""
    if user:
        if sys.platform == "darwin":
            base = os.path.join(os.path.expanduser("~"), "Library", "Application Support")
        else:
            base = os.environ.get("XDG_DATA_HOME", "")
            if not base.startswith("/"):
                base = os.path.join(os.path.expanduser("~"), ".local", "share")
        output = os.path.join(base, "libfreenect")
    if output is None:
        return filename
    if output.lower().endswith(".bin"):
        return output
    if not os.path.isdir(output):
        os.makedirs(output)
    return os.path.join(output, filename)


###############################################################################
# --nui: the skeleton tracker's models for the optional libfreenect_nui addon
#
# Kinect10.dll (x64, in the Runtime's MSI "a1") carries the body-part forest
# and the seated-mode head forest as zlib-compressed resources of type BINARY
# (reference §30.2, §30.3). They are copied out unchanged into a "nui"
# folder next to the firmware; libfreenect does not contain them.

NUI_MODELS = {
    # name: (size, SHA-256) of the resource as stored in Kinect10.dll v1.8 x64
    "DATABASE.WSDF": (13795880, "b3d084bd5848ef76de4219f592922c3b1e5379e5ccc975ff8d1f68c426b41164"),
    "HEADPOSITION.MSHDF": (246487, "343e3c2a23d4c5baf73af05f232131c9d13a9264f5dacde182106121750535cf"),
}


def pe_resource(dll, type_name, name):
    """Resource `name` of type `type_name` (both strings) in a PE32+ image."""
    pe = PE(dll)
    p, = struct.unpack_from("<I", dll, 0x3C)
    opt = p + 24
    rva, size = struct.unpack_from("<II", dll, opt + 112 + 2 * 8)  # data directory 2
    if not rva:
        raise ValueError("no resources")
    root = pe.base + rva

    def entries(dir_va):
        named, ids = struct.unpack("<HH", pe.read(dir_va + 12, 4))
        for i in range(named + ids):
            n, off = struct.unpack("<II", pe.read(dir_va + 16 + 8 * i, 8))
            label = None
            if n & 0x80000000:
                s = root + (n & 0x7FFFFFFF)
                ln, = struct.unpack("<H", pe.read(s, 2))
                label = pe.read(s + 2, 2 * ln).decode("utf-16-le")
            yield label, off

    def find(dir_va, want):
        for label, off in entries(dir_va):
            if label is not None and label.upper() == want.upper():
                return off
        raise KeyError(want)

    off = find(root, type_name)
    off = find(root + (off & 0x7FFFFFFF), name)
    lang_dir = root + (off & 0x7FFFFFFF)
    _, off = next(entries(lang_dir))       # first language
    data_rva, data_size = struct.unpack("<II", pe.read(root + off, 8))
    return pe.read(pe.base + data_rva, data_size)


def fetch_nui_models(target_dir, installer_path=None, dll_path=None):
    import zlib
    try:
        if dll_path:
            print("Reading", dll_path)
            with open(dll_path, "rb") as f:
                dll = f.read()
        else:
            installer = read_runtime_installer(installer_path, "these skeleton tracking models")
            dll = runtime_file(installer, "a1", "FILE_x64_Kinect10.dll")
            if dll is None:
                sys.exit("error: the installer does not contain Kinect10.dll")
        models = {}
        for name, (size, sha) in NUI_MODELS.items():
            data = pe_resource(dll, "BINARY", name)
            if len(data) != size or hashlib.sha256(data).hexdigest() != sha:
                sys.exit("error: %s in Kinect10.dll is not the known model; not installing anything" % name)
            models[name] = data
    except (ValueError, KeyError, IndexError, struct.error, zlib.error) as e:
        sys.exit("error: the models could not be extracted (%s: %s); not installing anything"
                 % (type(e).__name__, e))
    if not os.path.isdir(target_dir):
        os.makedirs(target_dir)
    for name, data in models.items():
        path = os.path.join(target_dir, name)
        with open(path, "wb") as f:
            f.write(data)
        print("Wrote", path, "(%d bytes, SHA-256 verified)" % len(data))


if __name__ == "__main__" and len(sys.argv) >= 2 and sys.argv[1] == "--nui":
    # fwfetcher.py --nui [--from INSTALLER | --dll Kinect10.dll] [--user | DIR]
    #   DIR: where the models go (default ./nui); --user: the user's libfreenect/nui
    args = sys.argv[2:]
    installer_path = dll_path = output = None
    user = False
    while args:
        if args[0] in ("--from", "--dll") and len(args) >= 2:
            if args[0] == "--from":
                installer_path = args[1]
            else:
                dll_path = args[1]
            args = args[2:]
        elif args[0] == "--user":
            user = True
            args = args[1:]
        elif not args[0].startswith("-") and output is None:
            output = args[0]
            args = args[1:]
        else:
            sys.exit("usage: fwfetcher.py --nui [--from INSTALLER | --dll Kinect10.dll] [--user | DIR]")
    if user and output is not None:
        sys.exit("error: give either --user or DIR, not both")
    if user:
        target = os.path.join(os.path.dirname(data_target(None, True, "x")), "nui")
    else:
        target = output or "nui"
    fetch_nui_models(target, installer_path, dll_path)
    sys.exit(0)


if __name__ == "__main__" and len(sys.argv) >= 2 and sys.argv[1] == "--audio-weights":
    # fwfetcher.py --audio-weights [--from INSTALLER | --dll KinectAudio10.dll] [--user | OUTPUT]
    args = sys.argv[2:]
    installer_path = dll_path = output = None
    user = False
    while args:
        if args[0] in ("--from", "--dll") and len(args) >= 2:
            if args[0] == "--from":
                installer_path = args[1]
            else:
                dll_path = args[1]
            args = args[2:]
        elif args[0] == "--user":
            user = True
            args = args[1:]
        elif not args[0].startswith("-") and output is None:
            output = args[0]
            args = args[1:]
        else:
            sys.exit("usage: fwfetcher.py --audio-weights [--from INSTALLER | --dll KinectAudio10.dll] [--user | OUTPUT]")
    if user and output is not None:
        sys.exit("error: give either --user or OUTPUT, not both")
    try:
        target = data_target(output, user, AUDIO_WEIGHTS_FILE)
    except OSError as e:
        sys.exit("error: cannot create the output directory: %s" % e)
    fetch_audio_weights(target, installer_path, dll_path)
    sys.exit(0)


def uac_target(output, user):
    """Where --uac writes the firmware. OUTPUT is a file if it ends in .bin and
    a directory otherwise (created if needed, firmware named UACFirmware.bin);
    --user means $XDG_DATA_HOME/libfreenect (~/.local/share/libfreenect), or on
    macOS ~/Library/Application Support/libfreenect."""
    if user:
        if sys.platform == "darwin":
            base = os.path.join(os.path.expanduser("~"), "Library", "Application Support")
        else:
            base = os.environ.get("XDG_DATA_HOME", "")
            if not base.startswith("/"):
                base = os.path.join(os.path.expanduser("~"), ".local", "share")
        output = os.path.join(base, "libfreenect")
    if output is None:
        return "UACFirmware.bin"
    if output.lower().endswith(".bin"):
        return output
    if not os.path.isdir(output):
        os.makedirs(output)
    return os.path.join(output, "UACFirmware.bin")


if __name__ == "__main__" and len(sys.argv) >= 2 and sys.argv[1] == "--uac":
    # fwfetcher.py --uac [--from INSTALLER] [--user | OUTPUT]
    #   OUTPUT: a .bin file, or a directory to put UACFirmware.bin in
    #   (default: UACFirmware.bin in the current directory)
    args = sys.argv[2:]
    installer_path = None
    user = False
    output = None
    while args:
        if args[0] == "--from" and len(args) >= 2:
            installer_path = args[1]
            args = args[2:]
        elif args[0] == "--user":
            user = True
            args = args[1:]
        elif not args[0].startswith("-") and output is None:
            output = args[0]
            args = args[1:]
        else:
            sys.exit("usage: fwfetcher.py --uac [--from INSTALLER] [--user | OUTPUT]")
    if user and output is not None:
        sys.exit("error: give either --user or OUTPUT, not both")
    try:
        target = uac_target(output, user)
    except OSError as e:
        sys.exit("error: cannot create the output directory: %s" % e)
    fetch_uac_firmware(target, installer_path)
    sys.exit(0)

if __name__ == "__main__":
    target = "audios.bin"
    if len(sys.argv) == 2:
        target = sys.argv[1]
    if not os.path.isfile(target):
        fw = getFileOrURL("SystemUpdate.zip",
                          "https://www.xbox.com/system-update-usb")
        pirs = extractPirsFromZip(fw)

        lang = ["English", "Japanese", "German", "French", "Spanish",
                "Italian", "Korean", "Chinese", "Portuguese"]
        bio = io.BytesIO(pirs)
        basename = "FFFE07DF00000001"
        bio.name = basename
        pwd = os.getcwd()
        handle_live_pirs(bio, len(pirs) - 4)

        os.chdir(pwd)
        print("Moving audios.bin to current folder")
        os.rename(os.path.join(basename + ".dir", "audios.bin"), target)

        print("Cleaning up")
        os.unlink(basename + ".txt")
        for root, dirs, files in os.walk(basename + ".dir"):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
            os.rmdir(root)
        os.unlink("SystemUpdate.zip")
        print("Done!")
    else:
        print("Already have audios.bin")
