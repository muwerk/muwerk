# Test ustd and muwerk scheduler and message queue on Linux or Mac

## Build

Dependency: CMAKE

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

then start test with:

```bash
./muwerk-test
```

Memory-leak checks:

```
valgrind --leak-check=full ./muwerk-test 
```
