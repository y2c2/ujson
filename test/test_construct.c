#include "test_construct.h"
#include "ujson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_one_construct(ujson_t* json, char* expect_s)
{
    int ret = 0;
    size_t expect_s_len = strlen(expect_s);
    char* json_str = NULL;
    ujson_size_t json_str_len;

    if (ujson_stringify(&json_str, &json_str_len, json) != 0)
    {
        ret = -1;
        goto fail;
    }

    if (json_str_len != expect_s_len)
    {
        ret = -1;
        goto fail;
    }

    if (strncmp(expect_s, json_str, json_str_len) != 0)
    {
        ret = -1;
        goto fail;
    }

fail:
    if (json_str != NULL)
        free(json_str);
    return ret;
}

#define TEST_ONE_CONSTRUCT(json, expect_s)                                     \
    do                                                                         \
    {                                                                          \
        total++;                                                               \
        if (test_one_construct(json, expect_s) != 0)                           \
        {                                                                      \
            fprintf(stderr, "%s:%d: assert: %s construct test failed\n",       \
                    __FILE__, __LINE__, expect_s);                             \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            passed++;                                                          \
        }                                                                      \
    } while (0);

int test_construct(void)
{
    int total = 0;
    int passed = 0;

    /* undefined */
    {
        ujson_t* u;
        u = ujson_new_undefined();
        TEST_ONE_CONSTRUCT(u, "undefined");
        ujson_destroy(u);
    }

    /* null */
    {
        ujson_t* u;
        u = ujson_new_null();
        TEST_ONE_CONSTRUCT(u, "null");
        ujson_destroy(u);
    }

    /* false */
    {
        ujson_t* u;
        u = ujson_new_bool(ujson_false);
        TEST_ONE_CONSTRUCT(u, "false");
        ujson_destroy(u);
    }

    /* true */
    {
        ujson_t* u;
        u = ujson_new_bool(ujson_true);
        TEST_ONE_CONSTRUCT(u, "true");
        ujson_destroy(u);
    }

    /* number 1 */
    {
        ujson_t* u;
        u = ujson_new_number(1, 1.0);
        TEST_ONE_CONSTRUCT(u, "1");
        ujson_destroy(u);
    }

    /* number 123 */
    {
        ujson_t* u;
        u = ujson_new_number(123, 123.0);
        TEST_ONE_CONSTRUCT(u, "123");
        ujson_destroy(u);
    }

    /* number -123 */
    {
        ujson_t* u;
        u = ujson_new_number(-123, -123.0);
        TEST_ONE_CONSTRUCT(u, "-123");
        ujson_destroy(u);
    }

    /* "" */
    {
        ujson_t* u;
        u = ujson_new_string("", 0);
        TEST_ONE_CONSTRUCT(u, "\"\"");
        ujson_destroy(u);
    }

    /* "a" */
    {
        ujson_t* u;
        u = ujson_new_string("a", 1);
        TEST_ONE_CONSTRUCT(u, "\"a\"");
        ujson_destroy(u);
    }

    /* "abc" */
    {
        ujson_t* u;
        u = ujson_new_string("abc", 3);
        TEST_ONE_CONSTRUCT(u, "\"abc\"");
        ujson_destroy(u);
    }

    /* [] */
    {
        ujson_t* u;
        u = ujson_new_array();
        TEST_ONE_CONSTRUCT(u, "[]");
        ujson_destroy(u);
    }

    /* [1] */
    {
        ujson_t* u;
        u = ujson_new_array();
        ujson_array_push_back(u, ujson_array_item_new(ujson_new_integer(1)));
        TEST_ONE_CONSTRUCT(u, "[1]");
        ujson_destroy(u);
    }

    /* [1,2,3] */
    {
        ujson_t* u;
        u = ujson_new_array();
        ujson_array_push_back(u, ujson_array_item_new(ujson_new_integer(1)));
        ujson_array_push_back(u, ujson_array_item_new(ujson_new_integer(2)));
        ujson_array_push_back(u, ujson_array_item_new(ujson_new_integer(3)));
        TEST_ONE_CONSTRUCT(u, "[1,2,3]");
        ujson_destroy(u);
    }

    /* {} */
    {
        ujson_t* u;
        u = ujson_new_object();
        TEST_ONE_CONSTRUCT(u, "{}");
        ujson_destroy(u);
    }

    /* {"one":1} */
    {
        ujson_t* u;
        u = ujson_new_object();
        ujson_object_push_back(u,
                               ujson_object_item_new(ujson_new_string("one", 3),
                                                     ujson_new_integer(1)));
        TEST_ONE_CONSTRUCT(u, "{\"one\":1}");
        ujson_destroy(u);
    }

    /* {"one":1,"two":2,"three":3} */
    {
        ujson_t* u;
        u = ujson_new_object();
        ujson_object_push_back(u,
                               ujson_object_item_new(ujson_new_string("one", 3),
                                                     ujson_new_integer(1)));
        ujson_object_push_back(u,
                               ujson_object_item_new(ujson_new_string("two", 3),
                                                     ujson_new_integer(2)));
        ujson_object_push_back(
            u, ujson_object_item_new(ujson_new_string("three", 5),
                                     ujson_new_integer(3)));
        TEST_ONE_CONSTRUCT(u, "{\"one\":1,\"two\":2,\"three\":3}");
        ujson_destroy(u);
    }

    printf("%d of %d cases passed\n", passed, total);

    return 0;
}
