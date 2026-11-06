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
 * freenect-driver-install: bind the Kinect v1 USB functions to Microsoft's
 * in-box WinUSB driver, so libfreenect can use them through libusb.
 *
 * WinUSB (winusb.sys) ships with Windows and is signed by Microsoft, so this
 * works with Memory integrity (HVCI) enabled. Windows still requires the
 * driver *package* (INF + catalog) to be signed by a trusted publisher. This
 * tool therefore:
 *   1. writes an INF that only references the in-box WinUSB driver,
 *   2. builds a SHA-256 catalog (.cat) for it,
 *   3. signs the catalog with a freshly generated self-signed certificate and
 *      immediately destroys that certificate's private key, so the trusted
 *      certificate can never sign anything else,
 *   4. adds the certificate to the local machine's Root and TrustedPublisher
 *      stores, and installs the package for any Kinect that is plugged in.
 *
 * Only the interfaces libfreenect drives are bound. The microphone-array
 * interfaces (MI_02/MI_03 of 045E:02BB/02C3) stay with usbaudio.sys.
 *
 * Usage:
 *   freenect-driver-install --list
 *   freenect-driver-install --install        (Administrator)
 *   freenect-driver-install --uninstall      (Administrator)
 *   freenect-driver-install --package-only DIR
 *       Build and sign a package in DIR without changing the system.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <mscat.h>
#include <mssip.h>
#include <cryptuiapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define PACKAGE_BASENAME L"libfreenect-kinect"
#define CERT_SUBJECT_TAG L"libfreenect Kinect WinUSB"
#define CATALOG_OS_ATTR  L"2:6.2,2:6.3,2:10.0"

/* ---- Kinect v1 USB functions bound to WinUSB --------------------------- */

#define GUID_CAMERA   "{CF24629A-1276-4E8E-B1D5-ED9EE4E0AA92}" /* libfreenect's own */
#define GUID_MOTOR    "{E4CFCB17-3651-4BDA-8607-528812E60503}" /* same as the Kinect SDK */
#define GUID_AUDIO    "{F9DBE212-F689-4FDF-A75D-532E951FBD0A}" /* same as the Kinect SDK */
#define GUID_SECURITY "{69151671-6D3C-403E-B3D7-48438FB6F6CE}" /* same as the Kinect SDK */

typedef struct {
	const char *hwid;     /* hardware ID as it appears in the INF */
	const char *desc;     /* Device Manager name */
	const char *section;  /* INF install section */
} kinect_function;

static const kinect_function kinect_functions[] = {
	{ "USB\\VID_045E&PID_02AE",       "Xbox NUI Camera (libfreenect)",                    "Camera" },
	{ "USB\\VID_045E&PID_02BF",       "Kinect for Windows Camera (libfreenect)",          "Camera" },
	{ "USB\\VID_045E&PID_02B0",       "Xbox NUI Motor (libfreenect)",                     "Motor" },
	{ "USB\\VID_045E&PID_02AF",       "Kinect Motor (libfreenect)",                       "Motor" },
	/* Audio controller before firmware upload, and with the Xbox audios.bin
	 * firmware, which keeps this PID: libfreenect drives the whole device. */
	{ "USB\\VID_045E&PID_02AD",       "Xbox NUI Audio (libfreenect)",                     "Audio" },
	{ "USB\\VID_045E&PID_02BE",       "Kinect for Windows Audio (libfreenect)",           "Audio" },
	/* Audio controller running Microsoft's UAC firmware: only the control and
	 * security interfaces. MI_02/MI_03 are a standard USB Audio device. */
	{ "USB\\VID_045E&PID_02BB&MI_00", "Kinect Audio Array Control (libfreenect)",         "Audio" },
	{ "USB\\VID_045E&PID_02BB&MI_01", "Kinect Security Control (libfreenect)",            "Security" },
	{ "USB\\VID_045E&PID_02C3&MI_00", "Kinect for Windows Audio Array Control (libfreenect)", "Audio" },
	{ "USB\\VID_045E&PID_02C3&MI_01", "Kinect for Windows Security Control (libfreenect)",    "Security" },
};
#define NUM_FUNCTIONS (sizeof(kinect_functions) / sizeof(kinect_functions[0]))

static const struct { const char *section; const char *guid; } install_sections[] = {
	{ "Camera", GUID_CAMERA }, { "Motor", GUID_MOTOR }, { "Audio", GUID_AUDIO }, { "Security", GUID_SECURITY },
};

