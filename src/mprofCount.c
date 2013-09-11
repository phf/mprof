/*
	Copyright (c) 2013 by Alexander Haase ( https://www.alexanderhaase.net )
	Project: ahmalloc ( https://www.alexanderhaase.net/projects/ahmalloc.html )
	Created: 2013-09-10
	License: LGPL

	Module that counts application allocations

	TODO: rewrite with mmap to avoid destructor worries, maybe leverage MprofRecord?
*/
#include <mprofCount.h>
#include <string.h>
#include <stdio.h>
#include <mprofRecord.h>
#include <semaphore.h>
#include <assert.h>

struct MprofAllocCount {
	size_t malloc;
	size_t free;
	size_t calloc;
	size_t realloc;
};

__thread struct MprofAllocCount * mprofLocalCounts = NULL;

static struct mmapArea countsArea = MMAP_AREA_NULL; 
static sem_t mmapSem;
volatile size_t threadID = 0;

static void mprofCountConstruct( void ) {
	sem_init( &mmapSem, 0, 1 );
	assert( mmapOpen( &countsArea, "./mprof.counts", true ) );
	assert( mmapSize( &countsArea, sizeof( struct MprofAllocCount ), MMAP_AREA_SET ) );
}

static void mprofCountDestruct( void ) {
	sem_destroy( &mmapSem );
	mmapClose( &countsArea );
}

static void mprofCountThreadInit( void ) {
	if( mprofLocalCounts == NULL ) {
		//get an index into the mmapArea
		size_t mmapIndex = __sync_fetch_and_add( &threadID, 1 );

		//expand the mmap area if it's too small
		const size_t minSize = ( 1 + mmapIndex ) * sizeof( struct MprofAllocCount );
		if( minSize < countsArea.fileSize ) {
			sem_wait( &mmapSem );
			if( minSize < countsArea.fileSize ) {
				assert( mmapSize( &countsArea, minSize, MMAP_AREA_SET ) );
			}
			sem_post( &mmapSem );
		}

		//make a pointer
		mprofLocalCounts = ( (struct MprofAllocCount *) countsArea.base ) + mmapIndex;
	}
}

static void * mallocCount( size_t in_size ) {
	mprofCountThreadInit();
	mprofLocalCounts->malloc += 1;
	return defaultVtable.malloc( in_size );
}

static void * callocCount( size_t in_size, size_t in_qty ) {
	mprofCountThreadInit();
	mprofLocalCounts->calloc += 1;
	return defaultVtable.calloc( in_size, in_qty );
}

static void freeCount( void * in_ptr ) {
	mprofCountThreadInit();
	mprofLocalCounts->free += 1;
	defaultVtable.free( in_ptr );
}

static void * reallocCount( void * in_ptr, size_t in_size ) {
	mprofCountThreadInit();
	mprofLocalCounts->realloc += 1;
	return defaultVtable.realloc( in_ptr, in_size );
}

/*
static void mprofCountPrintf( const struct MprofAllocCount * counts ) {
	printf( "malloc:\t%llu\tfree:\t%llu\tcalloc:\t%llu\trealloc:\t%llu", 
		(unsigned long long) counts->malloc, 
		(unsigned long long) counts->free, 
		(unsigned long long) counts->calloc, 
		(unsigned long long) counts->realloc );
}*/

const struct AllocatorVtable mprofCountVtable = { &mallocCount, &freeCount, &callocCount, &reallocCount, &mprofCountConstruct, &mprofCountDestruct, "Count" };
