/*
 * mm_alloc.h
 *
 * Exports a clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>

struct chunk_info
{
	struct chunk_info *prev;
	struct chunk_info *next;
	int free;
	size_t size;
	char data[0];
};

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);