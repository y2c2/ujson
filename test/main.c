#include "test_construct.h"
#include "test_reverse.h"
#include "ujson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    ujson_allocator_set_malloc(malloc);
    ujson_allocator_set_free(free);
    test_reverse();
    test_construct();
    return 0;
}
