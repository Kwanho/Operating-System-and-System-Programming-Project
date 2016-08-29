/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

struct chunk_info *heap_base = NULL;
size_t info_size = sizeof(struct chunk_info);

void *mm_malloc(size_t size) {
    /* YOUR CODE HERE */

	if (size == 0){
		return NULL;
	}
	
	struct rlimit curr_limits;
	getrlimit(RLIMIT_DATA, &curr_limits);
	//rlim_t limit = curr_limits.rlim_cur;

	//check with rlimit
	if (size > 9999) {
		return NULL;
	}

	struct chunk_info *prev_chunk = NULL;
	struct chunk_info *curr_chunk = heap_base;

	//find free chunk
	while(curr_chunk!=NULL){
		if (curr_chunk->free == 1 && curr_chunk->size >= size){

			//try split
			if(curr_chunk->size >= size + info_size){
				//init split_chunk
				struct chunk_info *split_chunk = curr_chunk->data + size;

				split_chunk->prev = curr_chunk;
				split_chunk->next = curr_chunk->next;
				split_chunk->free = 1;
				split_chunk->size = curr_chunk->size - size - info_size;

				//update curr_chunk
				curr_chunk->next = split_chunk;
				curr_chunk->size = size;
			}
			
			//update curr_chunk
			curr_chunk->free = 0;
			
			memset(curr_chunk->data, 0, curr_chunk->size);

			return curr_chunk->data;
		}

		prev_chunk = curr_chunk;
		curr_chunk = curr_chunk->next;
	}

	//If free chunk is not found, move break to increase the size of the mapped region. 

	//first chunk_info
	if (heap_base == NULL) {
		heap_base = sbrk(0);
	}

	curr_chunk = sbrk(info_size + size);

	//init current chunk_info
	if (prev_chunk!=NULL){
		prev_chunk->next = curr_chunk;
		curr_chunk->prev = prev_chunk;
	} else {
		curr_chunk->prev = NULL;
	}
	
	curr_chunk->next = NULL;
	curr_chunk->free = 0;
	curr_chunk->size = size;
	memset(curr_chunk->data, 0, curr_chunk->size);

	return curr_chunk->data;
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */

	if (size == 0) {
		return NULL;
	}

	void *new_ptr;

	if (ptr != NULL){
		struct chunk_info *curr = ptr - info_size;

		new_ptr = mm_malloc(size);
		if (new_ptr==NULL){
			return NULL;
		}

		//struct chunk_info *new_chunk_info = new_ptr - info_size;
		if (curr->size < size){
			memcpy(new_ptr, ptr, curr->size);
		} else{
			memcpy(new_ptr, ptr, size);
		}

		//free old chunk
		mm_free(ptr);

	} else {
		new_ptr = mm_malloc(size);
	}	

    return new_ptr;

    return new_ptr;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */

	if (ptr!=NULL){

		struct chunk_info *curr_chunk = ptr - info_size;
		struct chunk_info *prev_chunk = curr_chunk->prev;
		struct chunk_info *next_chunk = curr_chunk->next;

		//mark current chunk free
		curr_chunk->free = 1;
		
		//fuse next
		if(next_chunk!=NULL && next_chunk->free==1){
			curr_chunk->size += info_size + next_chunk->size;
			curr_chunk->next = next_chunk->next;
		}
		//fuse prev
		if(prev_chunk!=NULL && prev_chunk->free==1){
			prev_chunk->size += info_size + curr_chunk->size;
			prev_chunk->next = curr_chunk->next;
		}
	}
}


