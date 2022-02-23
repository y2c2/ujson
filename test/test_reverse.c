#include "test_reverse.h"
#include "ujson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_one_reverse(char* s, char* expect_s)
{
    int ret = 0;
    size_t len = strlen(s);
    size_t expect_s_len = strlen(expect_s);
    ujson_t* json = NULL;
    char* json_str = NULL;
    ujson_size_t json_str_len;

    if ((json = ujson_parse(s, len)) == NULL)
    {
        return -1;
    }

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
    if (json != NULL)
        ujson_destroy(json);
    if (json_str != NULL)
        free(json_str);
    return ret;
}

#define TEST_ONE_REVERSE(s, expect_s)                                          \
    do                                                                         \
    {                                                                          \
        total++;                                                               \
        if (test_one_reverse(s, expect_s) != 0)                                \
        {                                                                      \
            fprintf(stderr, "%s:%d: assert: %s reverse test failed\n",         \
                    __FILE__, __LINE__, s);                                    \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            passed++;                                                          \
        }                                                                      \
    } while (0);

int test_reverse(void)
{
    int total = 0;
    int passed = 0;

    /* Number */
    TEST_ONE_REVERSE("123", "123");
    TEST_ONE_REVERSE(" 123", "123");
    TEST_ONE_REVERSE("123 ", "123");
    TEST_ONE_REVERSE(" 123 ", "123");
    TEST_ONE_REVERSE("0", "0");
    TEST_ONE_REVERSE(" 0", "0");
    TEST_ONE_REVERSE("0 ", "0");
    TEST_ONE_REVERSE(" 0 ", "0");
    TEST_ONE_REVERSE("-123", "-123");
    TEST_ONE_REVERSE(" -123", "-123");
    TEST_ONE_REVERSE("-123 ", "-123");
    TEST_ONE_REVERSE(" -123 ", "-123");

    /* Null */
    TEST_ONE_REVERSE("null", "null");
    TEST_ONE_REVERSE(" null", "null");
    TEST_ONE_REVERSE("null ", "null");
    TEST_ONE_REVERSE(" null ", "null");

    /* Boolean */
    TEST_ONE_REVERSE("true", "true");
    TEST_ONE_REVERSE(" true", "true");
    TEST_ONE_REVERSE("true ", "true");
    TEST_ONE_REVERSE(" true ", "true");
    TEST_ONE_REVERSE("false", "false");
    TEST_ONE_REVERSE(" false", "false");
    TEST_ONE_REVERSE("false ", "false");
    TEST_ONE_REVERSE(" false ", "false");

    /* String */
    TEST_ONE_REVERSE("\"\"", "\"\"");
    TEST_ONE_REVERSE(" \"\"", "\"\"");
    TEST_ONE_REVERSE("\"\" ", "\"\"");
    TEST_ONE_REVERSE(" \"\" ", "\"\"");
    TEST_ONE_REVERSE("\"a\"", "\"a\"");
    TEST_ONE_REVERSE("\"ab\"", "\"ab\"");
    TEST_ONE_REVERSE("\"\\\"\"", "\"\\\"\"");
    TEST_ONE_REVERSE("\"\\\\\"", "\"\\\\\"");
    TEST_ONE_REVERSE("\"\\/\"", "\"\\/\"");
    TEST_ONE_REVERSE("\"\\b\"", "\"\\b\"");
    TEST_ONE_REVERSE("\"\\f\"", "\"\\f\"");
    TEST_ONE_REVERSE("\"\\n\"", "\"\\n\"");
    TEST_ONE_REVERSE("\"\\r\"", "\"\\r\"");
    TEST_ONE_REVERSE("\"\\t\"", "\"\\t\"");
    TEST_ONE_REVERSE("\"知道\"", "\"知道\"");

    /* Array */
    TEST_ONE_REVERSE("[]", "[]");
    TEST_ONE_REVERSE(" []", "[]");
    TEST_ONE_REVERSE("[] ", "[]");
    TEST_ONE_REVERSE(" [] ", "[]");
    TEST_ONE_REVERSE(" [ ] ", "[]");
    TEST_ONE_REVERSE(" [  ] ", "[]");
    TEST_ONE_REVERSE("[1]", "[1]");
    TEST_ONE_REVERSE("[ 1]", "[1]");
    TEST_ONE_REVERSE("[1 ]", "[1]");
    TEST_ONE_REVERSE("[ 1 ]", "[1]");
    TEST_ONE_REVERSE("[1,2]", "[1,2]");
    TEST_ONE_REVERSE("[1 ,2]", "[1,2]");
    TEST_ONE_REVERSE("[1, 2]", "[1,2]");
    TEST_ONE_REVERSE("[1 , 2]", "[1,2]");
    TEST_ONE_REVERSE("[1,2,3]", "[1,2,3]");
    TEST_ONE_REVERSE(" [ 1 , 2 , 3 ] ", "[1,2,3]");

    /* Object */
    TEST_ONE_REVERSE("{}", "{}");
    TEST_ONE_REVERSE(" {}", "{}");
    TEST_ONE_REVERSE("{} ", "{}");
    TEST_ONE_REVERSE(" {} ", "{}");
    TEST_ONE_REVERSE(" { } ", "{}");
    TEST_ONE_REVERSE("{\"one\":1}", "{\"one\":1}");
    TEST_ONE_REVERSE(" { \"one\" : 1 } ", "{\"one\":1}");
    TEST_ONE_REVERSE("{\"one\":1,\"two\":2}", "{\"one\":1,\"two\":2}");
    TEST_ONE_REVERSE(" { \"one\" : 1 , \"two\" : 2 } ",
                     "{\"one\":1,\"two\":2}");
    TEST_ONE_REVERSE("{\"one\":1,\"two\":2,\"three\":3}",
                     "{\"one\":1,\"two\":2,\"three\":3}");
    TEST_ONE_REVERSE(" { \"one\" : 1 , \"two\" : 2 , \"three\" : 3 } ",
                     "{\"one\":1,\"two\":2,\"three\":3}");

    printf("%d of %d cases passed\n", passed, total);

    return 0;
}