/* ---- small helpers ------------------------------------------------------ */

static void print_win32_error(const wchar_t *what, DWORD err)
{
	wchar_t *msg = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL, err, 0, (LPWSTR)&msg, 0, NULL);
	fwprintf(stderr, L"error: %ls failed (0x%08lX)%ls%ls", what, err, msg ? L": " : L"\n", msg ? msg : L"");
	if (msg)
		LocalFree(msg);
}

static int is_elevated(void)
{
	HANDLE token;
	TOKEN_ELEVATION elevation;
	DWORD size = sizeof(elevation);
	int result = 0;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
			result = elevation.TokenIsElevated != 0;
		CloseHandle(token);
	}
	return result;
}

static void widen(const char *in, wchar_t *out, size_t out_len)
{
	MultiByteToWideChar(CP_ACP, 0, in, -1, out, (int)out_len);
}

/* Is `id` (exactly, case-insensitive) one of the entries of a REG_MULTI_SZ list? */
static int multi_sz_contains(const wchar_t *list, const wchar_t *id)
{
	for (; list && *list; list += wcslen(list) + 1)
		if (_wcsicmp(list, id) == 0)
			return 1;
	return 0;
}

/* ---- device enumeration ------------------------------------------------- */

typedef void (*device_visitor)(HDEVINFO set, SP_DEVINFO_DATA *dev, const kinect_function *fn, void *user);

/* Call `visit` for every present USB device that matches a Kinect function. */
static int for_each_kinect_device(device_visitor visit, void *user)
{
	HDEVINFO set = SetupDiGetClassDevsW(NULL, L"USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	SP_DEVINFO_DATA dev;
	DWORD i;
	int found = 0;
	if (set == INVALID_HANDLE_VALUE) {
		print_win32_error(L"SetupDiGetClassDevs", GetLastError());
		return -1;
	}
	dev.cbSize = sizeof(dev);
	for (i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++) {
		wchar_t ids[2048] = { 0 };
		size_t f;
		if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, NULL, (BYTE *)ids, sizeof(ids) - 2 * sizeof(wchar_t), NULL))
			continue;
		for (f = 0; f < NUM_FUNCTIONS; f++) {
			wchar_t want[64];
			widen(kinect_functions[f].hwid, want, 64);
			if (multi_sz_contains(ids, want)) {
				found++;
				if (visit)
					visit(set, &dev, &kinect_functions[f], user);
				break;
			}
		}
	}
	SetupDiDestroyDeviceInfoList(set);
	return found;
}

static void print_device(HDEVINFO set, SP_DEVINFO_DATA *dev, const kinect_function *fn, void *user)
{
	wchar_t service[64] = L"(no driver)", name[256] = L"", instance[512] = L"";
	(void)user;
	SetupDiGetDeviceRegistryPropertyW(set, dev, SPDRP_SERVICE, NULL, (BYTE *)service, sizeof(service), NULL);
	if (!SetupDiGetDeviceRegistryPropertyW(set, dev, SPDRP_FRIENDLYNAME, NULL, (BYTE *)name, sizeof(name), NULL))
		SetupDiGetDeviceRegistryPropertyW(set, dev, SPDRP_DEVICEDESC, NULL, (BYTE *)name, sizeof(name), NULL);
	SetupDiGetDeviceInstanceIdW(set, dev, instance, 512, NULL);
	wprintf(L"  %-30hs driver: %-10ls %ls\n      %ls\n", fn->hwid, service, name, instance);
}

typedef struct {
	DEVINST hubs[8];
	int num_hubs;
	int camera_or_audio_seen;
} list_state;

static void remember_hub(HDEVINFO set, SP_DEVINFO_DATA *dev, const kinect_function *fn, void *user)
{
	list_state *st = (list_state *)user;
	DEVINST parent;
	int i;
	print_device(set, dev, fn, NULL);
	if (strstr(fn->section, "Motor") == NULL)
		st->camera_or_audio_seen = 1;
	if (CM_Get_Parent(&parent, dev->DevInst, 0) != CR_SUCCESS)
		return;
	for (i = 0; i < st->num_hubs; i++)
		if (st->hubs[i] == parent)
			return;
	if (st->num_hubs < 8)
		st->hubs[st->num_hubs++] = parent;
}

/* Print every device on a hub, with its status, so that functions that fail to
 * enumerate (e.g. USB\DEVICE_DESCRIPTOR_FAILURE) are visible too. */
