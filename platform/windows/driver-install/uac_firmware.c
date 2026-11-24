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
 * Fetch the Kinect v1 audio/motor controller firmware (UAC.BIN, shipped as
 * UACFirmware.bin) from Microsoft's own Kinect for Windows Runtime v1.8
 * installer, on the user's machine.
 *
 * The installer is a WiX Burn bundle. Its payload is an attached cabinet that
 * holds MSI packages; the x64 drivers MSI ("a0") contains an embedded
 * cabinet with the file "UACFirmware". This code extracts that file with the
 * Cabinet (FDI) and Windows Installer APIs. The installer is never run.
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <winhttp.h>
#include <wintrust.h>
#include <softpub.h>
#include <bcrypt.h>
#include <msi.h>
#include <msiquery.h>
#include <fdi.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uac_firmware.h"

/* Microsoft's download (Kinect for Windows Runtime v1.8, 1.8.0.595). */
#define RUNTIME_HOST L"download.microsoft.com"
#define RUNTIME_PATH L"/download/e/c/5/ec50686b-82f4-4dbf-a922-980183b214e6/KinectRuntime-v1.8-Setup.exe"
#define RUNTIME_SIZE 116273144u
static const char RUNTIME_SHA256[] = "f4d4143fb0f0a8d276889c077bfc8af42bfe99c128cadab5e316bf015a9858e9";

/* The firmware itself: what we accept, whatever the installer's hash. */
#define FIRMWARE_SIZE 145920u
static const char FIRMWARE_SHA256[] = "4467ae36ad378c58477432729d74eed0f9d45d35213f4430a781d90f64cea3f9";
#define FIRMWARE_MAGIC 0xCA77F00Du
#define FIRMWARE_BASE 0x00080000u
#define FIRMWARE_ENTRY 0x00080030u

// The beamformer weights of KinectAudio10.dll v1.8 x64, as kinect_beam_weights.bin
#define KA10_WEIGHTSET_VA 0x180070098ull
#define AUDIO_WEIGHTS_SIZE 180340u
static const char AUDIO_WEIGHTS_SHA256[] = "a56ddba443684ba9588dc44e21cc499fd4b51bafc592fff086e4b40d4250e5e7";

/* ---- small helpers ------------------------------------------------------ */

static void fail(const wchar_t *what, DWORD err)
{
	wchar_t *msg = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
	               FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandleW(L"winhttp.dll"), err, 0, (LPWSTR)&msg, 0, NULL);
	fwprintf(stderr, L"error: %ls (0x%08lX)%ls%ls", what, err, msg ? L": " : L"\n", msg ? msg : L"");
	if (msg)
		LocalFree(msg);
}

static int sha256_hex(const BYTE *data, size_t len, char hex[65])
{
	BYTE digest[32];
	int i;
	if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0, (PUCHAR)data, (ULONG)len, digest, sizeof(digest)) != 0)
		return -1;
	for (i = 0; i < 32; i++)
		sprintf_s(hex + 2 * i, 3, "%02x", digest[i]);
	return 0;
}

static int write_file(const wchar_t *path, const BYTE *data, size_t len)
{
	HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written = 0;
	BOOL ok;
	if (h == INVALID_HANDLE_VALUE)
		return -1;
	ok = WriteFile(h, data, (DWORD)len, &written, NULL) && written == len;
	CloseHandle(h);
	return ok ? 0 : -1;
}

static BYTE *read_file(const wchar_t *path, size_t *len)
{
	HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	LARGE_INTEGER size;
	BYTE *data = NULL;
	DWORD got = 0;
	if (h == INVALID_HANDLE_VALUE)
		return NULL;
	if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < 512 * 1024 * 1024 &&
	    (data = (BYTE *)malloc((size_t)size.QuadPart)) != NULL &&
	    ReadFile(h, data, (DWORD)size.QuadPart, &got, NULL) && got == (DWORD)size.QuadPart) {
		*len = (size_t)size.QuadPart;
	} else {
		free(data);
		data = NULL;
	}
	CloseHandle(h);
	return data;
}

void uac_default_dir(wchar_t *dir, size_t len)
{
	wchar_t *base = NULL;
	if (SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &base) == S_OK) {
		swprintf(dir, len, L"%ls\\libfreenect", base);
		CoTaskMemFree(base);
	} else {
		swprintf(dir, len, L"C:\\ProgramData\\libfreenect");
	}
}

