/**
 *  Copyright (C) 2022 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 *
 */

#include "yyjson.h"
#include <assert.h>
#include <lauxhlib.h>

typedef struct {
    lua_State *L;
    int ref;
    lua_State *th;
    lua_Alloc allocf;
    void *ud;
    yyjson_alc alc;
    size_t usesize;
    size_t maxsize;
    int nomem;
} memalloc_t;

/* Same as libc's malloc(), should not be NULL. */
static void *malloc_lua(void *ctx, size_t size)
{
    memalloc_t *m = (memalloc_t *)ctx;
    lua_State *L  = m->th;
    void *ptr     = NULL;

    if (m->maxsize &&
        (SIZE_MAX - size < m->usesize || size + m->usesize > m->maxsize)) {
        // reached to maximum memory limit
        m->nomem = 1;
        return NULL;
    }

    ptr = m->allocf(m->ud, NULL, 0, size);
    if (ptr) {
        // keep alloc size
        // buffer for hexadecimal string 0xFFFFFFFFFFFFFFFF
        char b[20]  = {0};
        size_t blen = snprintf(b, sizeof(b), "%p", ptr);
        size_t *sz  = NULL;

        m->usesize += size;
        lua_pushlstring(L, b, blen);
        sz  = (size_t *)lua_newuserdata(L, sizeof(size_t));
        *sz = size;
        lua_rawset(L, 1);
    } else {
        m->nomem = 1;
    }

    return ptr;
}

/* Same as libc's realloc(), should not be NULL. */
static void *realloc_lua(void *ctx, void *ptr, size_t size)
{
    memalloc_t *m = (memalloc_t *)ctx;
    lua_State *L  = m->th;
    char b[20]    = {0};
    size_t blen   = 0;
    size_t *sz    = NULL;
    void *newptr  = NULL;

    if (m->maxsize &&
        (SIZE_MAX - size < m->usesize || size + m->usesize > m->maxsize)) {
        // reached to maximum memory limit
        m->nomem = 1;
        return NULL;
    }

    // get alloc size
    blen = snprintf(b, sizeof(b), "%p", ptr);
    lua_pushlstring(L, b, blen);
    lua_pushvalue(L, 2);
    lua_rawget(L, 1);
    sz     = (size_t *)lua_topointer(L, -1);
    // realloc
    newptr = m->allocf(m->ud, ptr, *sz, size);
    if (newptr) {
        // remove old alloc size
        lua_pushvalue(L, 2);
        lua_pushnil(L);
        lua_rawset(L, 1);

        // keep new alloc size
        m->usesize = m->usesize - *sz + size;
        *sz        = size;
        blen       = snprintf(b, sizeof(b), "%p", newptr);
        lua_pushlstring(L, b, blen);
        lua_replace(L, 2);
        lua_rawset(L, 1);
    } else {
        m->nomem = 1;
        lua_settop(L, 1);
    }

    return newptr;
}

/* Same as libc's free(), should not be NULL. */
static void free_lua(void *ctx, void *ptr)
{
    memalloc_t *m = (memalloc_t *)ctx;
    lua_State *L  = m->th;
    char b[20]    = {0};
    size_t blen   = snprintf(b, sizeof(b), "%p", ptr);
    size_t size   = 0;

    // get alloc size
    lua_pushlstring(L, b, blen);
    lua_pushvalue(L, 2);
    lua_rawget(L, 1);
    size = *(size_t *)lua_topointer(L, -1);
    lua_pop(L, 1);
    // remove alloc size
    lua_pushnil(L);
    lua_rawset(L, 1);
    // free
    m->allocf(m->ud, ptr, size, 0);
    m->usesize -= size;
}

static void memalloc_dispose(memalloc_t *m)
{
    assert(m->usesize == 0);
    lauxh_unref(m->L, m->ref);
}

static void memalloc_init(memalloc_t *m, lua_State *L, size_t maxsize)
{
    m->L      = L;
    m->allocf = lua_getallocf(L, &m->ud);
    m->th     = lua_newthread(L);
    m->ref    = lauxh_ref(L);

    // create the table that keeps alloc size of each pointer
    // key: pointer address in representation of hexadecimal string
    // value: alloc size that is stored in size_t*
    lua_newtable(m->th);
    m->alc.malloc  = malloc_lua;
    m->alc.realloc = realloc_lua;
    m->alc.free    = free_lua;
    m->alc.ctx     = (void *)m;
    m->usesize     = 0;
    m->maxsize     = maxsize;
    m->nomem       = 0;
}

