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

```
$ mkdir build
$ cd build
$ cmake .. -G Ninja
$ ninja -C build
```

## Running Tests

```
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
