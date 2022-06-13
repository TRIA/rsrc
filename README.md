## Overview of 'rsrc'
Resource object manager overlay for malloc/free

When writing code using malloc/free, keeping track of heap usage and the objects stored there can be a hassle.  Figuring out where the memory is going and what each is being used for is often object-specific and tedious, and finding memory leaks can be hard.  This is an overlay to malloc/free that provides a base set of services that
- Provide a common API to allocate/free/find/print "resources"/objects of the same type, while collecting usage statistics.
- Hold a mutable 'name' associated with each resource that can be used to identify its owner or current usage; changeable as that changes.
- Provide for pools of statically-allocated blocks of fixed-size resources that are held forever, and maintain their own free list for rapid allocation.
- Provide for pools of dynamically allocated fixed or variable-length resources, allocated from the heap as needed, tracked and managed, then given back to the heap when done.

Complete API information is included in the source code using doxygen conventions.  This version has been adapted to use FreeRTOS naming conventions, but should compile and run on any POSIX-ish system with a modern C compiler.
### Key APIs
Creating Pools:

- **pool handle = pxRsrcNewPool (pool name, size of each resource, initial allocation, incremental allocation, max number of resources)**, when either initial or incremental allocation is >1, used to create static Pools that have permanently allocated resources that are kept in a freelist when not in use
- **pool handle = pxRsrcNewVarPool (pool name, maximum number of resources it can contain)**, creates a Pool that contains variable-length resources, with the length specified on each allocation, and has no freelist
- **pool handle = pxRsrcNewDynPool (pool name, size of each resource, max number of resources)**, creates a Pool that dynamically allocates resources from the heap when needed, and returns them to the heap when freed


Note that pxRsrcNewVarPool and pxRsrcNewDynPool are simply convenience functions over pxRsrcNewPool with specific argument values.

Allocating from a Pool:

- **resource handle = pxRsrcAlloc (pool handle to , tracking name string)**, allocates a resource from any type of Pool except a Variable pool (since the length of a resource is not known in that case)
- **resource handle = pxRsrcVarAlloc (pool handle to Var Pool, tracking name string, resource size)**, allocates a resource of the specified size from the Pool

Miscellanous:

- **vRsrcRenameRsrc (resource handle, new target name string to associate with the resource)**
- **vRsrcPrintShort (pool handle or NULL)**, print a summary of usage statistics for the pool, or all pools if NULL
- **vRsrcPrintLong (pool handle or NULL)**, same as PrintShort, but also prints a summary of each resource in the specified Pool(s))

### Simple Example:
rsrcPoolP_t bufferpool = pxRsrcNewVarPool ("Buffers", MAXNUMBUFS); // create buffer pool

bufferP_t buf = (bufferP_t) pxRsrcVarAlloc (bufferpool, "WIFI_XMITBUF", WIFI_XMIT_BUF_SIZE);  // allocate a buffer

vRsrcPrintShort (bufferpool);  // prints hiwater/lowater/active/free/numallocs for the bufferpool

vRsrcPrintShort (NULL);   //prints the stats for ALL active pools

