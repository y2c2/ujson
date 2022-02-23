/* uJSON
 * Copyright(c) 2016-2020 y2c2 */

#include "ujson.h"
#include <stdio.h>

/* Constants */
#define UJSON_MBUF_DEFAULT_INIT_SIZE 512
#define UJSON_MBUF_DEFAULT_INC_SIZE 512

struct ujson_array_item
{
    struct ujson* value;

    struct ujson_array_item *prev, *next;
};

struct ujson_array
{
    ujson_array_item_t *begin, *end;

    ujson_size_t size;
};

struct ujson_object_item
{
    struct
    {
        char* s;
        ujson_size_t len;
        unsigned char checksum;
    } key;
    struct ujson* value;

    struct ujson_object_item *prev, *next;
};

struct ujson_object
{
    ujson_object_item_t *begin, *end;
};

struct ujson
{
    ujson_type_t type;
    union
    {
        ujson_bool part_bool;
        struct
        {
            double as_double;
            int as_int;
            ujson_bool is_double;
        } part_number;
        struct
        {
            char* s;
            ujson_size_t len;
            ujson_size_t ch_len;
        } part_string;
        ujson_array_t part_array;
        ujson_object_t part_object;
    } u;
};

/* Mutable Buffer */
struct ujson_mbuf
{
    char* body;
    ujson_size_t size;
    ujson_size_t capacity;
};
typedef struct ujson_mbuf ujson_mbuf_t;

/* Global Staff */
static ujson_malloc_cb_t g_ujson_malloc = NULL;
static ujson_free_cb_t g_ujson_free = NULL;
static ujson_t* ujson_parse_in(char** p, ujson_size_t* len);

/* Declarations */
static int ujson_stringify_value(ujson_mbuf_t* mbuf, const ujson_t* ujson);
static void ujson_destroy_value(ujson_t* ujson);

/* Allocators */
void ujson_allocator_set_malloc(ujson_malloc_cb_t cb) { g_ujson_malloc = cb; }

void ujson_allocator_set_free(ujson_free_cb_t cb) { g_ujson_free = cb; }

static void* ujson_malloc(ujson_size_t size) { return g_ujson_malloc(size); }

static void ujson_free(void* ptr) { g_ujson_free(ptr); }

/* Mutable Buffer */

static unsigned char ujson_checksum(char* buf, ujson_size_t len)
{
    unsigned char checksum = 0;

    while (len-- != 0)
    {
        checksum += *buf++;
    }

    return checksum;
}

static void* ujson_memcpy(void* dest, const void* src, ujson_size_t n)
{
    char* dest_p = dest;
    const char* src_p = src;
    while (n-- != 0)
    {
        *dest_p++ = *src_p++;
    }
    return dest;
}

static int ujson_strncmp(const char* s1, const char* s2, ujson_size_t n)
{
    const char *p1 = s1, *p2 = s2;
    if (n != 0)
    {
        do
        {
            if (*p1 != *p2++)
                break;
            if (*p1++ == '\0')
                return 0;
        } while (--n != 0);
        if (n > 0)
        {
            if (*p1 == '\0')
                return -1;
            if (*--p2 == '\0')
                return 1;
            return (unsigned char)*p1 - (unsigned char)*p2;
        }
    }
    return 0;
}

static int ujson_mbuf_init(ujson_mbuf_t* mbuf)
{
    mbuf->size = 0;
    mbuf->capacity = UJSON_MBUF_DEFAULT_INIT_SIZE;
    if ((mbuf->body = (char*)g_ujson_malloc(
             sizeof(char) * UJSON_MBUF_DEFAULT_INIT_SIZE)) == ((void*)0))
    {
        return -1;
    }
    return 0;
}

static void ujson_mbuf_uninit(ujson_mbuf_t* mbuf)
{
    if (mbuf->body != (void*)0)
    {
        g_ujson_free(mbuf->body);
        mbuf->body = (void*)0;
    }
}

static int ujson_mbuf_append(ujson_mbuf_t* mbuf, const char* s,
                             const ujson_size_t len)
{
    char* new_buf = (void*)0;
    ujson_size_t new_capacity;
    if (mbuf->size + len + 1 >= mbuf->capacity)
    {
        /* Extend */
        new_capacity = mbuf->size + len + 1 + UJSON_MBUF_DEFAULT_INC_SIZE;
        new_buf = (char*)g_ujson_malloc(sizeof(char) * new_capacity);
        if (new_buf == (void*)0)
            return -1;
        ujson_memcpy(new_buf, mbuf->body, mbuf->size);
        ujson_memcpy(new_buf + mbuf->size, s, len);
        mbuf->size = mbuf->size + len;
        new_buf[mbuf->size] = '\0';
        mbuf->capacity = new_capacity;
        g_ujson_free(mbuf->body);
        mbuf->body = new_buf;
    }
    else
    {
        ujson_memcpy(mbuf->body + mbuf->size, s, len);
        mbuf->size += len;
        mbuf->body[mbuf->size] = '\0';
    }
    return 0;
}

static char* ujson_mbuf_body(ujson_mbuf_t* mbuf) { return mbuf->body; }

static ujson_size_t ujson_mbuf_size(ujson_mbuf_t* mbuf) { return mbuf->size; }

static int ujson_mbuf_extract(char** json_str, ujson_size_t* json_str_len,
                              ujson_mbuf_t* mbuf)
{
    char* new_str = NULL;
    ujson_size_t len = ujson_mbuf_size(mbuf);

    if ((new_str = ujson_malloc(len + 1)) == NULL)
    {
        return -1;
    }
    ujson_memcpy(new_str, ujson_mbuf_body(mbuf), len);
    new_str[len] = '\0';
    *json_str = new_str;
    *json_str_len = len;
    return 0;
}