static void print_hub(DEVINST hub)
{
	wchar_t id[MAX_DEVICE_ID_LEN];
	DEVINST child;
	ULONG status, problem;
	if (CM_Get_Device_IDW(hub, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
		wprintf(L"\nDevices on the hub %ls:\n", id);
	if (CM_Get_Child(&child, hub, 0) != CR_SUCCESS) {
		wprintf(L"  (none)\n");
		return;
	}
	do {
		if (CM_Get_Device_IDW(child, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
			continue;
		if (CM_Get_DevNode_Status(&status, &problem, child, 0) == CR_SUCCESS && (status & DN_HAS_PROBLEM))
			wprintf(L"  %ls  (problem code %lu)\n", id, problem);
		else
			wprintf(L"  %ls\n", id);
	} while (CM_Get_Sibling(&child, child, 0) == CR_SUCCESS);
}

static int cmd_list(void)
{
	list_state st;
	int n, i;
	memset(&st, 0, sizeof(st));
	wprintf(L"Kinect v1 USB functions present:\n");
	n = for_each_kinect_device(remember_hub, &st);
	if (n == 0)
		wprintf(L"  none (is the Kinect plugged in and powered?)\n");
	for (i = 0; i < st.num_hubs; i++)
		print_hub(st.hubs[i]);
	if (n > 0) {
		wprintf(L"\nlibfreenect needs driver 'WinUSB' (or libusbK) on each Kinect function.\n");
		if (!st.camera_or_audio_seen)
			wprintf(L"Only the motor is present: the camera and audio have not enumerated. "
			        L"Check that the Kinect's own power adapter is connected.\n");
	}
	return n < 0 ? 1 : 0;
}

/* ---- package generation ------------------------------------------------- */

static int write_inf(const wchar_t *path)
{
	FILE *f;
	size_t i;
	SYSTEMTIME now;
	if (_wfopen_s(&f, path, L"w") != 0 || !f) {
		fwprintf(stderr, L"error: cannot write %ls\n", path);
		return -1;
	}
	GetSystemTime(&now);
	fprintf(f,
		"; Kinect v1 (Xbox 360 / Kinect for Windows) USB functions on the in-box WinUSB driver.\n"
		"; Generated by freenect-driver-install (libfreenect). Do not edit: the catalog\n"
		"; signature covers this file.\n"
		"\n"
		"[Version]\n"
		"Signature   = \"$Windows NT$\"\n"
		"Class       = USBDevice\n"
		"ClassGuid   = {88BAE032-5A81-49F0-BC3D-A4FF138216D6}\n"
		"Provider    = %%Provider%%\n"
		"CatalogFile = " "libfreenect-kinect.cat" "\n"
		"DriverVer   = %02u/%02u/%04u,1.0.%u.%u\n"
		"PnpLockdown = 1\n"
		"\n"
		"[Manufacturer]\n"
		"%%Provider%% = Kinect,NTamd64,NTarm64,NTx86\n",
		now.wMonth, now.wDay, now.wYear, now.wYear, now.wMonth * 100 + now.wDay);

	{
		const char *decorations[] = { "NTamd64", "NTarm64", "NTx86" };
		size_t d;
		for (d = 0; d < 3; d++) {
			fprintf(f, "\n[Kinect.%s]\n", decorations[d]);
			for (i = 0; i < NUM_FUNCTIONS; i++)
				fprintf(f, "%%Dev%u%% = %s_Install, %s\n", (unsigned)i, kinect_functions[i].section, kinect_functions[i].hwid);
		}
	}

	for (i = 0; i < sizeof(install_sections) / sizeof(install_sections[0]); i++) {
		const char *s = install_sections[i].section;
		fprintf(f,
			"\n[%s_Install]\n"
			"Include = winusb.inf\n"
			"Needs   = WINUSB.NT\n"
			"\n"
			"[%s_Install.Services]\n"
			"Include = winusb.inf\n"
			"Needs   = WINUSB.NT.Services\n"
			"\n"
			"[%s_Install.HW]\n"
			"AddReg = %s_AddReg\n"
			"\n"
			"[%s_AddReg]\n"
			"HKR,,DeviceInterfaceGUIDs,0x10000,\"%s\"\n",
			s, s, s, s, s, install_sections[i].guid);
	}

	fprintf(f, "\n[Strings]\nProvider = \"OpenKinect (libfreenect)\"\n");
	for (i = 0; i < NUM_FUNCTIONS; i++)
		fprintf(f, "Dev%u = \"%s\"\n", (unsigned)i, kinect_functions[i].desc);
	fclose(f);
	return 0;
}

static int put_ascii_attr_cat(HANDLE cat, const wchar_t *name, const wchar_t *value)
{
	DWORD flags = CRYPTCAT_ATTR_AUTHENTICATED | CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII;
	return CryptCATPutCatAttrInfo(cat, (LPWSTR)name, flags, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)), (BYTE *)value) != NULL;
}

static int put_ascii_attr_member(HANDLE cat, CRYPTCATMEMBER *member, const wchar_t *name, const wchar_t *value)
{
	DWORD flags = CRYPTCAT_ATTR_AUTHENTICATED | CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII;
	return CryptCATPutAttrInfo(cat, member, (LPWSTR)name, flags, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)), (BYTE *)value) != NULL;
}