#define AS_OBJECT_MT "yyjson.as_object"
#define AS_ARRAY_MT  "yyjson.as_array"
#define AS_NULL_MT   "yyjson.null"

static void *AS_OBJECT   = NULL;
static void *AS_ARRAY    = NULL;
static void *AS_NULL     = NULL;
static int AS_OBJECT_REF = LUA_NOREF;
static int AS_ARRAY_REF  = LUA_NOREF;
static int AS_NULL_REF   = LUA_NOREF;

#define tostring_lua(L, tname)                                                 \
    do {                                                                       \
        lauxh_isuserdataof((L), 1, (tname));                                   \
        lua_pushliteral((L), tname);                                           \
    } while (0)

static int object_tostring_lua(lua_State *L)
{
    tostring_lua(L, AS_OBJECT_MT);
    return 1;
}

static int array_tostring_lua(lua_State *L)
{
    tostring_lua(L, AS_ARRAY_MT);
    return 1;
}

static int null_tostring_lua(lua_State *L)
{
    tostring_lua(L, AS_NULL_MT);
    return 1;
}

static inline void init_aux_objects(lua_State *L)
{
    // create auxiliary object metatables
    AS_OBJECT = lua_newuserdata(L, 0);
    luaL_newmetatable(L, AS_OBJECT_MT);
    lauxh_pushfn2tbl(L, "__tostring", object_tostring_lua);
    lua_setmetatable(L, -2);
    AS_OBJECT_REF = lauxh_ref(L);

    AS_ARRAY = lua_newuserdata(L, 0);
    luaL_newmetatable(L, AS_ARRAY_MT);
    lauxh_pushfn2tbl(L, "__tostring", array_tostring_lua);
    lua_setmetatable(L, -2);
    AS_ARRAY_REF = lauxh_ref(L);

    AS_NULL = lua_newuserdata(L, 0);
    luaL_newmetatable(L, AS_NULL_MT);
    lauxh_pushfn2tbl(L, "__tostring", null_tostring_lua);
    lua_setmetatable(L, -2);
    AS_NULL_REF = lauxh_ref(L);
}

static int pushvalue(lua_State *L, yyjson_val *val, int with_null, int with_ref)
{
    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
        if (with_null) {
            lauxh_pushref(L, AS_NULL_REF);
            return 1;
        }
    case YYJSON_TYPE_NONE:
        lua_pushnil(L);
        return 1;

    case YYJSON_TYPE_BOOL:
        lua_pushboolean(L, yyjson_get_bool(val));
        return 1;

    case YYJSON_TYPE_NUM:
        switch (yyjson_get_subtype(val)) {
        case YYJSON_SUBTYPE_UINT:
            lua_pushinteger(L, yyjson_get_uint(val));
            break;
        case YYJSON_SUBTYPE_SINT:
            lua_pushinteger(L, yyjson_get_sint(val));
            break;
        case YYJSON_SUBTYPE_REAL:
            lua_pushnumber(L, yyjson_get_real(val));
            break;
        }
        return 1;

    case YYJSON_TYPE_STR:
        lua_pushstring(L, yyjson_get_str(val));
        return 1;

    case YYJSON_TYPE_ARR: {
        int rc             = 0;
        yyjson_arr_iter it = {0};

        yyjson_arr_iter_init(val, &it);
        if (!lua_checkstack(L, 2)) {
            lua_settop(L, 0);
            lua_pushnil(L);
            lua_pushliteral(L, "out of stack space");
            return 2;
        }
        lua_createtable(L, it.max, 0);
        if (with_ref) {
            lauxh_pushref(L, AS_ARRAY_REF);
            lua_rawseti(L, -2, -1);
        }
        while ((val = yyjson_arr_iter_next(&it))) {
            if ((rc = pushvalue(L, val, with_null, with_ref)) > 1) {
                return rc;
            }
            lua_rawseti(L, -2, it.idx);
        }
        return 1;
    }

    case YYJSON_TYPE_OBJ: {
        int rc             = 0;
        yyjson_obj_iter it = {0};
        yyjson_val *key    = NULL;

        yyjson_obj_iter_init(val, &it);
        if (!lua_checkstack(L, 3)) {
            lua_settop(L, 0);
            lua_pushnil(L);
            lua_pushliteral(L, "out of stack space");
            return 2;
        }
        lua_createtable(L, 0, it.max);
        if (with_ref) {
            lauxh_pushref(L, AS_OBJECT_REF);
            lua_rawseti(L, -2, -1);
        }
        while ((key = yyjson_obj_iter_next(&it))) {
            val = yyjson_obj_iter_get_val(key);
            if ((rc = pushvalue(L, val, with_null, with_ref)) > 1) {
                return rc;
            }
            lua_setfield(L, -2, yyjson_get_str(key));
        }
        return 1;
    }

    default:
        // unknown type
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushfstring(L, "unknown value type %d", yyjson_get_type(val));
        return 2;
    }
}