/* ---- download ----------------------------------------------------------- */

static BYTE *download_runtime(size_t *len)
{
	HINTERNET session = NULL, conn = NULL, req = NULL;
	BYTE *buf = NULL;
	size_t cap = RUNTIME_SIZE + 1024 * 1024, used = 0, next_report = 10u << 20;
	DWORD status = 0, size = sizeof(status);
	int ok = 0;

	session = WinHttpOpen(L"libfreenect-driver-install/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
	                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session ||
	    !(conn = WinHttpConnect(session, RUNTIME_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0)) ||
	    !(req = WinHttpOpenRequest(conn, L"GET", RUNTIME_PATH, NULL, WINHTTP_NO_REFERER,
	                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) ||
	    !WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
	    !WinHttpReceiveResponse(req, NULL)) {
		fail(L"downloading the Kinect Runtime from " RUNTIME_HOST, GetLastError());
		goto out;
	}
	if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
	                         &status, &size, WINHTTP_NO_HEADER_INDEX) || status != 200) {
		fwprintf(stderr, L"error: download.microsoft.com answered HTTP %lu\n", status);
		goto out;
	}
	buf = (BYTE *)malloc(cap);
	if (!buf)
		goto out;
	for (;;) {
		DWORD avail = 0, got = 0;
		if (!WinHttpQueryDataAvailable(req, &avail)) {
			fail(L"reading the download", GetLastError());
			goto out;
		}
		if (avail == 0)
			break;
		if (used + avail > cap) {
			fwprintf(stderr, L"error: the download is larger than expected\n");
			goto out;
		}
		if (!WinHttpReadData(req, buf + used, avail, &got)) {
			fail(L"reading the download", GetLastError());
			goto out;
		}
		used += got;
		if (used >= next_report) {
			wprintf(L"  %u of %u MB\n", (unsigned)(used >> 20), (unsigned)((RUNTIME_SIZE + (1u << 19)) >> 20));
			next_report += 10u << 20;
		}
	}
	*len = used;
	ok = 1;
out:
	if (req) WinHttpCloseHandle(req);
	if (conn) WinHttpCloseHandle(conn);
	if (session) WinHttpCloseHandle(session);
	if (!ok) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

/* ---- Authenticode -------------------------------------------------------- */

/* The file must carry a valid Authenticode signature by Microsoft Corporation. */
static int verify_microsoft_signature(const wchar_t *path)
{
	WINTRUST_FILE_INFO file = { 0 };
	WINTRUST_DATA data = { 0 };
	GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	LONG status;
	int ok = 0;

	file.cbStruct = sizeof(file);
	file.pcwszFilePath = path;
	data.cbStruct = sizeof(data);
	data.dwUIChoice = WTD_UI_NONE;
	data.fdwRevocationChecks = WTD_REVOKE_NONE;
	data.dwUnionChoice = WTD_CHOICE_FILE;
	data.pFile = &file;
	data.dwStateAction = WTD_STATEACTION_VERIFY;

	status = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
	if (status != ERROR_SUCCESS) {
		fail(L"the Kinect Runtime installer's signature is not valid", (DWORD)status);
	} else {
		CRYPT_PROVIDER_DATA *prov = WTHelperProvDataFromStateData(data.hWVTStateData);
		CRYPT_PROVIDER_SGNR *signer = prov ? WTHelperGetProvSignerFromChain(prov, 0, FALSE, 0) : NULL;
		if (signer && signer->csCertChain > 0 && signer->pasCertChain[0].pCert) {
			wchar_t name[256] = L"";
			CertGetNameStringW(signer->pasCertChain[0].pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, 256);
			if (wcscmp(name, L"Microsoft Corporation") == 0)
				ok = 1;
			else
				fwprintf(stderr, L"error: the installer is signed by \"%ls\", not Microsoft Corporation\n", name);
		} else {
			fwprintf(stderr, L"error: could not read the installer's signer\n");
		}
	}
	data.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
	return ok ? 0 : -1;
}

/* ---- cabinets (FDI) ----------------------------------------------------- */

typedef struct {
	const char *want;     /* file name inside the cabinet */
	const char *out_path; /* where to write it */
	int found;
} fdi_target;

