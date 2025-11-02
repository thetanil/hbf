/* SPDX-License-Identifier: MIT */
#include "hash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_hash_basic(void)
{
	char hash[9];
	int ret;

	ret = hbf_dns_safe_hash("testuser", hash);
	assert(ret == 0);
	assert(strlen(hash) == 8);
	printf("  ✓ Basic hash generation\n");
}

static void test_hash_deterministic(void)
{
	char hash1[9];
	char hash2[9];
	int ret1, ret2;

	ret1 = hbf_dns_safe_hash("testuser", hash1);
	ret2 = hbf_dns_safe_hash("testuser", hash2);

	assert(ret1 == 0);
	assert(ret2 == 0);
	assert(strcmp(hash1, hash2) == 0);
	printf("  ✓ Hash is deterministic\n");
}

static void test_hash_different_inputs(void)
{
	char hash1[9];
	char hash2[9];
	int ret1, ret2;

	ret1 = hbf_dns_safe_hash("user1", hash1);
	ret2 = hbf_dns_safe_hash("user2", hash2);

	assert(ret1 == 0);
	assert(ret2 == 0);
	assert(strcmp(hash1, hash2) != 0);
	printf("  ✓ Different inputs produce different hashes\n");
}

static void test_hash_dns_safe_chars(void)
{
	char hash[9];
	int ret;
	int i;

	ret = hbf_dns_safe_hash("testuser", hash);
	assert(ret == 0);

	for (i = 0; i < 8; i++) {
		assert((hash[i] >= '0' && hash[i] <= '9') ||
		       (hash[i] >= 'a' && hash[i] <= 'z'));
	}
	printf("  ✓ Hash contains only DNS-safe characters [0-9a-z]\n");
}

static void test_hash_null_inputs(void)
{
	char hash[9];
	int ret;

	ret = hbf_dns_safe_hash(NULL, hash);
	assert(ret == -1);

	ret = hbf_dns_safe_hash("test", NULL);
	assert(ret == -1);

	printf("  ✓ NULL input handling\n");
}

int main(void)
{
	printf("Running hash_test.c:\n");

	test_hash_basic();
	test_hash_deterministic();
	test_hash_different_inputs();
	test_hash_dns_safe_chars();
	test_hash_null_inputs();

	printf("\nAll tests passed!\n");
	return 0;
}