static int decode_lua(lua_State *L)
{
    size_t len           = 0;
    const char *str      = lauxh_checklstring(L, 1, &len);
    int with_null        = lauxh_optboolean(L, 2, 0);
    int with_ref         = lauxh_optboolean(L, 3, 0);
    lua_Integer maxsize  = lauxh_optinteger(L, 4, 0);
    yyjson_read_flag flg = lauxh_optflags(L, 5);
    yyjson_read_err err  = {0};
    yyjson_doc *doc      = NULL;
    memalloc_t m         = {0};
    int rc               = 3;

    lua_settop(L, 1);
    memalloc_init(&m, L, (maxsize < 0) ? 0 : (size_t)maxsize);
    if (flg & YYJSON_READ_INSITU) {
        len -= YYJSON_PADDING_SIZE;
    }
    doc = yyjson_read_opts((char *)str, len, flg, &m.alc, &err);
    if (doc) {
        rc = pushvalue(L, yyjson_doc_get_root(doc), with_null, with_ref);
        if (rc == 1) {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushinteger(L, yyjson_doc_get_read_size(doc));
            rc = 4;
        }
        yyjson_doc_free(doc);
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "%s at %d", err.msg, err.pos);
        lua_pushinteger(L, err.code);
    }
    memalloc_dispose(&m);

    return rc;
}

static yyjson_mut_val *tovalue(yyjson_mut_doc *doc, lua_State *L, int idx)
{
    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        return yyjson_mut_null(doc);

    case LUA_TBOOLEAN:
        return yyjson_mut_bool(doc, lua_toboolean(L, idx));

    case LUA_TNUMBER:
        if (lauxh_isinteger(L, idx)) {
            lua_Integer ival = lua_tointeger(L, idx);
            if (ival > 0) {
                return yyjson_mut_uint(doc, ival);
            }
            return yyjson_mut_sint(doc, ival);
        }
        return yyjson_mut_real(doc, lua_tonumber(L, idx));

    case LUA_TSTRING: {
        size_t len      = 0;
        const char *str = lua_tolstring(L, idx, &len);
        return yyjson_mut_strn(doc, str, len);
    }

    case LUA_TTABLE: {
        yyjson_mut_val *bin = NULL;

        // if the -1st element of a table is AS_ARRAY or AS_OBJECT, the table is
        // treated as that data type.
        lua_rawgeti(L, idx, -1);
        if (lua_type(L, -1) == LUA_TUSERDATA) {
            const void *ptr = lua_topointer(L, -1);
            lua_pop(L, 1);
            if (ptr == AS_OBJECT) {
                goto TREAT_AS_OBJECT;
            } else if (ptr == AS_ARRAY) {
                goto TREAT_AS_ARRAY;
            }
        }
        lua_pop(L, 1);

        // as array
        if (lauxh_rawlen(L, idx)) {
            lua_Integer prev = 0;

TREAT_AS_ARRAY:
            if (!(bin = yyjson_mut_arr(doc))) {
                // failed to alloc memory
                return NULL;
            }
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                lua_Integer i       = 0;
                yyjson_mut_val *val = NULL;

                if (lauxh_isinteger(L, -2) && (i = lua_tointeger(L, -2)) > 0 &&
                    (val = tovalue(doc, L, lua_gettop(L)))) {
                    if (i < prev) {
                        yyjson_mut_arr_insert(bin, val, (size_t)i - 1);
                    } else {
                        lua_Integer skip = i - prev;
                        // fill spaces
                        for (lua_Integer n = 1; n < skip; n++) {
                            yyjson_mut_val *nullval = yyjson_mut_null(doc);
                            if (!val) {
                                // failed to alloc memory
                                return NULL;
                            }
                            yyjson_mut_arr_append(bin, nullval);
                        }
                        yyjson_mut_arr_append(bin, val);
                        prev = i;
                    }
                }
                lua_pop(L, 1);
            }
            return bin;
        }

TREAT_AS_OBJECT:
        // as object
        if (!(bin = yyjson_mut_obj(doc))) {
            // failed to alloc memory
            return NULL;
        }
        lua_pushnil(L);
        while (lua_next(L, idx) != 0) {
            yyjson_mut_val *val = NULL;

            if (lua_type(L, -2) == LUA_TSTRING &&
                (val = tovalue(doc, L, lua_gettop(L)))) {
                yyjson_mut_val *key = tovalue(doc, L, lua_gettop(L) - 1);
                if (!key) {
                    // failed to alloc memory
                    return NULL;
                }
                yyjson_mut_obj_add(bin, key, val);
            }
            lua_pop(L, 1);
        }
        return bin;
    }

    case LUA_TUSERDATA:
        if (lauxh_isuserdataof(L, idx, AS_NULL_MT)) {
            return yyjson_mut_null(doc);
        }

    // case LUA_TLIGHTUSERDATA:
    // case LUA_TFUNCTION:
    // case LUA_TTHREAD:
    default:
        return NULL;
    }
}

