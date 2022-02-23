/* uJSON
 * Copyright(c) 2016-2020 y2c2 */

#ifndef UJSON_H
#define UJSON_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        ujson_false = 0,
        ujson_true = 1,
    } ujson_bool;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#include <stdio.h>
    typedef size_t ujson_size_t;
#else
typedef unsigned int ujson_size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

    typedef enum
    {
        UJSON_NULL,
        UJSON_UNDEFINED,
        UJSON_BOOL,
        UJSON_NUMEBR,
        UJSON_STRING,
        UJSON_ARRAY,
        UJSON_OBJECT,
    } ujson_type_t;

    struct ujson_array_item;
    typedef struct ujson_array_item ujson_array_item_t;
    struct ujson_array;
    typedef struct ujson_array ujson_array_t;
    struct ujson_object_item;
    typedef struct ujson_object_item ujson_object_item_t;
    struct ujson_object;
    typedef struct ujson_object ujson_object_t;
    struct ujson;
    typedef struct ujson ujson_t;

    /* Allocator */
    typedef void* (*ujson_malloc_cb_t)(ujson_size_t size);
    typedef void (*ujson_free_cb_t)(void* ptr);

    void ujson_allocator_set_malloc(ujson_malloc_cb_t cb);
    void ujson_allocator_set_free(ujson_free_cb_t cb);

    /* Create data structure */

    ujson_t* ujson_new_integer(int value);
    ujson_t* ujson_new_number(int value, double value_double);
    ujson_t* ujson_new_bool(ujson_bool value);
    ujson_t* ujson_new_null(void);
    ujson_t* ujson_new_undefined(void);
    ujson_t* ujson_new_string(char* s, ujson_size_t len);

    ujson_array_item_t* ujson_array_item_new(ujson_t* element);
    void ujson_array_item_destroy(ujson_array_item_t* item);
    ujson_t* ujson_new_array(void);
    int ujson_array_push_back(ujson_t* array, ujson_array_item_t* new_item);

    ujson_object_item_t* ujson_object_item_new(ujson_t* key, ujson_t* value);
    void ujson_object_item_destroy(ujson_object_item_t* item);
    ujson_t* ujson_new_object(void);
    int ujson_object_push_back(ujson_t* object, ujson_object_item_t* new_item);

    /* Inspector */

    ujson_type_t ujson_type(ujson_t* ujson);

    int ujson_as_integer_value(ujson_t* ujson);
    double ujson_as_double_value(ujson_t* ujson);

    ujson_bool ujson_as_bool_value(ujson_t* ujson);

    char* ujson_as_string_body(ujson_t* ujson);
    ujson_size_t ujson_as_string_size_in_character(ujson_t* ujson);
    ujson_size_t ujson_as_string_size_in_utf8_bytes(ujson_t* ujson);

    ujson_size_t ujson_as_array_size(ujson_t* ujson);
    ujson_array_item_t* ujson_as_array_first(ujson_t* ujson);
    ujson_array_item_t* ujson_as_array_next(ujson_array_item_t* item);
    ujson_t* ujson_as_array_item_value(ujson_array_item_t* item);

    ujson_object_item_t* ujson_as_object_first(ujson_t* ujson);
    ujson_object_item_t* ujson_as_object_next(ujson_object_item_t* item);
    char* ujson_as_object_item_key_body(ujson_object_item_t* item);
    ujson_size_t ujson_as_object_item_key_length(ujson_object_item_t* item);
    ujson_t* ujson_as_object_item_value(ujson_object_item_t* item);
    ujson_t* ujson_as_object_lookup(ujson_t* object, char* name,
                                    ujson_size_t len);

    /* Parse a JSON string and generate a JSON value */

    ujson_t* ujson_parse(char* s, ujson_size_t len);

    /* Configure */

    typedef enum
    {
        UJSON_STRINGIFY_CONFIG_STYLE_COMPACT = 0,
        UJSON_STRINGIFY_CONFIG_STYLE_INDENT,
    } ujson_stringify_config_style_t;

    typedef struct
    {
        ujson_stringify_config_style_t style;
        int repeat;
        char replacer;
    } ujson_stringify_config_t;

    /* Dump a JSON value and product a json string (with config) */
    int ujson_stringify_ex(char** json_str, ujson_size_t* json_str_len,
                           const ujson_t* ujson,
                           ujson_stringify_config_t* config);

    /* Dump a JSON value and product a json string */
    int ujson_stringify(char** json_str, ujson_size_t* json_str_len,
                        const ujson_t* ujson);

    /* Destroy JSON value */
    void ujson_destroy(ujson_t* ujson);

#ifdef __cplusplus
}
#endif

#endif