/* Build a catalog listing `file` (the INF). The member is hashed with SHA-1:
 * the driver store looks driver-package files up by their SHA-1 hash, and
 * rejects a SHA-256-only (version 2) catalog with
 * ERROR_FILE_HASH_NOT_IN_CATALOG. The catalog signature itself is SHA-256. */
static int write_catalog(const wchar_t *cat_path, const wchar_t *file_path, const wchar_t *file_name)
{
	HANDLE cat = INVALID_HANDLE_VALUE, fh = INVALID_HANDLE_VALUE;
	GUID subject;
	SIP_DISPATCH_INFO sip;
	SIP_SUBJECTINFO info;
	SIP_INDIRECT_DATA *indirect = NULL;
	DWORD indirect_len = 0, i;
	CRYPTCATMEMBER *member;
	HCRYPTPROV hash_prov = 0;
	wchar_t tag[2 * 64 + 1];
	int ok = 0;

	cat = CryptCATOpen((LPWSTR)cat_path, CRYPTCAT_OPEN_CREATENEW, 0, CRYPTCAT_VERSION_1, 0);
	if (cat == INVALID_HANDLE_VALUE) {
		print_win32_error(L"CryptCATOpen", GetLastError());
		return -1;
	}
	if (!put_ascii_attr_cat(cat, L"OS", L"_v100,_v100_X64,_v100_ARM64")) {
		print_win32_error(L"CryptCATPutCatAttrInfo", GetLastError());
		goto out;
	}
	for (i = 0; i < NUM_FUNCTIONS; i++) {
		wchar_t name[16], value[64], *p;
		swprintf(name, 16, L"HWID%u", (unsigned)(i + 1));
		widen(kinect_functions[i].hwid, value, 64);
		for (p = value; *p; p++)
			*p = towlower(*p);
		if (!put_ascii_attr_cat(cat, name, value)) {
			print_win32_error(L"CryptCATPutCatAttrInfo", GetLastError());
			goto out;
		}
	}

	fh = CreateFileW(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		print_win32_error(L"CreateFile (INF)", GetLastError());
		goto out;
	}

	/* Let the subject interface package (SIP) for this file type produce the
	 * indirect data, exactly as catalog tools do. */
	if (!CryptSIPRetrieveSubjectGuidForCatalogFile(file_path, fh, &subject)) {
		print_win32_error(L"CryptSIPRetrieveSubjectGuidForCatalogFile", GetLastError());
		goto out;
	}
	memset(&sip, 0, sizeof(sip));
	sip.cbSize = sizeof(sip);
	if (!CryptSIPLoad(&subject, 0, &sip)) {
		print_win32_error(L"CryptSIPLoad", GetLastError());
		goto out;
	}
	if (!CryptAcquireContextW(&hash_prov, NULL, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
		print_win32_error(L"CryptAcquireContext", GetLastError());
		goto out;
	}
	memset(&info, 0, sizeof(info));
	info.cbSize = sizeof(info);
	info.hProv = hash_prov;
	info.pgSubjectType = &subject;
	info.hFile = fh;
	info.pwsFileName = file_path;
	info.DigestAlgorithm.pszObjId = szOID_OIWSEC_sha1;
	info.dwEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
	if (!sip.pfCreate(&info, &indirect_len, NULL) ||
	    (indirect = (SIP_INDIRECT_DATA *)malloc(indirect_len)) == NULL ||
	    !sip.pfCreate(&info, &indirect_len, indirect)) {
		print_win32_error(L"SIP CreateIndirectData", GetLastError());
		goto out;
	}

	/* The member tag is the digest in upper-case hex. */
	if (indirect->Digest.cbData > 64)
		goto out;
	for (i = 0; i < indirect->Digest.cbData; i++)
		swprintf(tag + 2 * i, 3, L"%02X", indirect->Digest.pbData[i]);
	tag[2 * indirect->Digest.cbData] = 0;

	member = CryptCATPutMemberInfo(cat, (LPWSTR)file_name, tag, &subject, 0x200, indirect_len, (BYTE *)indirect);
	if (!member) {
		print_win32_error(L"CryptCATPutMemberInfo", GetLastError());
		goto out;
	}
	if (!put_ascii_attr_member(cat, member, L"File", file_name) ||
	    !put_ascii_attr_member(cat, member, L"OSAttr", CATALOG_OS_ATTR)) {
		print_win32_error(L"CryptCATPutAttrInfo", GetLastError());
		goto out;
	}
	if (!CryptCATPersistStore(cat)) {
		print_win32_error(L"CryptCATPersistStore", GetLastError());
		goto out;
	}
	ok = 1;
out:
	free(indirect);
	if (hash_prov)
		CryptReleaseContext(hash_prov, 0);
	if (fh != INVALID_HANDLE_VALUE)
		CloseHandle(fh);
	CryptCATClose(cat);
	return ok ? 0 : -1;
}

/* ---- signing ------------------------------------------------------------ */

/* Create a self-signed code-signing certificate whose key lives in a new,
 * uniquely named key container. The caller deletes the container afterwards. */
static PCCERT_CONTEXT create_signing_cert(wchar_t *container, size_t container_len)
{
	HCRYPTPROV prov = 0;
	HCRYPTKEY key = 0;
	PCCERT_CONTEXT cert = NULL;
	wchar_t subject_str[256], computer[MAX_COMPUTERNAME_LENGTH + 1] = L"this PC";
	DWORD computer_len = MAX_COMPUTERNAME_LENGTH + 1, name_len = 0;
	BYTE *name = NULL;
	CERT_NAME_BLOB subject;
	CRYPT_KEY_PROV_INFO prov_info;
	CRYPT_ALGORITHM_IDENTIFIER sig_alg;
	SYSTEMTIME start, end;
	GUID g;
	char *eku_oids[1] = { szOID_PKIX_KP_CODE_SIGNING };
	CERT_ENHKEY_USAGE eku = { 1, eku_oids };
	BYTE *eku_enc = NULL;
	DWORD eku_len = 0;
	CERT_EXTENSION ext;
	CERT_EXTENSIONS exts;

	CoCreateGuid(&g);
	swprintf(container, container_len, L"libfreenect-driver-%08lX%04X%04X", g.Data1, g.Data2, g.Data3);
	GetComputerNameW(computer, &computer_len);
	GetSystemTime(&start);
	swprintf(subject_str, 256, L"CN=\"" CERT_SUBJECT_TAG L" (local package signer, %ls, %04u-%02u-%02u)\"",
	         computer, start.wYear, start.wMonth, start.wDay);

	if (!CryptAcquireContextW(&prov, container, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_NEWKEYSET) ||
	    !CryptGenKey(prov, AT_SIGNATURE, (2048 << 16), &key)) {
		print_win32_error(L"creating the signing key", GetLastError());
		goto out;
	}
	if (!CertStrToNameW(X509_ASN_ENCODING, subject_str, CERT_X500_NAME_STR, NULL, NULL, &name_len, NULL) ||
	    (name = (BYTE *)malloc(name_len)) == NULL ||
	    !CertStrToNameW(X509_ASN_ENCODING, subject_str, CERT_X500_NAME_STR, NULL, name, &name_len, NULL)) {
		print_win32_error(L"CertStrToName", GetLastError());
		goto out;
	}
	if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, CRYPT_ENCODE_ALLOC_FLAG, NULL, &eku_enc, &eku_len)) {
		print_win32_error(L"encoding the EKU extension", GetLastError());
		goto out;
	}
	ext.pszObjId = szOID_ENHANCED_KEY_USAGE;
	ext.fCritical = FALSE;
	ext.Value.cbData = eku_len;
	ext.Value.pbData = eku_enc;
	exts.cExtension = 1;
	exts.rgExtension = &ext;

	subject.cbData = name_len;
	subject.pbData = name;
	memset(&prov_info, 0, sizeof(prov_info));
	prov_info.pwszContainerName = container;
	prov_info.pwszProvName = MS_ENH_RSA_AES_PROV_W;
	prov_info.dwProvType = PROV_RSA_AES;
	prov_info.dwKeySpec = AT_SIGNATURE;
	memset(&sig_alg, 0, sizeof(sig_alg));
	sig_alg.pszObjId = szOID_RSA_SHA256RSA;
	end = start;
	end.wYear += 20;  /* the key is destroyed right away, so a long validity costs nothing */

	cert = CertCreateSelfSignCertificate(prov, &subject, 0, &prov_info, &sig_alg, &start, &end, &exts);
	if (!cert)
		print_win32_error(L"CertCreateSelfSignCertificate", GetLastError());