static ujson_t* ujson_new(ujson_type_t type)
{
    ujson_t* new_json = ujson_malloc(sizeof(ujson_t));
    if (new_json == NULL)
        return NULL;
    new_json->type = type;
    switch (type)
    {
    case UJSON_BOOL:
    case UJSON_NULL:
    case UJSON_UNDEFINED:
    case UJSON_NUMEBR:
        break;
    case UJSON_STRING:
        new_json->u.part_string.s = NULL;
        break;
    case UJSON_ARRAY:
        new_json->u.part_array.begin = NULL;
        new_json->u.part_array.end = NULL;
        new_json->u.part_array.size = 0;
        break;
    case UJSON_OBJECT:
        new_json->u.part_object.begin = NULL;
        new_json->u.part_object.end = NULL;
        break;
    }
    return new_json;
}

ujson_t* ujson_new_integer(int value)
{
    ujson_t* new_ujson;
    new_ujson = ujson_new(UJSON_NUMEBR);
    new_ujson->u.part_number.is_double = ujson_false;
    new_ujson->u.part_number.as_int = value;
    new_ujson->u.part_number.as_double = (double)value;
    return new_ujson;
}

ujson_t* ujson_new_number(int value, double value_double)
{
    ujson_t* new_ujson;
    new_ujson = ujson_new(UJSON_NUMEBR);
    new_ujson->u.part_number.is_double = ujson_false;
    new_ujson->u.part_number.as_int = value;
    new_ujson->u.part_number.as_double = value_double;
    return new_ujson;
}

static ujson_t* ujson_new_string2(char* s, ujson_size_t len,
                                  ujson_size_t ch_len)
{
    ujson_t* new_ujson;
    new_ujson = ujson_new(UJSON_STRING);
    new_ujson->u.part_string.len = len;
    new_ujson->u.part_string.ch_len = ch_len;
    if (len == 0)
    {
        new_ujson->u.part_string.s = NULL;
    }
    else
    {
        if ((new_ujson->u.part_string.s =
                 (char*)ujson_malloc(sizeof(char) * (len + 1))) == NULL)
        {
            ujson_destroy(new_ujson);
            return NULL;
        }
        ujson_memcpy(new_ujson->u.part_string.s, s, len);
        new_ujson->u.part_string.s[len] = '\0';
    }
    return new_ujson;
}

typedef enum
{
    UJSON_PARSE_IN_STRING_STATE_INIT = 0,
    UJSON_PARSE_IN_STRING_STATE_ESCAPE,
} ujson_parse_in_string_state_t;

#ifndef ISDIGIT
#define ISDIGIT(ch) (('0' <= (ch)) && ((ch) <= '9'))
#endif

#ifndef ISHEXDIGIT
#define ISHEXDIGIT(ch)                                                         \
    (ISDIGIT(ch) || (('a' <= (ch)) && ((ch) <= 'f')) ||                        \
     (('A' <= (ch)) && ((ch) <= 'F')))
#endif

#ifndef ISHEXDIGIT_S4
#define ISHEXDIGIT_S4(p)                                                       \
    ((ISHEXDIGIT(*(p + 0))) && (ISHEXDIGIT(*(p + 1))) &&                       \
     (ISHEXDIGIT(*(p + 2))) && (ISHEXDIGIT(*(p + 3))))
#endif

#ifndef ISALPHA
#define ISALPHA(ch)                                                            \
    ((('a' <= (ch)) && ((ch) <= 'z')) || (('A' <= (ch)) && ((ch) <= 'Z')))
#endif

#ifndef ISID
#define ISID(ch) (ISALPHA(ch) || ISDIGIT(ch))
#endif

#ifndef ISWS
#define ISWS(ch)                                                               \
    (((ch) == '\t') || ((ch) == '\r') || ((ch) == '\n') || ((ch) == ' '))
#endif

#define IS_HYPER_ID(ch) (((ch)&128) != 0 ? 1 : 0)

static int ujson_parse_in_string_hexchar_to_num(char ch)
{
    int result;
    if (('0' <= ch) && (ch <= '9'))
    {
        result = (int)ch - (int)'0';
    }
    else if (('a' <= ch) && (ch <= 'f'))
    {
        result = (int)ch - (int)'a';
    }
    else if (('A' <= ch) && (ch <= 'F'))
    {
        result = (int)ch - (int)'A';
    }
    else
        result = -1;
    return result;
}

static int ujson_parse_in_string_hexchar_to_num_s4(char* p)
{
    unsigned int result = 0;
    int t;
    if ((t = ujson_parse_in_string_hexchar_to_num(*p)) == -1)
    {
        return -1;
    }
    result = (unsigned int)t;
    if ((t = ujson_parse_in_string_hexchar_to_num(*(p + 1))) == -1)
    {
        return -1;
    }
    result = (result << 4) | (unsigned int)t;
    if ((t = ujson_parse_in_string_hexchar_to_num(*(p + 2))) == -1)
    {
        return -1;
    }
    result = (result << 4) | (unsigned int)t;
    if ((t = ujson_parse_in_string_hexchar_to_num(*(p + 3))) == -1)
    {
        return -1;
    }
    result = (result << 4) | (unsigned int)t;
    return (int)result;
}

