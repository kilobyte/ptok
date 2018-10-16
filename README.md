Options I have thought of so far:

Cuckoo
======

Existing algorithm, needs thread safety.

### With a rwlock

A "multiple readers or one writer" lock.

* Pro: simple; cuckoo already tested
* Con: "stop the world" on writes, with massive latency for everyone

### RCU + deletion lock

We copy the table on every write; special kind of lock lets us know when to
free.

#### Deletion lock

The read lock is a single integer, and never blocks:
```
atomic_increment(r); // INC
... read
if (!atomic_fetch_add(r, -1)) // XADD
    free();
```

There's also a regular mutex for writes only.

After write we also do an unbalanced decrement:
```
atomic_decrement(r);
```
This eventually makes the lock drop to -1 (atomic_fetch_add gives the _old_
value thus 0), this makes the writer or one of readers free the old version.

* Pro: wait-free reads (but read thread may need to do the freeing)
* Con: writes need to rewrite everything

### ~~RCU + leaks~~

No freeing.

* Pro: no read locks at all
* Con: writes need to rewrite everything, ***leak*** everything.  32M
  entries = 1GB leaked on every write.  Hahaha no.

(Leaks here can be still tracked, there's just no safe way to free them
while the hashmap exists unless all users are stopped.)

Dynamic perfect hash
====================

[Explained here](https://en.wikipedia.org/wiki/Dynamic_perfect_hashing);
more complex than Cuckoo but also needs to check only two hash entries
(albeit in different tables).  The data is split into small pieces that can
be locked separately, except for whole-table rewrites; we can nearly
eliminate those by growing by huge amounts (1 → 256 → 65536 → 16777216 → 4G
entries).  Old tables can even be RCU leaked — we don't care about pieces so
much smaller than live data that leak only once.

### RCU + deletion lock

Unlike cuckoo, only the 2nd-level-tables (buckets) need locking; the
algorithm has to rewrite the whole bucket anyway so RCU comes naturally (but
buckets are very small).

### RCU + leaks

Individual buckets we leak are small so this might be acceptable, at least
unless there's a lot of remove+write cycles.

Radix with tail compression
===========================

Slices of 8, 11, 13 or 16 bits.

Every table has two additional fields:
* only_key
* only_val

As long as there's only a single entry in this whole subtree, **only_key**
holds its key, **only_val** the value.  Otherwise, **only_key** is zero and
we need to traverse to one of child nodes, which also might be compressed or
not.  We are not supposed to ever get keys of 0 but we still handle them
correctly (just slower).

A compressed subtree stores an uncompressed copy as well, readers merely
don't traverse it.

Writing an entry into an already allocated non-compressed subtree is
obvious and lock+wait-free.

A write into compressed writes 0 to **only_key**, doesn't matter if before
or after the write.

A write that needs to allocate, does so into an unconnected subtree, then
```atomic_compare_exchange(subtree_pointer, 0, new_subtree)```; if this
fails, someone else just allocated the subtree for us so we free what we
just allocated and retry.

* Pro: handles the 1-entry case efficiently, 2-entry in 255/256 cases
* Pro: completely lock-free, both for readers and writers
* Pro: not slowed at all by writes, no memory churn
* Con: reads many cache-lines

Traditional radix
=================

* Pro: also completely lock-free, not slowed by writes
* Pro: with many entries, half the cacheline accesses of a compressed radix
* Con: with 1 entry still needs to traverse the whole tree depth


Lessons learned so far:
=======================

Any synchronization, even by a single asm instruction, makes things around
100 times slower than even a slow algorithm with many cacheline accesses.
Thus, anything not lock-free in the read path is not an option.

So cuckoo is out.  This leaves dyn perfect hash and radixes.  But for dyn
perfect hash, rehashing would require RCU which needs either some sort of
synchronization or a way of knowing that all readers have exited (they
usually exit in micro/nanoseconds but sometimes a thread can get blocked for
hours).

Radixes are not even O(1) but O(word size) with many cacheline accesses but
so far seem to work fastest.  I'm unsure if my test cases are realistic
enough, though (esp. wrt. a programs's actual activity using up cache).

Tail-compressed radix has twice as many cacheline reads but for non-crafted
data works better: as long as tails are unique they don't need to be read.

A downside of radixes is drastically higher memory use.  This can be reduced by
small slices, at the cost of speed:
* cuckoo at 40% utilization needs 10 words per entry
* regular radix: slice=8: 2048 words; slice=4: 256 words
* tail compressed radix: worst case slightly worse than regular, otherwise
  depends on path uniqueness

Questions:
==========

* Are negative answers (ie, entries that are [no longer] in the hashmap) a
  valid usage?  Not for *obj_direct as that'd crash anyway, but will other
  users care?  In current implementation, there's a race between deletion
  and read; fixable at the cost of either:
  * never freeing deleted entries (can be collected with a sweep if we know
    readers don't access those) -- unless swept memory use would accumulate
    unbounded
  * complicating the algorithm a bit, an extra memory access from an already
    loaded cacheline, freeing deleted entries to a pool rather than malloc
    (memory use doesn't grow unless content grows), ~2x as much memory
    needed

  Neither of these is needed for *obj_direct.

* Are keys random (uuids are)?  Tail compression works badly (but correctly)
  if heads are shared:
  ```
  00000001
  00000002
  -- need to process 8 slices, disambiguated only at last position
  10000000
  20000000
  -- disambiguated at 1st position
  ```
  Trivially changeable by little-vs-big endian tails; can even use any
  permutation: malloced addresses differ near the end but have same last
  couple of slices.