out:
	if (eku_enc)
		LocalFree(eku_enc);
	free(name);
	if (key)
		CryptDestroyKey(key);
	if (prov)
		CryptReleaseContext(prov, 0);
	return cert;
}

static void delete_key_container(const wchar_t *container)
{
	HCRYPTPROV prov = 0;
	/* With CRYPT_DELETEKEYSET the handle is not returned; the container is gone. */
	if (!CryptAcquireContextW(&prov, container, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_DELETEKEYSET))
		print_win32_error(L"deleting the signing key", GetLastError());
}

static int sign_catalog(const wchar_t *cat_path, PCCERT_CONTEXT cert)
{
	CRYPTUI_WIZ_DIGITAL_SIGN_INFO si;
	CRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO ext;
	memset(&ext, 0, sizeof(ext));
	ext.dwSize = sizeof(ext);
	ext.pszHashAlg = szOID_NIST_sha256;
	memset(&si, 0, sizeof(si));
	si.dwSize = sizeof(si);
	si.dwSubjectChoice = CRYPTUI_WIZ_DIGITAL_SIGN_SUBJECT_FILE;
	si.pwszFileName = cat_path;
	si.dwSigningCertChoice = CRYPTUI_WIZ_DIGITAL_SIGN_CERT;
	si.pSigningCertContext = cert;
	si.dwAdditionalCertChoice = 0;
	si.pSignExtInfo = &ext;
	if (!CryptUIWizDigitalSign(CRYPTUI_WIZ_NO_UI, NULL, NULL, &si, NULL)) {
		print_win32_error(L"signing the catalog", GetLastError());
		return -1;
	}
	return 0;
}

