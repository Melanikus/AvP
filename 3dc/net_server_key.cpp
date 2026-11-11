// The list's key from its certificate (net_server_key.h says why only the key). Written 2026-11-10 by Melanikus for the AvP port (#175).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include <string.h>

#include "net_server_key.h"
#include "net_crypto.h"

// Windows' own certificate code, which comes with Windows
#pragma comment(lib, "crypt32.lib")

bool NetServerKey_OfCertificate(const uint8_t *certificate, size_t certificateSize, uint8_t *key, size_t room, size_t *keySize)
{
	if (keySize)
	{
		*keySize = 0;
	}

	if (!key || !keySize || room == 0 || room > 0xFFFFFFFFu)
	{
		return false;
	}

	NetCrypto_Wipe(key, room);

	if (!certificate || certificateSize == 0 || certificateSize > kNetServerKeyMostCertificateBytes)
	{
		return false;
	}

	PCCERT_CONTEXT const read = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, certificate, static_cast<DWORD>(certificateSize));

	if (!read || !read->pCertInfo)
	{
		if (read)
		{
			CertFreeCertificateContext(read);
		}

		return false;
	}

	// First how long the key's DER is, then the DER itself, only if it fits
	DWORD needed = 0;
	bool ok = CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, &read->pCertInfo->SubjectPublicKeyInfo, 0, NULL, NULL, &needed) &&
		needed > 0 && needed <= room;
	DWORD written = needed;
	ok = ok && CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, &read->pCertInfo->SubjectPublicKeyInfo, 0, NULL, key, &written) &&
		written > 0 && written <= needed;

	CertFreeCertificateContext(read);

	if (!ok)
	{
		NetCrypto_Wipe(key, room);
		return false;
	}

	*keySize = written;
	return true;
}
