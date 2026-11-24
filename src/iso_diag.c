/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

/*
 * Health of the isochronous streams, and a guess whether we run in a virtual
 * machine. A Kinect stream that delivers (almost) only empty packets usually
 * means that the USB path can't carry isochronous transfers: a VM's USB
 * passthrough (EHCI/xHCI emulation, usbip in WSL2), or a troublesome
 * controller. See "Troubleshooting" in README.md.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "freenect_internal.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

fn_iso_verdict fn_iso_diag_add(fn_iso_diag *d, uint64_t now_ms, int pkts, int empty, int failed)
{
	fn_iso_verdict v = FN_ISO_OK;
	if (d->window_start_ms == 0)
		d->window_start_ms = now_ms ? now_ms : 1;
	d->pkts += (uint32_t)pkts;
	d->empty += (uint32_t)empty;
	d->failed += (uint32_t)failed;
	if (now_ms - d->window_start_ms < FN_ISO_WINDOW_MS)
		return FN_ISO_OK;

	// A window is complete. The first one is a warm-up: the camera can take a
	// moment before it sends data after a stream starts.
	if (d->windows++ > 0 && d->pkts > 0) {
		if ((uint64_t)d->empty * 100 >= (uint64_t)d->pkts * FN_ISO_NO_DATA_PERCENT)
			v = FN_ISO_NO_DATA;
		else if ((uint64_t)d->failed * 100 >= (uint64_t)d->pkts * FN_ISO_LOSS_PERCENT)
			v = FN_ISO_HEAVY_LOSS;
	}
	d->last_pkts = d->pkts;
	d->last_empty = d->empty;
	d->last_failed = d->failed;
	d->pkts = d->empty = d->failed = 0;
	d->window_start_ms = now_ms;
	if (v != FN_ISO_OK) {
		// Report each kind of problem once per stream start.
		if (d->reported & (1 << v))
			return FN_ISO_OK;
		d->reported |= (1 << v);
	}
	return v;
}

#if !defined(__APPLE__) /* macOS asks the kernel instead (kern.hv_vmm_present) */
// Case-insensitive substring search.
static int contains_ci(const char *haystack, const char *needle)
{
	size_t n = strlen(needle), i, j;
	for (i = 0; haystack[i]; i++) {
		for (j = 0; j < n && haystack[i + j]; j++)
			if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j]))
				break;
		if (j == n)
			return 1;
	}
	return 0;
}

// Does this system vendor / product string name a hypervisor?
static int names_vm(const char *s, char *name, int size)
{
	static const char *const vms[] = {
		"QEMU", "KVM", "VMware", "VirtualBox", "innotek", "Parallels", "Xen",
		"Bochs", "BHYVE", "Virtual Machine" /* Hyper-V guests */, "UTM", NULL
	};
	int i;
	for (i = 0; vms[i]; i++) {
		if (contains_ci(s, vms[i])) {
			snprintf(name, (size_t)size, "%s", s);
			return 1;
		}
	}
	return 0;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static void read_line(const char *path, char *buf, int size)
{
	FILE *f = fopen(path, "r");
	buf[0] = '\0';
	if (!f)
		return;
	if (fgets(buf, size, f))
		buf[strcspn(buf, "\r\n")] = '\0';
	fclose(f);
}
#endif

int fn_detect_vm(char *name, int size)
{
	char vendor[128], product[128];
	name[0] = '\0';
#if defined(_WIN32)
	DWORD len = sizeof(vendor);
	vendor[0] = product[0] = '\0';
	if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer",
	                 RRF_RT_REG_SZ, NULL, vendor, &len) != ERROR_SUCCESS)
		vendor[0] = '\0';
	len = sizeof(product);
	if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName",
	                 RRF_RT_REG_SZ, NULL, product, &len) != ERROR_SUCCESS)
		product[0] = '\0';
#elif defined(__APPLE__)
	int vmm = 0;
	size_t len = sizeof(vmm);
	(void)vendor; (void)product;
	if (sysctlbyname("kern.hv_vmm_present", &vmm, &len, NULL, 0) == 0 && vmm) {
		snprintf(name, (size_t)size, "macOS virtual machine");
		return 1;
	}
	return 0;
#else
	read_line("/proc/sys/kernel/osrelease", vendor, sizeof(vendor));
	if (contains_ci(vendor, "microsoft") || contains_ci(vendor, "WSL")) {
		snprintf(name, (size_t)size, "WSL2");
		return 1;
	}
	read_line("/sys/class/dmi/id/sys_vendor", vendor, sizeof(vendor));
	read_line("/sys/class/dmi/id/product_name", product, sizeof(product));
#endif
#if !defined(__APPLE__)
	return names_vm(vendor, name, size) || names_vm(product, name, size);
#endif
}