static FNALLOC(fdi_alloc) { return malloc(cb); }
static FNFREE(fdi_free) { free(pv); }
static FNOPEN(fdi_open) { int fd = -1; _sopen_s(&fd, pszFile, oflag | _O_BINARY, _SH_DENYNO, pmode); return fd; }
static FNREAD(fdi_read) { return (UINT)_read((int)hf, pv, cb); }
static FNWRITE(fdi_write) { return (UINT)_write((int)hf, pv, cb); }
static FNCLOSE(fdi_close) { return _close((int)hf); }
static FNSEEK(fdi_seek) { return _lseek((int)hf, dist, seektype); }

static FNFDINOTIFY(fdi_notify)
{
	fdi_target *t = (fdi_target *)pfdin->pv;
	switch (fdint) {
	case fdintCOPY_FILE:
		if (_stricmp(pfdin->psz1, t->want) == 0) {
			int fd = -1;
			_sopen_s(&fd, t->out_path, _O_BINARY | _O_CREAT | _O_TRUNC | _O_WRONLY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
			return fd;
		}
		return 0; /* skip */
	case fdintCLOSE_FILE_INFO:
		_close((int)pfdin->hf);
		t->found = 1;
		return TRUE;
	default:
		return 0;
	}
}

/* Extract file `want` from the cabinet cab_dir\cab_name (ANSI paths; cab_dir ends with '\'). */
static int cab_extract(const char *cab_dir, const char *cab_name, const char *want, const char *out_path)
{
	ERF erf;
	fdi_target t = { want, out_path, 0 };
	HFDI h = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read, fdi_write, fdi_close, fdi_seek, cpuUNKNOWN, &erf);
	BOOL ok;
	if (!h)
		return -1;
	ok = FDICopy(h, (char *)cab_name, (char *)cab_dir, 0, fdi_notify, NULL, &t);
	FDIDestroy(h);
	return (ok && t.found) ? 0 : -1;
}

/* ---- Burn bundle and MSI ----------------------------------------------- */

/* The bundle's payload cabinet is the second "MSCF" cabinet in the file (the
 * first holds the bundle's own UI). Returns a pointer into data. */
static const BYTE *find_payload_cab(const BYTE *data, size_t len, size_t *cab_len)
{
	static const BYTE sig[8] = { 'M', 'S', 'C', 'F', 0, 0, 0, 0 };
	size_t i;
	int seen = 0;
	for (i = 0; i + 12 <= len; i++) {
		if (data[i] == 'M' && memcmp(data + i, sig, 8) == 0) {
			DWORD size = *(const DWORD *)(data + i + 8);
			if (++seen == 2) {
				if (size > len - i)
					return NULL;
				*cab_len = size;
				return data + i;
			}
		}
	}
	return NULL;
}

/* Calls fn for every stream of the MSI that is a cabinet (written to
 * cab_path first); stops when fn returns 0. Returns 0 if one did. */
static int msi_each_cab(const wchar_t *msi_path, const wchar_t *cab_path,
                        int (*fn)(void *ctx), void *ctx)
{
	MSIHANDLE db = 0, view = 0, rec = 0;
	int ok = 0;
	if (MsiOpenDatabaseW(msi_path, MSIDBOPEN_READONLY, &db) != ERROR_SUCCESS ||
	    MsiDatabaseOpenViewW(db, L"SELECT `Name`, `Data` FROM `_Streams`", &view) != ERROR_SUCCESS ||
	    MsiViewExecute(view, 0) != ERROR_SUCCESS) {
		fwprintf(stderr, L"error: cannot read a package inside the installer\n");
		goto out;
	}
	while (!ok && MsiViewFetch(view, &rec) == ERROR_SUCCESS) {
		DWORD size = MsiRecordDataSize(rec, 2);
		if (size > 8) {
			BYTE *buf = (BYTE *)malloc(size);
			DWORD got = size;
			if (buf && MsiRecordReadStream(rec, 2, (char *)buf, &got) == ERROR_SUCCESS &&
			    got == size && memcmp(buf, "MSCF", 4) == 0 && write_file(cab_path, buf, size) == 0 &&
			    fn(ctx) == 0)
				ok = 1;
			free(buf);
		}
		MsiCloseHandle(rec);
		rec = 0;
	}
out:
	if (view) MsiCloseHandle(view);
	if (db) MsiCloseHandle(db);
	return ok ? 0 : -1;
}

/* ---- the Runtime installer ---------------------------------------------- */

