# Arch

Basic overview of the archictecture for optics.


## Regions

Regions are shm files mmap-ed into memory which hold all the lenses data so that
they can be shared with the external polling agent.


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
period before it proceeds to read the lenses.

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