vRsrcPrintLong (bufferpool); // prints pool info, and name tag and other info on each active resource
vRsrcFree (buf);
`
## Background
This data structure allocator was written several years ago for a network project that used a lot of dynamically allocated objects.  It manages pools of dynamically-allocated structures of a given type, called (sorry, it’s an often-used word) “resources”, which we'll call "rsrc"s, and besides doing typical allocation/deallocation, it gathers basic statistics (active, free, total allocs, hiwater, lowater), allows pinning the max number of allocations from the pool that will succeed, and provides generic printing of pool and resource information.  I found it incredibly helpful during debugging, and to avoid and find memory leaks.  I used it for buffers, flows, state machines, message headers, basically anything that was being dynamically allocated.  I  logged the pool statistics for all pools on a heartbeat timer, and used that to track behavior under load and memory usage/leaks at idle.

## Implementation
There are two data structures involved: a resource Pool struct, one per pool (there can be >1 pool with resources of the same type, there are no restrictions on what a pool contains), and a per-resource prefix struct that goes before the front of each structure.

To use it, you first allocate a Pool.  The Pool data struct keeps track of things like the name of the resource pool, the current number of free and allocated resources, a list head for all the in-use resources, optional alloc/free/print helper routine pointers for that resource type, statistics of usage, and a freelist with the available free resources in the pool.  Pool structures are never freed (at present, though this may be changed), and to minimize heap fragmentation it's best to allocate them early in the run.  Once you have a pool defined, you can allocate resources from it and free them, and perform other useful operations like printing summaries of usage.
Pool structures are linked together into a linked list when allocated, which allows a “short” status printing routine to trivially walk the list and print the number of in-use and free resources for all pools of resources, by pool name.  This is handy for logging or an occasional status/sanity check.  A "long" verbose print routine additionally runs through the in-use-resource list for each pool, printing the name associated with the resource (by convention, indicating the last significant change in usage), then calling the custom print routine (if provided) for each resource.  That’s a handy thing to call in fatal situations, or in the debugger.

A set of zero or more resources are at any instant being managed by a Pool, in the active and/or free lists. There are two strategies implemented for resources: statically allocated in blocks, and permanently either in the active or free list, and dynamically allocated, where each resource not in the active list for the pool is returned to the heap.

Static pools comprise an initial allocation of resources (can be 0) allocated when the pool is created, and an increment number (can be 0) that will be added whenever the freelist goes empty and a resource is requested -- in a static pool, at least one of those allocations must be 2 or more.  In a dynamic pool, initial and/or incremental allocation sizes must be 1.  (Pool structures are themselves resources, but due to their permanent nature they are generally allocated using a static pool.)  

If a heap allocation attempt to expand a pool fails, the default action is to abort() after printing which pool was being expanded and how many items were requested.  However before taking that action, a check is made to see if the user has set an out-of-memory user function.  If so, it will be called, and if it returns, then NULL is returned from the allocation request and no abort() is done.  Pools can optionally have a maximum size set at creation time; this is the sum of the counts of resources in the active and free list.  The value 0 means "unlimited", which will cause out-of-memory action when such a pool cannot be expanded to fulfil a request.  When a pool with a non-zero maximum size limit reaches its limit, attempts to allocate from it will return a NULL result and no out-of-memory situation is triggered.

Each resource in memory has a prefix struct in front of the allocated data portion, basically the same way malloc’ed storage works.  Overhead is currently 4 pointers long (as compared to malloc’s 1 or 2, depending on alignment requirements and algorithm, plus loss to internal and external fragmentation).  The prefix links the resource into either the in-use or free list, has a pointer to a "tracking name" string (const char *) that is assigned to the resource, and has a pointer back to the pool it belongs to.  The name is set initially at allocation time (often to the allocating function or purpose), and can be changed whenever you want, which is useful for tracking the travel of a resource (for example, the last function that touched it or the name of a queue it was put on).  That (invisible to structure users) header is then followed by the actual resource data, the address of which all the users of the resource use normally, unaware of the resource prefix’s presence.  

To a user, using rsrc looks a lot like using malloc/free, so existing code works without change other than a different call for alloc/free’ing, and the option to change the tracking name on the resource.  For static pools, the allocate/free CPU overhead is lower than malloc, and there’s double-free detection implemented by detecting an attempt to free a resource that has been marked in the prefix as already being in the freelist.

### Helper Routines
A very useful feature is being able to assign "helper" functions to a pool.  A Pool can have helper routines associated with it, and if provided they are called by pool management routines at appropriate times.  The helpers can be (re-)set at any time, and are optional.  If a helper is NULL, which is the default for new pools (other than the print helper for the Pool pool), no helper action is performed.  

The Alloc helper function is called just before returning a newly-allocated resource, and can initialize the resource, for example, to initialize linked list headers, allocate dynamic pointed-to structures, add the resource to a list, etc.  This can simplify usage of a resource type and reduce opportunites for errors by performing common initialization operations once, in one place.

The Free helper routine is called before adding a freed resource to the freelist.  It can unlink the resource from lists, free pointed-to memory, etc.  it is EXTREMELY important for this code to be cautious about null or stale pointers, since a resource can theoreticallly be in any state when it is freed.  

The Print helper routine is called when the built-in print routines are asked to print a resource.  It can be used to provide a custom printout of the struct, which is a huge time-saver when debugging vs. poking with the debugger or decoding hex.  As in the case for Free, it is important to be cautious of things like stale pointers (and especially NULL pointers), as the resource may be in virtually any state when printed.  A simple PrintInHex routine is provided and can serve as a helper before, or instead of, writing a custom one for a resource type.

### Doubly-Linked Lists
To replace my own list management routines, I adopted the Amazon IoT list routines (https://github.com/aws/amazon-freertos/blob/main/libraries/c_sdk/standard/common/include/private/iot_doubly_linked_list.h), because they use the same in-struct usage pattern and calling sequence as my own that were used in the original and can coexist with the FreeRTOS kernel list routines. These are well-documented, do not name-clash with the kernel list routines, and all operations are macros that expand inline, so it was very painless to integrate.  In-struct lists are also a much better match for the rsrc concept than pointer-based lists, and for lots of other things.

## Notes
I found this package to be incredibly useful in my RINA prototype.  I called the “brief” resource logging routine on a heartbeat timer, and used it to watch resource usage go up and down as tests ran.  If I saw that a pool was leaking resources (didn't return to 0-in-use when idle, or in-use growing unbounded), I would simply call the verbose printing routine and the culprit was often obvious from the names of the last places that touched the resource type.  You don’t have to change any files other than your own in order to add new resource types, and all the infrastructure “just works” automatically for new ones.  So it's easy to adopt code that currently uses malloc/free to use this instead.
