Ptech
=====

Short for Preprocessor Technology is a messy macro language that is made to produce
C code.

The `ptech.h` header also includes a simple build system that just calls `cc`.

## Usage

### Bootstrap

Once a build.c file that uses `ptech.h` is written, run:

```
cc -o build build.c
```

### Build

Then onwards the project can be build with:

```
./build
```

If build.c is changed:

```
./build
./build
```

## Example

[build.c](./build.c) builds [simple.txt](./simple.txt)

```c
#define PTECH_IMPLEMENTATION
#include "ptech.h"

int main(int argc, char *argv[]) {
    int builder = 0;
    int force = 0;

    if (argc > 1)
        if (strcmp(argv[1], "builder") == 0)
            builder = 1;
	else
            force = 1;

    /* build the builder (this program) */
    if( build(__FILE__, argv[0], builder) != SKIPPED ) return 1;

    if( preprocess("simple.txt", "simple.c", force) == FAILED ) return 1;
    if( build("simple.c", "simple", force) == FAILED ) return 1;
}

```

```console
$ cc -o build build.c
$ ./build
CC ./build <- build.c  skip.
PP simple.c <- simple.txt  ok.
CC simple <- simple.c  ok.
$ ./simple | xxd
JSR 10
00000000: 030a                                     ..
```

## Provides

```c
enum pt_return {
    FAILED = -1,
    SKIPPED = 0,
    SUCCESS = 1
};

enum pt_return
build(const char* input, const char* output, int force);

enum pt_return
preprocess(const char* input, const char* output, int force);
```

Possible values of `force`:
-  0 : Only do action if the input is newer than output.
- !0 : Do the action.

## Notes

Using it is a pain in the rear.
