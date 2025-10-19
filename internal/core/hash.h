/* SPDX-License-Identifier: MIT */
#ifndef HBF_CORE_HASH_H
#define HBF_CORE_HASH_H

#include <stddef.h>

/*
 * Generate a DNS-safe hash from an input string (e.g., username).
 * The hash is lowercase alphanumeric [0-9a-z], exactly 8 characters.
 * Uses SHA-256 and base-36 encoding for collision resistance.
 *
 * @param input: The input string (e.g., username)
 * @param output: Buffer to store the 8-character hash (must be >= 9 bytes for null terminator)
 * @return 0 on success, -1 on error
 */
int hbf_dns_safe_hash(const char *input, char *output);

#endif /* HBF_CORE_HASH_H */
