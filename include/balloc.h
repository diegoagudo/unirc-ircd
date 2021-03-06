/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

/*! \file balloc.h
 * \brief A block allocator
 * \version $Id: balloc.h 403 2006-06-14 05:04:42Z jon $
 * \todo Get rid of all typedefs in this file
 */

#ifndef INCLUDED_balloc_h
#define INCLUDED_balloc_h

#include "setup.h"

#ifndef NOBALLOC
#include "client.h"
#include "tools.h"
#include "memory.h"
#include "ircd_defs.h"


/*! \brief Block contains status information for
 *         an allocated block in our heap.
 */
struct Block
{
	int freeElems;		/*!< Number of available elems */
	size_t alloc_size;	/*!< Size of data space for each block */
	struct Block *next;	/*!< Next in our chain of blocks */
	void *elems;		/*!< Points to allocated memory */
	dlink_list free_list;	/*!< Chain of free memory blocks */
};

typedef struct Block Block;

struct MemBlock
{
	dlink_node self;	/*!< Node for linking into free_list or used_list */
	Block *block;		/*!< Which block we belong to */
};
typedef struct MemBlock MemBlock;

/*! \brief BlockHeap contains the information for the root node of the
 *         memory heap.
 */
struct BlockHeap
{
	size_t elemSize;	/*!< Size of each element to be stored */
	int elemsPerBlock;	/*!< Number of elements per block */
	int blocksAllocated;	/*!< Number of blocks allocated */
	int freeElems;		/*!< Number of free elements */
	Block *base;		/*!< Pointer to first block */
	const char *name;	/*!< Name of the heap */
	struct BlockHeap *next;	/*!< Pointer to next heap */
};

typedef struct BlockHeap BlockHeap;


extern int BlockHeapFree(BlockHeap *, void *);
extern void *BlockHeapAlloc(BlockHeap *);

extern BlockHeap *BlockHeapCreate(const char *const, size_t, int);
extern int BlockHeapDestroy(BlockHeap *);
extern void initBlockHeap(void);
extern void block_heap_report_stats(struct Client *);
#else /* NOBALLOC */

typedef struct BlockHeap BlockHeap;
/* This is really kludgy, passing ints as pointers is always bad. */
#define BlockHeapCreate(blah, es, epb) ((BlockHeap*)(es))
#define BlockHeapAlloc(x) MyMalloc((int)x)
#define BlockHeapFree(x,y) MyFree(y)
#endif /* NOBALLOC */
#endif /* INCLUDED_balloc_h */