/* Build INF + signed catalog in `dir`. Returns the signing certificate (its
 * private key already destroyed) or NULL. */
static PCCERT_CONTEXT build_package(const wchar_t *dir, wchar_t *inf_path, size_t inf_len)
{
	wchar_t cat_path[MAX_PATH], container[96];
	PCCERT_CONTEXT cert;
	int ok;

	SHCreateDirectoryExW(NULL, dir, NULL);
	swprintf(inf_path, inf_len, L"%ls\\" PACKAGE_BASENAME L".inf", dir);
	swprintf(cat_path, MAX_PATH, L"%ls\\" PACKAGE_BASENAME L".cat", dir);
	DeleteFileW(cat_path);

	if (write_inf(inf_path) != 0)
		return NULL;
	wprintf(L"Wrote %ls\n", inf_path);
	if (write_catalog(cat_path, inf_path, PACKAGE_BASENAME L".inf") != 0)
		return NULL;
	wprintf(L"Wrote %ls\n", cat_path);

	cert = create_signing_cert(container, 96);
	if (!cert) {
		delete_key_container(container);
		return NULL;
	}
	ok = sign_catalog(cat_path, cert) == 0;
	/* Destroy the private key whatever happened: nothing may be signed with it later. */
	delete_key_container(container);
	CertSetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL);
	if (!ok) {
		CertFreeCertificateContext(cert);
		return NULL;
	}
	wprintf(L"Signed the catalog; the signing key has been destroyed.\n");
	return cert;
}

/* ---- trust stores ------------------------------------------------------- */

static int add_cert_to_store(PCCERT_CONTEXT cert, const wchar_t *store_name)
{
	HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
	                                 CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_OPEN_EXISTING_FLAG, store_name);
	int ok;
	if (!store) {
		print_win32_error(L"CertOpenStore", GetLastError());
		return -1;
	}
	ok = CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
	if (!ok)
		print_win32_error(L"CertAddCertificateContextToStore", GetLastError());
	CertCloseStore(store, 0);
	return ok ? 0 : -1;
}

