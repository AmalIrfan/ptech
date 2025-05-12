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

    if( preprocess("simple.t", "simple.c", force) == FAILED ) return 1;
    if( build("simple.c", "simple", force) == FAILED ) return 1;
}

