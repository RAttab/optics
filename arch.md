# Arch

Basic overview of the archictecture for optics.

Keep in mind that optics was designed to minimize the overhead of recording
values to lens. This means that we try to minimize contention between recording
and polling of lenses and that creating and free-ing lenses are not considered
critical at the moment.


## Regions

Regions are shm files mmap-ed into memory which hold all the lenses data so that
they can be shared with the external polling agent. Since we support a variable
number of lens created and freed at runtime, this requires that we implement the
following features:

- Growing the mmap region
- In region allocator

### Growth

Growing a region without affecting ongoing concurrent write is fairly tricky as
it prevents us from straight up moving the vma (ie. Virtual Memory Area) without
syncrhonizing with all ongoing writes.

The current solution is to cheat and remap the same file into memory multiple
times. This allows the usage of pointers into the region which will remain valid
as we grow the region since we never get rid of the old mappings. Note that this
does inflate the virtual memory stats of the process by a factor of 2 and
potentially add redundant entries in the TLB.

Note that we considered the possibility of allocating a huge vma (eg. 4Gb and
up) but that caused valgrind to become almost unusable as it would grind for
several seconds on every test cases. That would additionally inflate the virtual
memory stats of every process that include optics which is not pretty and could
cause pointless debates.


### Allocation

The decision to go for an in-region allocator hedged on our need to synchronize
with the poller to free lenses. The current implementation, uses a lightweight
EBR-like scheme to free lenses in the polling agent process which means that our
allocator must be in the region.

The allocator itself is a very simple free-list allocator segmented into
multiple predefined size classes. Synchronization is done through a single
global lock since we don't consider this part of the code to be a critical path.


## Polling

Polling has to happen periodically by a poller and has the following goals:

* Read the value of all lens.
* Publish the lens values to the registered backends.
* Reset the value of all lens.

Note that polling resets the values of the lenses which means that there can
only be one active poller at a time.


### Epoch

Optics uses an epoch mechanism to accomplish 2 goals:

* Snapshot and reset all the metrics for polling.
* Defer the de-allocation of metrics that are no longer needed.


#### Snapshots

The goal of the snapshotting mechanism is to reduce contention between the
polling thread and the recording threads while also providing an easy way to
snapshot and reset the lens data.

The idea is to maintain 2 version of every lens and use a global epoch counter
to determine which of the two should be used for recording a new value. The
poller can then increment the epoch counter whenever it is ready to read the
region which will transition all writes to the new versions of the lenses. We
can then minize the number of stragglers by having the poller wait a grace
period before it proceeds to read the lenses on the epoch that was vacated.

By avoiding contention in this manner, reading and resetting a lens has very
little impact on the writting threads while still allowing non-trivial lens
implementations. As a bonus we also get a relatively stable snapshot of the
lenses we're reading which can mitigate the effect of long polling time (polling
that takes longer then a second).


#### Defered de-allocations

To traverse all the lens in a region in a lock-free manner, each lens is
inserted in a linked list. This brings up the classical problem of when to free
the node when it is removed from the linked list assuming that there could be a
polling thread traversing it at any moment.

The solution implemented is a sort-of lightweight EBR scheme that exploits the
fact that there's only one thread that will be traversing the linked list. This
allows us to piggy-back on the existing epoch mechanism used to create snapshots
to maintain a list per-epoch of defered-freed lenses which will be cleaned up
before we increment the epoch counter in the poller.