static int encode_lua(lua_State *L)
{
    yyjson_read_flag flg = 0;
    yyjson_write_err err = {0};
    yyjson_mut_doc *doc  = NULL;
    yyjson_mut_val *val  = NULL;
    size_t len           = 0;
    const char *str      = NULL;
    lua_Integer maxsize  = 0;
    memalloc_t m         = {0};
    int rc               = 3;

    luaL_checkany(L, 1);
    maxsize = lauxh_optinteger(L, 2, 0);
    flg     = lauxh_optflags(L, 3);
    memalloc_init(&m, L, (maxsize < 0) ? 0 : (size_t)maxsize);

    doc = yyjson_mut_doc_new(&m.alc);
    if (!doc) {
        err.msg  = strerror(ENOMEM);
        err.code = YYJSON_READ_ERROR_MEMORY_ALLOCATION;
        goto FAIL;
    }

    lua_settop(L, 1);
    val = tovalue(doc, L, 1);
    if (m.nomem || (!val && !(val = yyjson_mut_null(doc)))) {
        err.msg  = strerror(ENOMEM);
        err.code = YYJSON_READ_ERROR_MEMORY_ALLOCATION;
        goto FAIL;
    }
    yyjson_mut_doc_set_root(doc, val);

    str = yyjson_mut_write_opts(doc, flg, &m.alc, &len, &err);
    if (!str) {
FAIL:
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, err.msg);
        lua_pushinteger(L, err.code);
    } else {
        lua_pushlstring(L, str, len);
        m.alc.free(m.alc.ctx, (void *)str);
        rc = 1;
    }
    yyjson_mut_doc_free(doc);
    memalloc_dispose(&m);

    return rc;
}