/* Remove every certificate this tool ever added from a local machine store. */
static int remove_our_certs(const wchar_t *store_name)
{
	HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
	                                 CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_OPEN_EXISTING_FLAG, store_name);
	PCCERT_CONTEXT cert = NULL;
	int removed = 0;
	if (!store)
		return 0;
	while ((cert = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_W,
	                                          CERT_SUBJECT_TAG, NULL)) != NULL) {
		/* CertDeleteCertificateFromStore frees the context. */
		if (!CertDeleteCertificateFromStore(cert))
			break;
		removed++;
	}
	CertCloseStore(store, 0);
	return removed;
}

/* ---- driver store ------------------------------------------------------- */

/* Remove all driver packages previously installed by this tool. They are
 * recognised by their catalog file name. */
static int remove_our_packages(void)
{
	wchar_t pattern[MAX_PATH], inf_dir[MAX_PATH];
	WIN32_FIND_DATAW fd;
	HANDLE find;
	int removed = 0;

	GetWindowsDirectoryW(inf_dir, MAX_PATH);
	wcscat_s(inf_dir, MAX_PATH, L"\\INF");
	swprintf(pattern, MAX_PATH, L"%ls\\oem*.inf", inf_dir);
	find = FindFirstFileW(pattern, &fd);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	do {
		wchar_t path[MAX_PATH], catalog[MAX_PATH] = L"";
		HINF inf;
		INFCONTEXT ctx;
		swprintf(path, MAX_PATH, L"%ls\\%ls", inf_dir, fd.cFileName);
		inf = SetupOpenInfFileW(path, NULL, INF_STYLE_WIN4, NULL);
		if (inf == INVALID_HANDLE_VALUE)
			continue;
		if (SetupFindFirstLineW(inf, L"Version", L"CatalogFile", &ctx))
			SetupGetStringFieldW(&ctx, 1, catalog, MAX_PATH, NULL);
		SetupCloseInfFile(inf);
		if (_wcsicmp(catalog, PACKAGE_BASENAME L".cat") != 0)
			continue;
		if (SetupUninstallOEMInfW(fd.cFileName, SUOI_FORCEDELETE, NULL)) {
			wprintf(L"Removed driver package %ls\n", fd.cFileName);
			removed++;
		} else {
			print_win32_error(L"SetupUninstallOEMInf", GetLastError());
		}
	} while (FindNextFileW(find, &fd));
	FindClose(find);
	return removed;
}

typedef struct {
	const wchar_t *inf_path;
	int installed, failed, reboot;
} install_state;

static void install_on_device(HDEVINFO set, SP_DEVINFO_DATA *dev, const kinect_function *fn, void *user)
{
	install_state *st = (install_state *)user;
	wchar_t hwid[64];
	BOOL reboot = FALSE;
	(void)set; (void)dev;
	widen(fn->hwid, hwid, 64);
	/* Installs on every present device with this hardware ID. NONINTERACTIVE
	 * makes it fail instead of waiting on UI that a console tool never shows. */
	if (UpdateDriverForPlugAndPlayDevicesW(NULL, hwid, st->inf_path, INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE, &reboot)) {
		wprintf(L"  %-30ls now uses WinUSB\n", hwid);
		st->installed++;
	} else {
		DWORD err = GetLastError();
		if (err != ERROR_NO_SUCH_DEVINST) {
			print_win32_error(hwid, err);
			st->failed++;
		}
	}
	if (reboot)
		st->reboot = 1;
}

static void package_dir(wchar_t *dir, size_t len)
{
	wchar_t *base = NULL;
	if (SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &base) == S_OK) {
		swprintf(dir, len, L"%ls\\libfreenect\\driver", base);
		CoTaskMemFree(base);
	} else {
		swprintf(dir, len, L"C:\\ProgramData\\libfreenect\\driver");
	}
}

/* Remove a Kinect function's device node, so it re-enumerates without the
 * driver whose package is about to be deleted. */
static void remove_device(HDEVINFO set, SP_DEVINFO_DATA *dev, const kinect_function *fn, void *user)
{
	BOOL reboot = FALSE;
	int *removed = (int *)user;
	(void)fn;
	if (DiUninstallDevice(NULL, set, dev, 0, &reboot))
		(*removed)++;
	else
		print_win32_error(L"DiUninstallDevice", GetLastError());
}

