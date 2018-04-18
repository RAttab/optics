# Optics

Metrics gathering library for high-throughput data which supports polling by an
external agent.


## Building

Dependencies:
- cmake
- libbsd
- libdaemon
- libmicrohttpd

Optional Dependencies:
- ninja (faster than `gmake`)
- cmocka (tests are disabled if not present)
- valgrind (run with `ctest -V . -L valgrind`)
- ubsan (part of clang - enabled via `cmake -DENABLE_UBSAN`)
- clang-tidy (run with `ninja clang-tidy`)

Building:
```
cmake . -G Ninja
ninja
ninja test
sudo ninja install
```

Out-of-source build (recommended for devs):
```
mkdir build
(cd build; cmake .. -G Ninja)
ninja -C build
ninja -C build test
sudo ninja -C build install
```

Options:
- `cmake . -G Ninja`: use ninja which is generally faster than gmake
- `cmake . -DENABLE_UBSAN=ON`: compile with the gcc undefined behaviour sanitizer
- `cmake . -DCMAKE_INSTALL_PREFIX=/path/to/install`: where files should be
  installed (defaults to `/usr/local`)
- `ninja clang-tidy`: compile opticsd with clang-tidy
- `ctest -V . -L test`: Run only the functional tests
- `ctest -V . -L valgrind`: Run only the valgrind tests
- `ctest -V . -L bench`: run only the benchmarks


If you'd rather use Docker, you can build your own image with this command:
```
docker image build --tag optics:0.1.13 .
```

To run the Docker image, you need to enable IPC between the container and the host,
and map a local port to optics's HTTP port.
```
docker container run                            \
    --name my_optics --ipc=host -p 3002:3002 -d \
    optics:0.1.13                               \
    --dump-stdout --dump-carbon=localhost:2003 --http-port=3002
```

### Usage

To log metrics refer to this [example](test/example.c) which describes the
basics of creating and logging metrics.

Polling metrics is accomplished by running the `opticsd` daemon which will
automatically pick up any newly created optics instances and start dumping them
to its configured backends. The following is a simple example for running the
daemon:

```
	opticsd --freq=10 --http-port=3002 --dump-prometheus --dump-carbon=127.0.0.1:2003
```

Use the `--help` argument for more options.

The daemon also has a simple HTTP interface to dump the current set of available
values and can be queried like so:

```
curl -s localhost:3002/metrics/json
```

### Pre-emptive Nit-picking

* Assumes AMD64
* Not 100% POSIX-compliant
* Only Linux is supported
