# Optics

Metrics gathering library for high-throughput data which supports polling by an
external agent.


## Building

Building:
```
cmake . -G Ninja
ninja
```

Tests:
```
ctest -V . -L test
ctest -V . -L valgrind
ctest -V . -L bench
```


### Usage

To log metrics refer to this [example](test/example.c) which describes all the
basics.

Polling can be done using the `opticsd` binary like so:

```
	opticsd --freq=10 --dump-stdout --dump-carbon=127.0.0.1:2003
```

The daemon will automatically pick-up any newly created optics instances and
start dumping them to its configured backends.