/* Ask Plug and Play to scan for hardware changes and wait for it. */
static void rescan_devices(void)
{
	DEVINST root;
	if (CM_Locate_DevNodeW(&root, NULL, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
		CM_Reenumerate_DevNode(root, CM_REENUMERATE_SYNCHRONOUS);
}

static int cmd_uninstall(int quiet)
{
	int devices = 0, pk, c1, c2;
	/* Devices first, while their driver package still exists. */
	for_each_kinect_device(remove_device, &devices);
	pk = remove_our_packages();
	c1 = remove_our_certs(L"TrustedPublisher");
	c2 = remove_our_certs(L"Root");
	rescan_devices();
	if (!quiet) {
		wprintf(L"Removed %d device(s), %d driver package(s) and %d certificate(s).\n", devices, pk, c1 + c2);
		wprintf(L"The Kinect functions now have no driver until one is installed again.\n");
	}
	return 0;
}

static int cmd_install(void)
{
	wchar_t dir[MAX_PATH], inf_path[MAX_PATH], oem_name[MAX_PATH];
	PCCERT_CONTEXT cert;
	install_state st = { 0 };

	/* Replace any earlier installation, including its certificates. */
	cmd_uninstall(1);

	package_dir(dir, MAX_PATH);
	cert = build_package(dir, inf_path, MAX_PATH);
	if (!cert)
		return 1;
	if (add_cert_to_store(cert, L"Root") != 0 || add_cert_to_store(cert, L"TrustedPublisher") != 0) {
		CertFreeCertificateContext(cert);
		return 1;
	}
	CertFreeCertificateContext(cert);
	wprintf(L"Trusted the package certificate (local machine Root and TrustedPublisher).\n");

	/* Stage the package in the driver store, so functions that only appear
	 * later (e.g. 02BB/02C3 after the audio firmware upload) pick it up. */
	if (!SetupCopyOEMInfW(inf_path, NULL, SPOST_PATH, 0, oem_name, MAX_PATH, NULL, NULL)) {
		print_win32_error(L"SetupCopyOEMInf", GetLastError());
		cmd_uninstall(1);
		return 1;
	}
	wprintf(L"Added the driver package to the driver store as %ls\n", oem_name);

	st.inf_path = inf_path;
	if (for_each_kinect_device(install_on_device, &st) == 0)
		wprintf(L"No Kinect is plugged in; WinUSB will be used automatically when it is.\n");
	if (st.reboot)
		wprintf(L"Windows asks for a reboot to finish.\n");
	return st.failed ? 1 : 0;
}

static int cmd_package_only(const wchar_t *dir)
{
	wchar_t inf_path[MAX_PATH], full_dir[MAX_PATH];
	PCCERT_CONTEXT cert;
	/* SHCreateDirectoryEx needs an absolute path. */
	DWORD n = GetFullPathNameW(dir, MAX_PATH, full_dir, NULL);
	if (n == 0 || n >= MAX_PATH) {
		fwprintf(stderr, L"error: invalid directory %ls\n", dir);
		return 1;
	}
	cert = build_package(full_dir, inf_path, MAX_PATH);
	if (!cert)
		return 1;
	CertFreeCertificateContext(cert);
	wprintf(L"Package built. No certificate was trusted and nothing was installed.\n");
	return 0;
}

static void usage(void)
{
	wprintf(L"freenect-driver-install: bind the Kinect v1 to Microsoft's in-box WinUSB driver\n\n"
	        L"  --list               show the Kinect USB functions and their current drivers\n"
	        L"  --install            install WinUSB for them (run as Administrator)\n"
	        L"  --uninstall          remove what --install added (run as Administrator)\n"
	        L"  --package-only DIR   only build and sign the driver package in DIR\n");
}

int wmain(int argc, wchar_t **argv)
{
	if (argc >= 2 && wcscmp(argv[1], L"--list") == 0)
		return cmd_list();
	if (argc >= 3 && wcscmp(argv[1], L"--package-only") == 0)
		return cmd_package_only(argv[2]);
	if (argc >= 2 && (wcscmp(argv[1], L"--install") == 0 || wcscmp(argv[1], L"--uninstall") == 0)) {
		if (!is_elevated()) {
			fwprintf(stderr, L"error: %ls needs Administrator rights. Run it from an elevated terminal.\n", argv[1]);
			return 1;
		}
		return wcscmp(argv[1], L"--install") == 0 ? cmd_install() : cmd_uninstall(0);
	}
	usage();
	return argc >= 2 && wcscmp(argv[1], L"--help") != 0 ? 1 : 0;
}
