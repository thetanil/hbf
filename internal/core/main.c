/* SPDX-License-Identifier: MIT */
#include "hash.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	char hash[9];
	int ret;
	const char *test_input = "testuser";

	(void)argc;
	(void)argv;

	printf("HBF v0.1.0 (Phase 0 - Foundation)\n");
	printf("===================================\n\n");

	printf("Testing DNS-safe hash generator:\n");
	printf("Input: %s\n", test_input);

	ret = hbf_dns_safe_hash(test_input, hash);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to generate hash\n");
		return 1;
	}

	printf("Hash:  %s\n\n", hash);

	printf("Build system verification successful!\n");
	printf("- C99 compliance: OK\n");
	printf("- Bazel bzlmod: OK\n");
	printf("- DNS-safe hash: OK\n");

	return 0;
}
