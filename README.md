# Optics

Metrics gathering library for high-throughput data which supports polling by an
external agent.


## Building

Dependencies:
- cmake
- librt
- libbsd

Optional Dependencies:
- ninja (faster then `gmake`)
- cmocka (tests are disabled if not present)
- valgrind (run with `ctest -V . -L valgrind`)
- ubsan (enable via `cmake -DENABLE_UBSAN`)
- clang-tidy (run with `ninja clang-tidy`)

Building:
```
cmake . -G Ninja
ninja
ninja test
```

Options:

- `cmake . -G Ninja`: use ninja which is generally faster then gmake
- `cmake . -DENABLE_UBSAN=ON`: compile with the gcc undefined behaviour sanitizer
- `ctest -V . -L test`: Run only the funcitonal tests
- `ctest -V . -L valgrind`: Run only the valgrind tests
- `ctest -V . -L bench`: run only the benchmarks


### Usage

To log metrics refer to this [example](test/example.c) which describes all the
basics.

Polling can be done using the `opticsd` binary like so:

```
	opticsd --freq=10 --dump-stdout --dump-carbon=127.0.0.1:2003
```

The daemon will automatically pick-up any newly created optics instances and
start dumping them to its configured backends. Use the `--help` argument for
more details.


### Pre-emptive Nit-picking

* Assumes amd64.
* Not POSIX compliant.
