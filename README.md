# Workshop

## Dependencies

- Linux (`ssh -A lga-test-1.sys.adgear.com`)
- cmake
- ninja
- gdb
- valgrind
- libcmocka
- libbsd


## Build

```bash
$ git clone -b workshop --depth 1 git@github.com:RAttab/optics
$ cd optics
$
$ mkdir build
$ (cd build && cmake .. -G Ninja)
$
$ ninja -C build
```


## Running Tests

```bash
$ ninja -C build test
$
$ cd build && ctest -V .. -L test
$ cd build && ctest -V .. -R key_test
$ gdb ./build/bin/key_test
$
$ cd build && ctest -V .. -L valgrind
$ cd build && ctest -V .. -R key_valgrind
$ valgrind ./build/bin/key_test
```


## Fix Me!

- `lens_dist_test` (gdb)
- `key_test` (valgrind)
- `region_test` (valgrind)


## GDB Cheat Sheet

http://darkdust.net/files/GDB%20Cheat%20Sheet.pdf


## Running valgrind

```bash
$ valgrind <command>
$ valgrind --leak-check=full --track-origins=yes <commnad>

$ valgrind --gen-suppressions=all
$ valgrind --suppressions=<file>
```