static ujson_size_t id_hyper_length(char ch)
{
    ujson_size_t bytes_number;
    unsigned char uch = (unsigned char)ch;
    /* 0xxxxxxx */
    if ((uch & 0x80) == 0)
        bytes_number = 1;
    /* 110xxxxx, 10xxxxxx */
    else if ((uch & 0xe0) == 0xc0)
        bytes_number = 2;
    /* 1110xxxx, 10xxxxxx, 10xxxxxx */
    else if ((uch & 0xf0) == 0xe0)
        bytes_number = 3;
    /* 11110xxx, 10xxxxxx, 10xxxxxx, 10xxxxxx */
    else if ((uch & 0xf8) == 0xf0)
        bytes_number = 4;
    /* 111110xx, 10xxxxxx, 10xxxxxx, 10xxxxxx, 10xxxxxx */
    else if ((uch & 0xfc) == 0xf8)
        bytes_number = 5;
    /* 1111110x, 10xxxxxx, 10xxxxxx, 10xxxxxx, 10xxxxxx, 10xxxxxx */
    else if ((uch & 0xfe) == 0xfc)
        bytes_number = 6;
    else
        bytes_number = 0;
    return bytes_number;
}

ujson_t* ujson_new_string(char* s, ujson_size_t len)
{
    char* p = s;
    ujson_t* new_str = NULL;
    ujson_mbuf_t buffer;
    ujson_parse_in_string_state_t state = UJSON_PARSE_IN_STRING_STATE_INIT;
    ujson_size_t bytes_number;
    ujson_size_t ch_len = 0;
    int value_u;
    char writebuf[7];
    /* Initialize buffer */
    ujson_mbuf_init(&buffer);
    while (len > 0)
    {
        switch (state)
        {
        case UJSON_PARSE_IN_STRING_STATE_INIT:
            if (*p == '"')
            {
                goto finish;
            }
            else if (*p == '\\')
            {
                if ((len < 2))
                {
                    goto fail;
                }
                state = UJSON_PARSE_IN_STRING_STATE_ESCAPE;
                p++;
                len--;
            }
            else if (IS_HYPER_ID(*p))
            {
                bytes_number = id_hyper_length(*p);
                if (len < bytes_number)
                {
                    goto fail;
                }
                if (ujson_mbuf_append(&buffer, p, bytes_number) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p += bytes_number;
                len -= bytes_number;
            }
            else
            {
                if (ujson_mbuf_append(&buffer, p, 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            break;
        case UJSON_PARSE_IN_STRING_STATE_ESCAPE:
            if (*p == '"')
            {
                if (ujson_mbuf_append(&buffer, "\"", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == '\\')
            {
                if (ujson_mbuf_append(&buffer, "\\", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == '/')
            {
                if (ujson_mbuf_append(&buffer, "/", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'b')
            {
                if (ujson_mbuf_append(&buffer, "\b", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'f')
            {
                if (ujson_mbuf_append(&buffer, "\f", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'n')
            {
                if (ujson_mbuf_append(&buffer, "\n", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'r')
            {
                if (ujson_mbuf_append(&buffer, "\r", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 't')
            {
                if (ujson_mbuf_append(&buffer, "\t", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if ((*p == 'u') && (len >= 5) && ISHEXDIGIT_S4(p + 1))
            {
                value_u = ujson_parse_in_string_hexchar_to_num_s4(p + 1);
                if (value_u == -1)
                    goto fail;
                if ((0 <= value_u) && (value_u <= 0x7F))
                {
                    writebuf[0] = ((char)value_u);
                    if (ujson_mbuf_append(&buffer, writebuf, 1) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else if ((0x80 <= value_u) && (value_u <= 0x7FF))
                {
                    writebuf[0] = (char)(0xc0 | (((unsigned int)value_u) >> 6));
                    writebuf[1] =
                        (char)(0x80 | (((unsigned int)value_u) & 0x3f));
                    if (ujson_mbuf_append(&buffer, writebuf, 2) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else if ((0x800 <= value_u) && (value_u <= 0x7FFF))
                {
                    writebuf[0] =
                        (char)(0xe0 | (((unsigned int)value_u) >> 12));
                    writebuf[1] =
                        (char)(0x80 | (((unsigned int)value_u >> 6) & 0x3f));
                    writebuf[2] =
                        (char)(0x80 | (((unsigned int)value_u) & 0x3f));
                    if (ujson_mbuf_append(&buffer, writebuf, 3) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else
                {
                    goto fail;
                }
            }
            else
            {
                goto fail;
            }
            /* Reset state */
            state = UJSON_PARSE_IN_STRING_STATE_INIT;
            break;
        }
    }
finish:
    new_str = ujson_new_string2(ujson_mbuf_body(&buffer),
                                ujson_mbuf_size(&buffer), ch_len);
    ujson_mbuf_uninit(&buffer);
    return new_str;
fail:
    ujson_mbuf_uninit(&buffer);
    return NULL;
}

ujson_t* ujson_new_bool(ujson_bool value)
{
    ujson_t* new_ujson;

    new_ujson = ujson_new(UJSON_BOOL);
    new_ujson->u.part_bool = value;

    return new_ujson;
}

ujson_t* ujson_new_null(void) { return ujson_new(UJSON_NULL); }

ujson_t* ujson_new_undefined(void) { return ujson_new(UJSON_UNDEFINED); }

ujson_array_item_t* ujson_array_item_new(ujson_t* element)
{
    ujson_array_item_t* new_item = ujson_malloc(sizeof(ujson_array_item_t));
    if (new_item == NULL)
        return NULL;
    new_item->next = new_item->prev = NULL;
    new_item->value = element;
    return new_item;
}

void ujson_array_item_destroy(ujson_array_item_t* item)
{
    if (item->value != NULL)
    {
        ujson_destroy_value(item->value);
    }
    ujson_free(item);
}

ujson_t* ujson_new_array(void)
{
    ujson_t* new_ujson;

    new_ujson = ujson_new(UJSON_ARRAY);
    new_ujson->u.part_array.begin = NULL;
    new_ujson->u.part_array.end = NULL;
    new_ujson->u.part_array.size = 0;

    return new_ujson;
}

int ujson_array_push_back(ujson_t* array, ujson_array_item_t* new_item)
{
    if (array->u.part_array.begin == NULL)
    {
        array->u.part_array.begin = new_item;
        array->u.part_array.end = new_item;
    }
    else
    {
        array->u.part_array.end->next = new_item;
        new_item->prev = array->u.part_array.end;
        array->u.part_array.end = new_item;
    }
    array->u.part_array.size++;
    return 0;
}

ujson_object_item_t* ujson_object_item_new(ujson_t* key, ujson_t* value)
{
    ujson_object_item_t* new_item = ujson_malloc(sizeof(ujson_object_item_t));
    if (new_item == NULL)
        return NULL;
    new_item->next = new_item->prev = NULL;
    /* Key */
    new_item->key.len = key->u.part_string.len;
    if ((new_item->key.s = (char*)ujson_malloc(
             sizeof(char) * (new_item->key.len + 1))) == NULL)
    {
        ujson_free(new_item);
        return NULL;
    }
    ujson_memcpy(new_item->key.s, key->u.part_string.s, new_item->key.len);
    new_item->key.s[new_item->key.len] = '\0';
    ujson_destroy_value(key);
    new_item->key.checksum = ujson_checksum(new_item->key.s, new_item->key.len);
    /* Value */
    new_item->value = value;
    return new_item;
}

void ujson_object_item_destroy(ujson_object_item_t* item)
{
    if (item->key.s != NULL)
    {
        ujson_free(item->key.s);
    }
    if (item->value != NULL)
    {
        ujson_destroy_value(item->value);
    }
    ujson_free(item);
}

ujson_t* ujson_new_object(void)
{
    ujson_t* new_ujson;
    new_ujson = ujson_new(UJSON_OBJECT);
    new_ujson->u.part_object.begin = NULL;
    new_ujson->u.part_object.end = NULL;
    return new_ujson;
}

int ujson_object_push_back(ujson_t* object, ujson_object_item_t* new_item)
{
    if (object->u.part_object.begin == NULL)
    {
        object->u.part_object.begin = new_item;
        object->u.part_object.end = new_item;
    }
    else
    {
        object->u.part_object.end->next = new_item;
        new_item->prev = object->u.part_object.end;
        object->u.part_object.end = new_item;
    }
    return 0;
}

/* Inspector */

ujson_type_t ujson_type(ujson_t* ujson) { return ujson->type; }

int ujson_as_integer_value(ujson_t* ujson)
{
    return ujson->u.part_number.as_int;
}

double ujson_as_double_value(ujson_t* ujson)
{
    return ujson->u.part_number.as_double;
}

ujson_bool ujson_as_bool_value(ujson_t* ujson) { return ujson->u.part_bool; }

char* ujson_as_string_body(ujson_t* ujson) { return ujson->u.part_string.s; }

ujson_size_t ujson_as_string_size_in_character(ujson_t* ujson)
{
    return ujson->u.part_string.ch_len;
}

ujson_size_t ujson_as_string_size_in_utf8_bytes(ujson_t* ujson)
{
    return ujson->u.part_string.len;
}

ujson_size_t ujson_as_array_size(ujson_t* ujson)
{
    return ujson->u.part_array.size;
}

ujson_array_item_t* ujson_as_array_first(ujson_t* ujson)
{
    return ujson->u.part_array.begin;
}

ujson_array_item_t* ujson_as_array_next(ujson_array_item_t* item)
{
    return item->next;
}

ujson_t* ujson_as_array_item_value(ujson_array_item_t* item)
{
    return item->value;
}

ujson_object_item_t* ujson_as_object_first(ujson_t* ujson)
{
    return ujson->u.part_object.begin;
}

ujson_object_item_t* ujson_as_object_next(ujson_object_item_t* item)
{
    return item->next;
}

char* ujson_as_object_item_key_body(ujson_object_item_t* item)
{
    return item->key.s;
}

ujson_size_t ujson_as_object_item_key_length(ujson_object_item_t* item)
{
    return item->key.len;
}

ujson_t* ujson_as_object_item_value(ujson_object_item_t* item)
{
    return item->value;
}

ujson_t* ujson_as_object_lookup(ujson_t* object, char* name, ujson_size_t len)
{
    ujson_object_item_t* item_cur = object->u.part_object.begin;
    unsigned char checksum = ujson_checksum(name, len);
    while (item_cur != NULL)
    {
        if ((item_cur->key.len == len) &&
            (item_cur->key.checksum == checksum) &&
            (ujson_strncmp(item_cur->key.s, name, len) == 0))
        {
            return item_cur->value;
        }
        item_cur = item_cur->next;
    }
    return NULL;
}

static void ujson_skip_whitespace(char** p_io, ujson_size_t* len_io)
{
    while ((*len_io != 0) && (ISWS(**p_io)))
    {
        (*p_io)++;
        (*len_io)--;
    }
}

static ujson_t* ujson_parse_in_number(char** p_io, ujson_size_t* len_io)
{
    char* p = *p_io;
    ujson_size_t len = *len_io;
    int value = 0;
    double value_double = 0.0;
    ujson_bool negative = ujson_false;
    ujson_t* result = NULL;
    double base;
    /* Negative */
    if (*p == '-')
    {
        negative = ujson_true;
        p++;
        len--;
    }
    /* Integer Part */
    if (len == 0)
    {
        return NULL;
    }
    if (*p == '0')
    {
        value = 0;
        p++;
        len--;
    }
    else if (('1' <= *p) && (*p <= '9'))
    {
        while ((len > 0) && (ISDIGIT(*p)))
        {
            value = value * 10 + ((*p) - '0');
            p++;
            len--;
        }
    }
    else
    {
        return NULL;
    }
    /* Fill double part */
    value_double = (double)value;
    /* Fractal Part */
    if ((len > 0) && (*p == '.'))
    {
        /* Skip '.' */
        p++;
        len--;
        base = 0.1;
        while ((len > 0) && (ISDIGIT(*p)))
        {
            value_double += base * ((*p) - '0');
            base /= 10;
            p++;
            len--;
        }
    }
    /* Negative */
    if (negative == ujson_true)
    {
        value = -value;
        value_double = -value_double;
    }
    result = ujson_new_number(value, value_double);
    *p_io = p;
    *len_io = len;
    return result;
}

static ujson_t* ujson_parse_in_string(char** p_io, ujson_size_t* len_io)
{
    char* p = *p_io;
    ujson_size_t len = *len_io;
    ujson_t* result = NULL;
    ujson_mbuf_t buffer;
    ujson_parse_in_string_state_t state = UJSON_PARSE_IN_STRING_STATE_INIT;
    ujson_size_t bytes_number;
    ujson_size_t ch_len = 0;
    int value_u;
    char writebuf[7];
    /* Initialize buffer */
    ujson_mbuf_init(&buffer);
    /* Skip '"' */
    p++;
    len--;
    while (len > 0)
    {
        switch (state)
        {
        case UJSON_PARSE_IN_STRING_STATE_INIT:
            if (*p == '"')
            {
                goto finish;
            }
            else if (*p == '\\')
            {
                if ((len < 2))
                {
                    goto fail;
                }
                state = UJSON_PARSE_IN_STRING_STATE_ESCAPE;
                p++;
                len--;
            }
            else if (IS_HYPER_ID(*p))
            {
                bytes_number = id_hyper_length(*p);
                if (len < bytes_number)
                {
                    goto fail;
                }
                if (ujson_mbuf_append(&buffer, p, bytes_number) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p += bytes_number;
                len -= bytes_number;
            }
            else
            {
                if (ujson_mbuf_append(&buffer, p, 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            break;
        case UJSON_PARSE_IN_STRING_STATE_ESCAPE:
            if (*p == '"')
            {
                if (ujson_mbuf_append(&buffer, "\"", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == '\\')
            {
                if (ujson_mbuf_append(&buffer, "\\", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == '/')
            {
                if (ujson_mbuf_append(&buffer, "/", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'b')
            {
                if (ujson_mbuf_append(&buffer, "\b", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'f')
            {
                if (ujson_mbuf_append(&buffer, "\f", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'n')
            {
                if (ujson_mbuf_append(&buffer, "\n", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 'r')
            {
                if (ujson_mbuf_append(&buffer, "\r", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if (*p == 't')
            {
                if (ujson_mbuf_append(&buffer, "\t", 1) != 0)
                {
                    goto fail;
                }
                ch_len++;
                p++;
                len--;
            }
            else if ((*p == 'u') && (len >= 5) && ISHEXDIGIT_S4(p + 1))
            {
                value_u = ujson_parse_in_string_hexchar_to_num_s4(p + 1);
                if (value_u == -1)
                    goto fail;
                if ((0 <= value_u) && (value_u <= 0x7F))
                {
                    writebuf[0] = ((char)value_u);
                    if (ujson_mbuf_append(&buffer, writebuf, 1) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else if ((0x80 <= value_u) && (value_u <= 0x7FF))
                {
                    writebuf[0] = (char)(0xc0 | (((unsigned int)value_u) >> 6));
                    writebuf[1] =
                        (char)(0x80 | (((unsigned int)value_u) & 0x3f));
                    if (ujson_mbuf_append(&buffer, writebuf, 2) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else if ((0x800 <= value_u) && (value_u <= 0x7FFF))
                {
                    writebuf[0] =
                        (char)(0xe0 | (((unsigned int)value_u) >> 12));
                    writebuf[1] =
                        (char)(0x80 | (((unsigned int)value_u >> 6) & 0x3f));
                    writebuf[2] =
                        (char)(0x80 | (((unsigned int)value_u) & 0x3f));
                    if (ujson_mbuf_append(&buffer, writebuf, 3) != 0)
                    {
                        goto fail;
                    }
                    ch_len++;
                    p += 5;
                    len -= 5;
                }
                else
                {
                    goto fail;
                }
            }
            else
            {
                goto fail;
            }
            /* Reset state */
            state = UJSON_PARSE_IN_STRING_STATE_INIT;
            break;
        }
    }
finish:
    if (len == 0)
    {
        goto fail;
    }
    /* Skip '"' */
    p++;
    len--;
    if ((result = ujson_new_string2(ujson_mbuf_body(&buffer),
                                    ujson_mbuf_size(&buffer), ch_len)) == NULL)
    {
        goto fail;
    }
    *p_io = p;
    *len_io = len;
fail:
    /* Uninitialize buffer */
    ujson_mbuf_uninit(&buffer);
    return result;
}

typedef enum
{
    UJSON_PARSE_IN_ARRAY_STATE_INIT = 0,
    UJSON_PARSE_IN_ARRAY_STATE_VALUE,
    UJSON_PARSE_IN_ARRAY_STATE_COMMA,
    UJSON_PARSE_IN_ARRAY_STATE_FINISH,
} ujson_parse_in_array_state_t;

static ujson_t* ujson_parse_in_array(char** p_io, ujson_size_t* len_io)
{
    char* p = *p_io;
    ujson_size_t len = *len_io;
    ujson_t* new_array = NULL;
    ujson_t* new_element = NULL;
    ujson_array_item_t* new_element_item = NULL;
    ujson_parse_in_array_state_t state = UJSON_PARSE_IN_ARRAY_STATE_INIT;
    if ((new_array = ujson_new_array()) == NULL)
    {
        return NULL;
    }
    /* Skip '[' */
    p++;
    len--;
    while ((len != 0) && (state != UJSON_PARSE_IN_ARRAY_STATE_FINISH))
    {
        switch (state)
        {
        case UJSON_PARSE_IN_ARRAY_STATE_INIT:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if (*p == ']')
            {
                state = UJSON_PARSE_IN_ARRAY_STATE_FINISH;
            }
            else
            {
                if ((new_element = ujson_parse_in(&p, &len)) == NULL)
                {
                    goto fail;
                }
                if ((new_element_item = ujson_array_item_new(new_element)) ==
                    NULL)
                {
                    goto fail;
                }
                new_element = NULL;
                ujson_array_push_back(new_array, new_element_item);
                new_element_item = NULL;
                state = UJSON_PARSE_IN_ARRAY_STATE_VALUE;
            }
            break;
        case UJSON_PARSE_IN_ARRAY_STATE_VALUE:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if (*p == ']')
            {
                state = UJSON_PARSE_IN_ARRAY_STATE_FINISH;
            }
            else if (*p == ',')
            {
                p++;
                len--;
                state = UJSON_PARSE_IN_ARRAY_STATE_COMMA;
            }
            else
            {
                goto fail;
            }
            break;
        case UJSON_PARSE_IN_ARRAY_STATE_COMMA:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if ((new_element = ujson_parse_in(&p, &len)) == NULL)
            {
                goto fail;
            }
            if ((new_element_item = ujson_array_item_new(new_element)) == NULL)
            {
                goto fail;
            }
            new_element = NULL;
            ujson_array_push_back(new_array, new_element_item);
            new_element_item = NULL;
            state = UJSON_PARSE_IN_ARRAY_STATE_VALUE;
            break;
        case UJSON_PARSE_IN_ARRAY_STATE_FINISH:
            break;
        }
    }
    /* Skip ']' */
    p++;
    len--;
    *p_io = p;
    *len_io = len;
    goto done;
fail:
    if (new_array != NULL)
    {
        ujson_destroy_value(new_array);
        new_array = NULL;
    }
    if (new_element != NULL)
    {
        ujson_destroy_value(new_element);
    }
done:
    return new_array;
}

typedef enum
{
    UJSON_PARSE_IN_OBJECT_STATE_INIT = 0,
    UJSON_PARSE_IN_OBJECT_STATE_KEY,
    UJSON_PARSE_IN_OBJECT_STATE_COLON,
    UJSON_PARSE_IN_OBJECT_STATE_VALUE,
    UJSON_PARSE_IN_OBJECT_STATE_COMMA,
    UJSON_PARSE_IN_OBJECT_STATE_FINISH,
} ujson_parse_in_object_state_t;

static ujson_t* ujson_parse_in_object(char** p_io, ujson_size_t* len_io)
{
    char* p = *p_io;
    ujson_size_t len = *len_io;
    ujson_t* new_array = NULL;
    ujson_t* new_key = NULL;
    ujson_t* new_value = NULL;
    ujson_object_item_t* new_element_item = NULL;
    ujson_parse_in_object_state_t state = UJSON_PARSE_IN_OBJECT_STATE_INIT;
    if ((new_array = ujson_new_object()) == NULL)
    {
        return NULL;
    }
    /* Skip '{' */
    p++;
    len--;
    while ((len != 0) && (state != UJSON_PARSE_IN_OBJECT_STATE_FINISH))
    {
        switch (state)
        {
        case UJSON_PARSE_IN_OBJECT_STATE_INIT:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if (*p == '}')
            {
                state = UJSON_PARSE_IN_OBJECT_STATE_FINISH;
            }
            else
            {
                if ((new_key = ujson_parse_in(&p, &len)) == NULL)
                {
                    goto fail;
                }
                if (new_key->type != UJSON_STRING)
                {
                    goto fail;
                }
                state = UJSON_PARSE_IN_OBJECT_STATE_KEY;
            }
            break;
        case UJSON_PARSE_IN_OBJECT_STATE_KEY:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if (*p != ':')
            {
                goto fail;
            }
            p++;
            len--;
            state = UJSON_PARSE_IN_OBJECT_STATE_COLON;
            break;
        case UJSON_PARSE_IN_OBJECT_STATE_COLON:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if ((new_value = ujson_parse_in(&p, &len)) == NULL)
            {
                goto fail;
            }
            if ((new_element_item =
                     ujson_object_item_new(new_key, new_value)) == NULL)
            {
                goto fail;
            }
            new_key = NULL;
            new_value = NULL;
            ujson_object_push_back(new_array, new_element_item);
            new_element_item = NULL;
            state = UJSON_PARSE_IN_OBJECT_STATE_VALUE;
            break;
        case UJSON_PARSE_IN_OBJECT_STATE_VALUE:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if (*p == '}')
            {
                state = UJSON_PARSE_IN_OBJECT_STATE_FINISH;
            }
            else if (*p == ',')
            {
                state = UJSON_PARSE_IN_OBJECT_STATE_COMMA;
                p++;
                len--;
            }
            else
            {
                goto fail;
            }
            break;
        case UJSON_PARSE_IN_OBJECT_STATE_COMMA:
            ujson_skip_whitespace(&p, &len);
            if (len == 0)
            {
                goto fail;
            }
            if ((new_key = ujson_parse_in(&p, &len)) == NULL)
            {
                goto fail;
            }
            state = UJSON_PARSE_IN_OBJECT_STATE_KEY;
            break;
        case UJSON_PARSE_IN_OBJECT_STATE_FINISH:
            break;
        }
    }
    /* Skip '}' */
    p++;
    len--;
    *p_io = p;
    *len_io = len;
    goto done;
fail:
    if (new_array != NULL)
    {
        ujson_destroy_value(new_array);
        new_array = NULL;
    }
    if (new_key != NULL)
    {
        ujson_destroy_value(new_key);
    }
    if (new_value != NULL)
    {
        ujson_destroy_value(new_value);
    }
done:
    return new_array;
}

#define MATCH_IDENTIFIER(p, len, expected_s, expected_len)                     \
    (((len == expected_len) ||                                                 \
      ((len > expected_len) && (!ISID(*(p + expected_len))))) &&               \
     (ujson_strncmp(p, expected_s, expected_len) == 0))

static ujson_t* ujson_parse_in(char** p_io, ujson_size_t* len_io)
{
    char* p = *p_io;
    ujson_size_t len = *len_io;
    ujson_t* result = NULL;
    /* Skip whitespace */
    ujson_skip_whitespace(&p, &len);
    if (ISDIGIT(*p))
    {
        result = ujson_parse_in_number(&p, &len);
    }
    else if (*p == '-')
    {
        result = ujson_parse_in_number(&p, &len);
    }
    else if (*p == '\"')
    {
        result = ujson_parse_in_string(&p, &len);
    }
    else if (MATCH_IDENTIFIER(p, len, "null", 4))
    {
        result = ujson_new(UJSON_NULL);
        p += 4;
        len -= 4;
    }
    else if (MATCH_IDENTIFIER(p, len, "undefined", 9))
    {
        result = ujson_new(UJSON_UNDEFINED);
        p += 9;
        len -= 9;
    }
    else if (MATCH_IDENTIFIER(p, len, "true", 4))
    {
        result = ujson_new_bool(ujson_true);
        p += 4;
        len -= 4;
    }
    else if (MATCH_IDENTIFIER(p, len, "false", 5))
    {
        result = ujson_new_bool(ujson_false);
        p += 5;
        len -= 5;
    }
    else if (*p == '[')
    {
        result = ujson_parse_in_array(&p, &len);
    }
    else if (*p == '{')
    {
        result = ujson_parse_in_object(&p, &len);
    }
    else
    {
        result = NULL;
    }
    *p_io = p;
    *len_io = len;
    return result;
}

/* Parse a JSON string and generate a JSON value */
ujson_t* ujson_parse(char* s, ujson_size_t len)
{
    return ujson_parse_in(&s, &len);
}

static int ujson_stringify_value_number(ujson_mbuf_t* mbuf,
                                        const ujson_t* ujson)
{
    char buf[32];
    int len;
    if (ujson->u.part_number.is_double == ujson_true)
    {
        len = snprintf(buf, 32, "%.12lf", ujson->u.part_number.as_double);
    }
    else
    {
        len = snprintf(buf, 32, "%d", ujson->u.part_number.as_int);
    }
    if (len < 0)
    {
        return -1;
    }
    if (ujson_mbuf_append(mbuf, buf, (ujson_size_t)len) != 0)
    {
        return -1;
    }
    return 0;
}

static int ujson_stringify_value_string(ujson_mbuf_t* mbuf,
                                        const ujson_t* ujson)
{
    char* p = ujson->u.part_string.s;
    ujson_size_t len = ujson->u.part_string.len;
    ujson_size_t bytes_number;
    if (ujson_mbuf_append(mbuf, "\"", 1) != 0)
    {
        return -1;
    }
    while (len != 0)
    {
        switch (*p)
        {
        case '\"':
            if (ujson_mbuf_append(mbuf, "\\\"", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\\':
            if (ujson_mbuf_append(mbuf, "\\\\", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '/':
            if (ujson_mbuf_append(mbuf, "\\/", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\b':
            if (ujson_mbuf_append(mbuf, "\\b", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\f':
            if (ujson_mbuf_append(mbuf, "\\f", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\n':
            if (ujson_mbuf_append(mbuf, "\\n", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\r':
            if (ujson_mbuf_append(mbuf, "\\r", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        case '\t':
            if (ujson_mbuf_append(mbuf, "\\t", 2) != 0)
            {
                return -1;
            }
            p++;
            len--;
            break;
        default:
            if (IS_HYPER_ID(*p))
            {
                bytes_number = id_hyper_length(*p);
                if (len < bytes_number)
                {
                    return -1;
                }
                if (ujson_mbuf_append(mbuf, p, bytes_number) != 0)
                {
                    return -1;
                }
                p += bytes_number;
                len -= bytes_number;
            }
            else
            {
                if (ujson_mbuf_append(mbuf, p, 1) != 0)
                {
                    return -1;
                }
                p++;
                len--;
            }
            break;
        }
    }
    if (ujson_mbuf_append(mbuf, "\"", 1) != 0)
    {
        return -1;
    }
    return 0;
}

static int ujson_stringify_value_array(ujson_mbuf_t* mbuf, const ujson_t* ujson)
{
    int first = 1;
    ujson_array_item_t* item_cur;
    if (ujson_mbuf_append(mbuf, "[", 1) != 0)
    {
        return -1;
    }
    item_cur = ujson->u.part_array.begin;
    while (item_cur != NULL)
    {
        if (first == 1)
        {
            first = 0;
        }
        else
        {
            if (ujson_mbuf_append(mbuf, ",", 1) != 0)
            {
                return -1;
            }
        }
        if (ujson_stringify_value(mbuf, item_cur->value) != 0)
        {
            return -1;
        }
        item_cur = item_cur->next;
    }
    if (ujson_mbuf_append(mbuf, "]", 1) != 0)
    {
        return -1;
    }
    return 0;
}

static int ujson_stringify_value_object(ujson_mbuf_t* mbuf,
                                        const ujson_t* ujson)
{
    int first = 1;
    ujson_object_item_t* item_cur;
    if (ujson_mbuf_append(mbuf, "{", 1) != 0)
    {
        return -1;
    }
    item_cur = ujson->u.part_object.begin;
    while (item_cur != NULL)
    {
        if (first == 1)
        {
            first = 0;
        }
        else
        {
            if (ujson_mbuf_append(mbuf, ",", 1) != 0)
            {
                return -1;
            }
        }
        if (ujson_mbuf_append(mbuf, "\"", 1) != 0)
        {
            return -1;
        }
        if (ujson_mbuf_append(mbuf, item_cur->key.s, item_cur->key.len) != 0)
        {
            return -1;
        }
        if (ujson_mbuf_append(mbuf, "\"", 1) != 0)
        {
            return -1;
        }
        if (ujson_mbuf_append(mbuf, ":", 1) != 0)
        {
            return -1;
        }
        if (ujson_stringify_value(mbuf, item_cur->value) != 0)
        {
            return -1;
        }
        item_cur = item_cur->next;
    }
    if (ujson_mbuf_append(mbuf, "}", 1) != 0)
    {
        return -1;
    }
    return 0;
}

static int ujson_stringify_value(ujson_mbuf_t* mbuf, const ujson_t* ujson)
{
    switch (ujson->type)
    {
    case UJSON_NULL:
        if (ujson_mbuf_append(mbuf, "null", 4) != 0)
        {
            return -1;
        }
        break;
    case UJSON_UNDEFINED:
        if (ujson_mbuf_append(mbuf, "undefined", 9) != 0)
        {
            return -1;
        }
        break;
    case UJSON_BOOL:
        if (ujson->u.part_bool == ujson_false)
        {
            if (ujson_mbuf_append(mbuf, "false", 5) != 0)
            {
                return -1;
            }
        }
        else
        {
            if (ujson_mbuf_append(mbuf, "true", 4) != 0)
            {
                return -1;
            }
        }
        break;
    case UJSON_NUMEBR:
        if (ujson_stringify_value_number(mbuf, ujson) != 0)
        {
            return -1;
        }
        break;
    case UJSON_STRING:
        if (ujson_stringify_value_string(mbuf, ujson) != 0)
        {
            return -1;
        }
        break;
    case UJSON_ARRAY:
        if (ujson_stringify_value_array(mbuf, ujson) != 0)
        {
            return -1;
        }
        break;
    case UJSON_OBJECT:
        if (ujson_stringify_value_object(mbuf, ujson) != 0)
        {
            return -1;
        }
        break;
    }
    return 0;
}

/* Dump a JSON value and product a json string (with config) */
int ujson_stringify_ex(char** json_str, ujson_size_t* json_str_len,
                       const ujson_t* ujson, ujson_stringify_config_t* config)
{
    int ret = 0;
    ujson_mbuf_t mbuf;
    (void)config;
    if (ujson_mbuf_init(&mbuf) != 0)
    {
        return -1;
    }
    if (ujson_stringify_value(&mbuf, ujson) != 0)
    {
        ret = -1;
        goto fail;
    }
    if (ujson_mbuf_extract(json_str, json_str_len, &mbuf) != 0)
    {
        ret = -1;
        goto fail;
    }
fail:
    ujson_mbuf_uninit(&mbuf);
    return ret;
}

/* Dump a JSON value and product a json string */
int ujson_stringify(char** json_str, ujson_size_t* json_str_len,
                    const ujson_t* ujson)
{
    int ret;
    ujson_stringify_config_t config;

    config.style = UJSON_STRINGIFY_CONFIG_STYLE_COMPACT;
    config.replacer = 0;
    config.repeat = 0;

    ret = ujson_stringify_ex(json_str, json_str_len, ujson, &config);

    return ret;
}

static void ujson_destroy_value_array(ujson_t* ujson)
{
    ujson_array_item_t *item_cur, *item_next;

    item_cur = ujson->u.part_array.begin;
    while (item_cur != NULL)
    {
        item_next = item_cur->next;
        ujson_array_item_destroy(item_cur);

        item_cur = item_next;
    }
}

static void ujson_destroy_value_object(ujson_t* ujson)
{
    ujson_object_item_t *item_cur, *item_next;

    item_cur = ujson->u.part_object.begin;
    while (item_cur != NULL)
    {
        item_next = item_cur->next;
        ujson_object_item_destroy(item_cur);

        item_cur = item_next;
    }
}

static void ujson_destroy_value(ujson_t* ujson)
{
    switch (ujson->type)
    {
    case UJSON_NULL:
    case UJSON_BOOL:
    case UJSON_NUMEBR:
    case UJSON_UNDEFINED:
        break;

    case UJSON_STRING:
        if (ujson->u.part_string.s != NULL)
        {
            ujson_free(ujson->u.part_string.s);
        }
        break;

    case UJSON_ARRAY:
        ujson_destroy_value_array(ujson);
        break;

    case UJSON_OBJECT:
        ujson_destroy_value_object(ujson);
        break;
    }
    ujson_free(ujson);
}

void ujson_destroy(ujson_t* ujson) { ujson_destroy_value(ujson); }