int kr_open(kinect_runtime *kr, const wchar_t *local_installer)
{
	BYTE *installer = NULL;
	size_t installer_len = 0, cab_len = 0;
	const BYTE *cab;
	wchar_t exe_path[MAX_PATH], cab_path[MAX_PATH];
	char hex[65];
	GUID g;
	int rc = -1;

	memset(kr, 0, sizeof(*kr));
	/* Private temporary directory; the ANSI form is needed by FDI. */
	GetTempPathW(MAX_PATH, kr->tmp);
	CoCreateGuid(&g);
	swprintf(kr->tmp + wcslen(kr->tmp), MAX_PATH - wcslen(kr->tmp), L"libfreenect-fw-%08lX\\", g.Data1);
	if (!CreateDirectoryW(kr->tmp, NULL) ||
	    WideCharToMultiByte(CP_ACP, 0, kr->tmp, -1, kr->tmp_a, MAX_PATH, NULL, NULL) == 0) {
		fwprintf(stderr, L"error: cannot create a temporary directory\n");
		kr->tmp[0] = 0;
		return -1;
	}
	swprintf(exe_path, MAX_PATH, L"%lsKinectRuntime-v1.8-Setup.exe", kr->tmp);
	swprintf(cab_path, MAX_PATH, L"%lspayload.cab", kr->tmp);

	if (local_installer) {
		wprintf(L"Reading %ls\n", local_installer);
		installer = read_file(local_installer, &installer_len);
		if (!installer) {
			fwprintf(stderr, L"error: cannot read %ls\n", local_installer);
			goto out;
		}
	} else {
		wprintf(L"Downloading Microsoft's Kinect for Windows Runtime v1.8 (%u MB) from https://" RUNTIME_HOST L"\n",
		        (unsigned)((RUNTIME_SIZE + (1u << 19)) >> 20));
		installer = download_runtime(&installer_len);
		if (!installer)
			goto out;
	}
	if (write_file(exe_path, installer, installer_len) != 0) {
		fwprintf(stderr, L"error: cannot write %ls\n", exe_path);
		goto out;
	}
	if (verify_microsoft_signature(exe_path) != 0)
		goto out;
	wprintf(L"Signature valid: Microsoft Corporation.\n");
	if (sha256_hex(installer, installer_len, hex) == 0 && strcmp(hex, RUNTIME_SHA256) != 0)
		wprintf(L"Note: the installer's SHA-256 differs from the known one (Microsoft re-signs its downloads);"
		        L" what is extracted from it is checked instead.\n");
	cab = find_payload_cab(installer, installer_len, &cab_len);
	if (!cab || write_file(cab_path, cab, cab_len) != 0) {
		fwprintf(stderr, L"error: the installer does not contain the expected payload\n");
		goto out;
	}
	kr->ready = 1;
	rc = 0;
out:
	free(installer);
	DeleteFileW(exe_path);
	if (rc != 0)
		kr_close(kr);
	return rc;
}

typedef struct {
	kinect_runtime *kr;
	const char *file;
	char out_a[MAX_PATH];
} find_ctx;

static int try_cab(void *p)
{
	find_ctx *c = (find_ctx *)p;
	return cab_extract(c->kr->tmp_a, "inner.cab", c->file, c->out_a);
}

BYTE *kr_extract(kinect_runtime *kr, const char *msi_name, const char *file, size_t *len)
{
	wchar_t msi_path[MAX_PATH], inner[MAX_PATH], out_path[MAX_PATH], wname[64];
	char msi_a[MAX_PATH];
	find_ctx c;
	BYTE *data = NULL;
	if (!kr->ready)
		return NULL;
	MultiByteToWideChar(CP_ACP, 0, msi_name, -1, wname, 64);
	swprintf(msi_path, MAX_PATH, L"%lsmsi-%ls", kr->tmp, wname);
	swprintf(inner, MAX_PATH, L"%lsinner.cab", kr->tmp);
	swprintf(out_path, MAX_PATH, L"%lsextracted.bin", kr->tmp);
	WideCharToMultiByte(CP_ACP, 0, msi_path, -1, msi_a, MAX_PATH, NULL, NULL);
	c.kr = kr;
	c.file = file;
	WideCharToMultiByte(CP_ACP, 0, out_path, -1, c.out_a, MAX_PATH, NULL, NULL);
	if (cab_extract(kr->tmp_a, "payload.cab", msi_name, msi_a) != 0) {
		fwprintf(stderr, L"error: cannot extract package %hs from the installer\n", msi_name);
		goto out;
	}
	if (msi_each_cab(msi_path, inner, try_cab, &c) != 0) {
		fwprintf(stderr, L"error: package %hs does not contain %hs\n", msi_name, file);
		goto out;
	}
	data = read_file(out_path, len);
out:
	DeleteFileW(msi_path);
	DeleteFileW(inner);
	DeleteFileW(out_path);
	return data;
}