LUALIB_API int luaopen_yyjson(lua_State *L)
{
    init_aux_objects(L);

    lua_createtable(L, 0, 2);
    // export symbols
    lauxh_pushref(L, AS_OBJECT_REF);
    lua_setfield(L, -2, "AS_OBJECT");

    lauxh_pushref(L, AS_ARRAY_REF);
    lua_setfield(L, -2, "AS_ARRAY");

    lauxh_pushref(L, AS_NULL_REF);
    lua_setfield(L, -2, "NULL");

    // export functions
    lauxh_pushfn2tbl(L, "encode", encode_lua);
    lauxh_pushfn2tbl(L, "decode", decode_lua);

    /** Options for JSON reader. */
    /** Default option (RFC 8259 compliant):
        - Read positive integer as uint64_t.
        - Read negative integer as int64_t.
        - Read floating-point number as double with correct rounding.
        - Read integer which cannot fit in uint64_t or int64_t as double.
        - Report error if real number is infinity.
        - Report error if string contains invalid UTF-8 character or BOM.
        - Report error on trailing commas, comments, inf and nan literals. */
    lauxh_pushint2tbl(L, "READ_NOFLAG", YYJSON_READ_NOFLAG);
    /** Read the input data in-situ.
        This option allows the reader to modify and use input data to store
       string values, which can increase reading speed slightly. The caller
       should hold the input data before free the document. The input data must
       be padded by at least `YYJSON_PADDING_SIZE` byte. For example: "[1,2]"
       should be "[1,2]\0\0\0\0", length should be 5. */
    lauxh_pushint2tbl(L, "READ_INSITU", YYJSON_READ_INSITU);
    lauxh_pushint2tbl(L, "PADDING_SIZE", YYJSON_PADDING_SIZE);
    /** Stop when done instead of issues an error if there's additional content
        after a JSON document. This option may used to parse small pieces of
       JSON in larger data, such as NDJSON. */
    lauxh_pushint2tbl(L, "READ_STOP_WHEN_DONE", YYJSON_READ_STOP_WHEN_DONE);
    /** Allow single trailing comma at the end of an object or array,
        such as [1,2,3,] {"a":1,"b":2,}. */
    lauxh_pushint2tbl(L, "READ_ALLOW_TRAILING_COMMAS",
                      YYJSON_READ_ALLOW_TRAILING_COMMAS);
    /** Allow C-style single line and multiple line comments. */
    lauxh_pushint2tbl(L, "READ_ALLOW_COMMENTS", YYJSON_READ_ALLOW_COMMENTS);
    /** Allow inf/nan number and literal, case-insensitive,
        such as 1e999, NaN, inf, -Infinity. */
    lauxh_pushint2tbl(L, "READ_ALLOW_INF_AND_NAN",
                      YYJSON_READ_ALLOW_INF_AND_NAN);
    /** Read number as raw string (value with YYJSON_TYPE_RAW type),
    inf/nan literal is also read as raw with `ALLOW_INF_AND_NAN` flag. */
    lauxh_pushint2tbl(L, "READ_NUMBER_AS_RAW", YYJSON_READ_NUMBER_AS_RAW);
    /** Allow reading invalid unicode when parsing string values (non-standard).
        Invalid characters will be allowed to appear in the string values, but
        invalid escape sequences will still be reported as errors.
        This flag does not affect the performance of correctly encoded strings.

        @warning Strings in JSON values may contain incorrect encoding when this
        option is used, you need to handle these strings carefully to avoid
       security risks. */
    lauxh_pushint2tbl(L, "READ_ALLOW_INVALID_UNICODE",
                      YYJSON_READ_ALLOW_INVALID_UNICODE);

    /** Result code for JSON reader. */
    /** Success, no error. */
    lauxh_pushint2tbl(L, "READ_SUCCESS", YYJSON_READ_SUCCESS);
    /** Invalid parameter, such as NULL string or invalid file path. */
    lauxh_pushint2tbl(L, "READ_ERROR_INVALID_PARAMETER",
                      YYJSON_READ_ERROR_INVALID_PARAMETER);
    /** Memory allocation failure occurs. */
    lauxh_pushint2tbl(L, "READ_ERROR_MEMORY_ALLOCATION",
                      YYJSON_READ_ERROR_MEMORY_ALLOCATION);
    /** Input JSON string is empty. */
    lauxh_pushint2tbl(L, "READ_ERROR_EMPTY_CONTENT",
                      YYJSON_READ_ERROR_EMPTY_CONTENT);
    /** Unexpected content after document, such as "[1]#". */
    lauxh_pushint2tbl(L, "READ_ERROR_UNEXPECTED_CONTENT",
                      YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    /** Unexpected ending, such as "[123". */
    lauxh_pushint2tbl(L, "READ_ERROR_UNEXPECTED_END",
                      YYJSON_READ_ERROR_UNEXPECTED_END);
    /** Unexpected character inside the document, such as "[#]". */
    lauxh_pushint2tbl(L, "READ_ERROR_UNEXPECTED_CHARACTER",
                      YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
    /** Invalid JSON structure, such as "[1,]". */
    lauxh_pushint2tbl(L, "READ_ERROR_JSON_STRUCTURE",
                      YYJSON_READ_ERROR_JSON_STRUCTURE);
    /** Invalid comment, such as unclosed multi-line comment. */
    lauxh_pushint2tbl(L, "READ_ERROR_INVALID_COMMENT",
                      YYJSON_READ_ERROR_INVALID_COMMENT);
    /** Invalid number, such as "123.e12", "000". */
    lauxh_pushint2tbl(L, "READ_ERROR_INVALID_NUMBER",
                      YYJSON_READ_ERROR_INVALID_NUMBER);
    /** Invalid string, such as invalid escaped character inside a string. */
    lauxh_pushint2tbl(L, "READ_ERROR_INVALID_STRING",
                      YYJSON_READ_ERROR_INVALID_STRING);
    /** Invalid JSON literal, such as "truu". */
    lauxh_pushint2tbl(L, "READ_ERROR_LITERAL", YYJSON_READ_ERROR_LITERAL);
    /** Failed to open a file. */
    lauxh_pushint2tbl(L, "READ_ERROR_FILE_OPEN", YYJSON_READ_ERROR_FILE_OPEN);
    /** Failed to read a file. */
    lauxh_pushint2tbl(L, "READ_ERROR_FILE_READ", YYJSON_READ_ERROR_FILE_READ);

    /** Options for JSON writer. */
    /** Default option:
        - Write JSON minify.
        - Report error on inf or nan number.
        - Do not validate string encoding.
        - Do not escape unicode or slash. */
    lauxh_pushint2tbl(L, "WRITE_NOFLAG", YYJSON_WRITE_NOFLAG);
    /** Write JSON pretty with 4 space indent. */
    lauxh_pushint2tbl(L, "WRITE_PRETTY", YYJSON_WRITE_PRETTY);
    /** Escape unicode as `uXXXX`, make the output ASCII only. */
    lauxh_pushint2tbl(L, "WRITE_ESCAPE_UNICODE", YYJSON_WRITE_ESCAPE_UNICODE);
    /** Escape '/' as '\/'. */
    lauxh_pushint2tbl(L, "WRITE_ESCAPE_SLASHES", YYJSON_WRITE_ESCAPE_SLASHES);
    /** Write inf and nan number as 'Infinity' and 'NaN' literal (non-standard).
     */
    lauxh_pushint2tbl(L, "WRITE_ALLOW_INF_AND_NAN",
                      YYJSON_WRITE_ALLOW_INF_AND_NAN);
    /** Write inf and nan number as null literal.
        This flag will override `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag. */
    lauxh_pushint2tbl(L, "WRITE_INF_AND_NAN_AS_NULL",
                      YYJSON_WRITE_INF_AND_NAN_AS_NULL);
    /** Allow invalid unicode when encoding string values (non-standard).
        Invalid characters in string value will be copied byte by byte.
        If `YYJSON_WRITE_ESCAPE_UNICODE` flag is also set, invalid character
        will be escaped as `U+FFFD` (replacement character). This flag does not
        affect the performance of correctly encoded strings. */
    lauxh_pushint2tbl(L, "WRITE_ALLOW_INVALID_UNICODE",
                      YYJSON_WRITE_ALLOW_INVALID_UNICODE);

    /** Result code for JSON writer */
    /** Success, no error. */
    lauxh_pushint2tbl(L, "WRITE_SUCCESS", YYJSON_WRITE_SUCCESS);
    /** Invalid parameter, such as NULL document. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_INVALID_PARAMETER",
                      YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    /** Memory allocation failure occurs. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_MEMORY_ALLOCATION",
                      YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
    /** Invalid value type in JSON document. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_INVALID_VALUE_TYPE",
                      YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE);
    /** NaN or Infinity number occurs. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_NAN_OR_INF",
                      YYJSON_WRITE_ERROR_NAN_OR_INF);
    /** Failed to open a file. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_FILE_OPEN", YYJSON_WRITE_ERROR_FILE_OPEN);
    /** Failed to write a file. */
    lauxh_pushint2tbl(L, "WRITE_ERROR_FILE_WRITE",
                      YYJSON_WRITE_ERROR_FILE_WRITE);

    return 1;
}
