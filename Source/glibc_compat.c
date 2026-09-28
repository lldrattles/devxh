/**
 * DevXH glibc compatibility shim (link-time symbol backports).
 *
 * The release binary targets glibc >= 2.35 (Ubuntu 22.04 LTS). Vendored
 * dependencies compiled on newer build hosts can pick up newer glibc-only
 * entry points, which would silently raise the runtime floor:
 *
 *   - libzt (C++ sources against a glibc >= 2.38 host) references the C23
 *     strtol family redirects __isoc23_strtol/strtoll/strtoul/strtoull
 *     (glibc 2.38+). Behaviour difference vs classic strtol is only C23
 *     "0b"/"0B" binary-prefix parsing, which libzt never feeds it.
 *   - vendored libsodium (randombytes_sysrandom / randombytes_internal_random)
 *     references arc4random() when the host advertises it (glibc 2.36+).
 *
 * This translation unit is force-linked (--whole-archive) into the server
 * executable and provides those symbols locally, so the linker binds them
 * here instead of emitting versioned requirements on glibc 2.36/2.38.
 * On build hosts where nothing references them, the linker simply discards
 * the unused definitions (or they sit unused; both are harmless).
 *
 * arc4random() is implemented over getentropy(2) (Linux 3.17+, glibc 2.25+),
 * which is what glibc's own arc4random uses under the hood.
 */

#include <stdlib.h>

long __isoc23_strtol(const char *nptr, char **endptr, int base)
{
	return strtol(nptr, endptr, base);
}

long long __isoc23_strtoll(const char *nptr, char **endptr, int base)
{
	return strtoll(nptr, endptr, base);
}

unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base)
{
	return strtoul(nptr, endptr, base);
}

unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base)
{
	return strtoull(nptr, endptr, base);
}

#include <sys/random.h>

unsigned int arc4random(void)
{
	unsigned int r;
	int tries = 32;

	while (tries-- > 0) {
		if (getentropy(&r, sizeof(r)) == 0)
			return r;
	}
	/* Unrecoverable entropy failure: fail loudly rather than return
	 * predictable output to cryptographic consumers (libsodium). */
	abort();
}