void kr_close(kinect_runtime *kr)
{
	wchar_t p[MAX_PATH];
	if (!kr->tmp[0])
		return;
	swprintf(p, MAX_PATH, L"%lspayload.cab", kr->tmp);
	DeleteFileW(p);
	RemoveDirectoryW(kr->tmp);
	kr->tmp[0] = 0;
	kr->ready = 0;
}

int kr_install(const wchar_t *dest_dir, const wchar_t *name, const BYTE *data, size_t len)
{
	wchar_t full[MAX_PATH], path[MAX_PATH];
	if (GetFullPathNameW(dest_dir, MAX_PATH, full, NULL) == 0)
		return -1;
	SHCreateDirectoryExW(NULL, full, NULL);
	swprintf(path, MAX_PATH, L"%ls\\%ls", full, name);
	if (write_file(path, data, len) != 0) {
		fwprintf(stderr, L"error: cannot write %ls\n", path);
		return -1;
	}
	wprintf(L"Installed %ls (%zu bytes, SHA-256 verified).\n", path, len);
	return 0;
}

int kr_sha256_hex(const BYTE *data, size_t len, char hex[65])
{
	return sha256_hex(data, len, hex);
}

/* ---- the firmware ------------------------------------------------------- */

static int check_firmware(const BYTE *fw, size_t len)
{
	char hex[65];
	const DWORD *w = (const DWORD *)fw;
	if (len != FIRMWARE_SIZE) {
		fwprintf(stderr, L"error: the extracted firmware has %zu bytes, expected %u\n", len, FIRMWARE_SIZE);
		return -1;
	}
	if (w[0] != FIRMWARE_MAGIC || w[3] != FIRMWARE_BASE || w[5] != FIRMWARE_ENTRY) {
		fwprintf(stderr, L"error: the extracted firmware has an unexpected header\n");
		return -1;
	}
	if (sha256_hex(fw, len, hex) != 0 || strcmp(hex, FIRMWARE_SHA256) != 0) {
		fwprintf(stderr, L"error: the extracted firmware's SHA-256 does not match\n");
		return -1;
	}
	return 0;
}

int kr_fetch_firmware(kinect_runtime *kr, const wchar_t *dest_dir)
{
	size_t len = 0;
	BYTE *fw = kr_extract(kr, "a0", "UACFirmware", &len);
	int rc = -1;
	if (fw && check_firmware(fw, len) == 0)
		rc = kr_install(dest_dir, UAC_FIRMWARE_NAME, fw, len);
	free(fw);
	return rc;
}

int uac_fetch_firmware(const wchar_t *dest_dir, const wchar_t *local_installer)
{
	kinect_runtime kr;
	int rc;
	if (kr_open(&kr, local_installer) != 0)
		return -1;
	rc = kr_fetch_firmware(&kr, dest_dir);
	kr_close(&kr);
	return rc;
}

/* ---- PE images ---------------------------------------------------------- */

const BYTE *kr_pe_at(const BYTE *img, size_t len, ULONGLONG va, size_t n)
{
	DWORD pe, i;
	WORD nsec, optsize;
	ULONGLONG base, rva;
	const BYTE *opt;
	if (len < 0x40 || img[0] != 'M' || img[1] != 'Z')
		return NULL;
	pe = *(const DWORD *)(img + 0x3C);
	if ((size_t)pe + 24 + 0x70 > len || memcmp(img + pe, "PE\0\0", 4) != 0 ||
	    *(const WORD *)(img + pe + 4) != 0x8664)
		return NULL;
	nsec = *(const WORD *)(img + pe + 6);
	optsize = *(const WORD *)(img + pe + 20);
	opt = img + pe + 24;
	if (*(const WORD *)opt != 0x20B)
		return NULL;
	base = *(const ULONGLONG *)(opt + 24);
	if (va < base)
		return NULL;
	rva = va - base;
	for (i = 0; i < nsec; i++) {
		const BYTE *s = opt + optsize + 40 * i;
		DWORD sva, rawsize, rawptr;
		if ((size_t)(s + 40 - img) > len)
			return NULL;
		sva = *(const DWORD *)(s + 12);
		rawsize = *(const DWORD *)(s + 16);
		rawptr = *(const DWORD *)(s + 20);
		if (rva >= sva && rva + n <= (ULONGLONG)sva + rawsize) {
			ULONGLONG o = rawptr + (rva - sva);
			if (o + n > len)
				return NULL;
			return img + o;
		}
	}
	return NULL;
}

/* ---- the beamformer weights (§40.6.3) ----------------------------------- */

static double dabs(double v) { return v < 0 ? -v : v; }

static DWORD crc32_of(const BYTE *p, size_t n)
{
	DWORD c = 0xFFFFFFFFu;
	size_t i;
	int k;
	for (i = 0; i < n; i++) {
		c ^= p[i];
		for (k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
	}
	return ~c;
}

/* kinect_beam_weights.bin (layout in src/audio_dsp.c) from KinectAudio10.dll,
 * the same as fwfetcher.py --audio-weights. Returns its size, or 0. */
static size_t beam_weights_file(const BYTE *dll, size_t len, BYTE *out)
{
	const BYTE *hdr = kr_pe_at(dll, len, KA10_WEIGHTSET_VA, 0x38), *p;
	ULONGLONG ptr[4];
	size_t o, table = 11 * 256 * 4 * 8;
	int b, k;
	if (!hdr || *(const int *)(hdr + 8) != 4 || *(const int *)(hdr + 12) != 256 || *(const int *)(hdr + 16) != 11)
		return 0;
	memcpy(ptr, hdr + 0x18, sizeof(ptr));
	memcpy(out, "FNBW", 4);
	*(DWORD *)(out + 4) = 1;
	*(DWORD *)(out + 8) = 4;
	*(DWORD *)(out + 12) = 256;
	*(DWORD *)(out + 16) = 11;
	*(DWORD *)(out + 20) = 0;
	o = 24;
	for (b = 0; b < 11; b++) {
		double beam[3];
		p = kr_pe_at(dll, len, ptr[0] + 24 * b, 24);
		if (!p)
			return 0;
		memcpy(beam, p, sizeof(beam));
		if (dabs(beam[0] - (-50 + 10 * b) * 3.14159265358979 / 180.0) > 0.01 || beam[1] != 0.0)
			return 0;
		memcpy(out + o, &beam[0], 8);
		o += 8;
	}
	p = kr_pe_at(dll, len, ptr[1], 4 * 256);
	if (!p)
		return 0;
	for (k = 0; k < 256; k++) {
		float f;
		memcpy(&f, p + 4 * k, 4);
		if (dabs(f - 31.25 * k) > 1e-3)
			return 0;
	}
	for (k = 2; k < 4; k++) {
		p = kr_pe_at(dll, len, ptr[k], table);
		if (!p)
			return 0;
		memcpy(out + o, p, table);
		o += table;
	}
	*(DWORD *)(out + o) = crc32_of(out, o);
	return o + 4;
}

int kr_fetch_audio_weights(kinect_runtime *kr, const wchar_t *dest_dir)
{
	size_t len = 0, n;
	BYTE *dll = kr_extract(kr, "a1", "FILE_x64_KinectAudio10.dll", &len);
	BYTE *out = (BYTE *)malloc(AUDIO_WEIGHTS_SIZE);
	char hex[65];
	int rc = -1;
	if (!dll || !out)
		goto done;
	n = beam_weights_file(dll, len, out);
	if (n != AUDIO_WEIGHTS_SIZE || sha256_hex(out, n, hex) != 0 || strcmp(hex, AUDIO_WEIGHTS_SHA256) != 0) {
		fwprintf(stderr, L"error: the beamformer weights in KinectAudio10.dll are not the known ones; not installing them\n");
		goto done;
	}
	rc = kr_install(dest_dir, AUDIO_WEIGHTS_NAME, out, n);
done:
	free(dll);
	free(out);
	return rc;
}

/* ---- the skeleton tracking models (§30.2, §30.3) ------------------------ */

static const struct {
	const char *name;
	const wchar_t *wname;
	size_t size;
	const char *sha256;
} nui_models[] = {
	{ "DATABASE.WSDF", L"DATABASE.WSDF", 13795880u, "b3d084bd5848ef76de4219f592922c3b1e5379e5ccc975ff8d1f68c426b41164" },
	{ "HEADPOSITION.MSHDF", L"HEADPOSITION.MSHDF", 246487u, "343e3c2a23d4c5baf73af05f232131c9d13a9264f5dacde182106121750535cf" },
};

/* The resource of type `type` named `name` (both strings) in an x64 PE image. */
static const BYTE *pe_resource(const BYTE *img, size_t len, const char *type, const char *name, size_t *out_len)
{
	DWORD pe, rva;
	ULONGLONG base, root;
	const BYTE *p;
	const char *want[2];
	int level;
	ULONGLONG dir;
	if (len < 0x40)
		return NULL;
	pe = *(const DWORD *)(img + 0x3C);
	if ((size_t)pe + 24 + 112 + 24 > len)
		return NULL;
	base = *(const ULONGLONG *)(img + pe + 24 + 24);
	rva = *(const DWORD *)(img + pe + 24 + 112 + 2 * 8);
	if (!rva)
		return NULL;
	root = base + rva;
	want[0] = type;
	want[1] = name;
	dir = root;
	for (level = 0; level < 3; level++) {
		const BYTE *hdr = kr_pe_at(img, len, dir, 16);
		DWORD i, n, found = 0, off = 0;
		if (!hdr)
			return NULL;
		n = (DWORD)*(const WORD *)(hdr + 12) + *(const WORD *)(hdr + 14);
		for (i = 0; i < n && !found; i++) {
			const BYTE *e = kr_pe_at(img, len, dir + 16 + 8 * i, 8);
			DWORD id, o;
			if (!e)
				return NULL;
			id = *(const DWORD *)e;
			o = *(const DWORD *)(e + 4);
			if (level == 2) { /* the first language */
				found = 1;
				off = o;
			} else if (id & 0x80000000u) {
				const BYTE *s = kr_pe_at(img, len, root + (id & 0x7FFFFFFFu), 2);
				WORD ln, j;
				const BYTE *str;
				if (!s)
					return NULL;
				ln = *(const WORD *)s;
				str = kr_pe_at(img, len, root + (id & 0x7FFFFFFFu) + 2, 2u * ln);
				if (!str || strlen(want[level]) != ln)
					continue;
				for (j = 0; j < ln; j++) {
					WORD c = *(const WORD *)(str + 2 * j);
					char a = want[level][j];
					if (c >= 'a' && c <= 'z') c = (WORD)(c - 32);
					if (a >= 'a' && a <= 'z') a = (char)(a - 32);
					if (c != (WORD)(unsigned char)a)
						break;
				}
				if (j == ln) {
					found = 1;
					off = o;
				}
			}
		}
		if (!found)
			return NULL;
		if (level < 2) {
			if (!(off & 0x80000000u))
				return NULL;
			dir = root + (off & 0x7FFFFFFFu);
		} else {
			const BYTE *data = kr_pe_at(img, len, root + off, 8);
			DWORD drva, dsize;
			if (!data)
				return NULL;
			drva = *(const DWORD *)data;
			dsize = *(const DWORD *)(data + 4);
			p = kr_pe_at(img, len, base + drva, dsize);
			if (p)
				*out_len = dsize;
			return p;
		}
	}
	return NULL;
}

int kr_fetch_nui_models(kinect_runtime *kr, const wchar_t *dest_dir)
{
	size_t len = 0, i;
	BYTE *dll = kr_extract(kr, "a1", "FILE_x64_Kinect10.dll", &len);
	const BYTE *res[2];
	size_t res_len[2] = { 0, 0 };
	wchar_t dir[MAX_PATH];
	int rc = -1;
	if (!dll)
		return -1;
	for (i = 0; i < 2; i++) {
		char hex[65];
		res[i] = pe_resource(dll, len, "BINARY", nui_models[i].name, &res_len[i]);
		if (!res[i] || res_len[i] != nui_models[i].size || sha256_hex(res[i], res_len[i], hex) != 0 ||
		    strcmp(hex, nui_models[i].sha256) != 0) {
			fwprintf(stderr, L"error: %hs in Kinect10.dll is not the known model; not installing anything\n", nui_models[i].name);
			goto done;
		}
	}
	swprintf(dir, MAX_PATH, L"%ls\\nui", dest_dir);
	for (i = 0; i < 2; i++)
		if (kr_install(dir, nui_models[i].wname, res[i], res_len[i]) != 0)
			goto done;
	rc = 0;
done:
	free(dll);
	return rc;
}
