/**
 *  Copyright (C) 2025 Masatoshi Fukunaga
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

#include <assert.h>
#include <ctype.h>
// lua
#include <lauxhlib.h>
#include <lua_errno.h>
#include <lua_error.h>
// yyjson
#include "yyjson.h"

static const char *read_err2name(yyjson_read_err err)
{
    if (err.code == YYJSON_READ_SUCCESS) {
        /** Success, no error. */
        return "SUCCESS";
    } else if (err.code == YYJSON_READ_ERROR_INVALID_PARAMETER) {
        /** Invalid parameter, such as NULL input string or 0 input length. */
        return "ERROR_INVALID_PARAMETER";
    } else if (err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
        /** Memory allocation failure occurs. */
        return "ERROR_MEMORY_ALLOCATION";
    } else if (err.code == YYJSON_READ_ERROR_EMPTY_CONTENT) {
        /** Input JSON string is empty. */
        return "ERROR_EMPTY_CONTENT";
    } else if (err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT) {
        /** Unexpected content after document, such as `[123]abc`. */
        return "ERROR_UNEXPECTED_CONTENT";
    } else if (err.code == YYJSON_READ_ERROR_UNEXPECTED_END) {
        /** Unexpected ending, such as `[123`. */
        return "ERROR_UNEXPECTED_END";
    } else if (err.code == YYJSON_READ_ERROR_UNEXPECTED_CHARACTER) {
        /** Unexpected character inside the document, such as `[abc]`. */
        return "ERROR_UNEXPECTED_CHARACTER";
    } else if (err.code == YYJSON_READ_ERROR_JSON_STRUCTURE) {
        /** Invalid JSON structure, such as `[1,]`. */
        return "ERROR_JSON_STRUCTURE";
    } else if (err.code == YYJSON_READ_ERROR_INVALID_COMMENT) {
        /** Invalid comment, such as unclosed multi-line comment. */
        return "ERROR_INVALID_COMMENT";
    } else if (err.code == YYJSON_READ_ERROR_INVALID_NUMBER) {
        /** Invalid number, such as `123.e12`, `000`. */
        return "ERROR_INVALID_NUMBER";
    } else if (err.code == YYJSON_READ_ERROR_INVALID_STRING) {
        /** Invalid string, such as invalid escaped character inside a string.
         */
        return "ERROR_INVALID_STRING";
    } else if (err.code == YYJSON_READ_ERROR_LITERAL) {
        /** Invalid JSON literal, such as `truu`. */
        return "ERROR_LITERAL";
    } else if (err.code == YYJSON_READ_ERROR_FILE_OPEN) {
        /** Failed to open a file. */
        return "ERROR_FILE_OPEN";
    } else if (err.code == YYJSON_READ_ERROR_FILE_READ) {
        /** Failed to read a file. */
        return "ERROR_FILE_READ";
    } else {
        return "unsupported error code";
    }
}

static const char *write_err2name(yyjson_write_err err)
{
    if (err.code == YYJSON_WRITE_SUCCESS) {
        /** Success, no error. */
        return "SUCCESS";
    } else if (err.code == YYJSON_WRITE_ERROR_INVALID_PARAMETER) {
        /** Invalid parameter, such as NULL document. */
        return "ERROR_INVALID_PARAMETER";
    } else if (err.code == YYJSON_WRITE_ERROR_MEMORY_ALLOCATION) {
        /** Memory allocation failure occurs. */
        return "ERROR_MEMORY_ALLOCATION";
    } else if (err.code == YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE) {
        /** Invalid value type in JSON document. */
        return "ERROR_INVALID_VALUE_TYPE";
    } else if (err.code == YYJSON_WRITE_ERROR_NAN_OR_INF) {
        /** NaN or Infinity number occurs. */
        return "ERROR_NAN_OR_INF";
    } else if (err.code == YYJSON_WRITE_ERROR_FILE_OPEN) {
        /** Failed to open a file. */
        return "ERROR_FILE_OPEN";
    } else if (err.code == YYJSON_WRITE_ERROR_FILE_WRITE) {
        /** Failed to write a file. */
        return "ERROR_FILE_WRITE";
    } else if (err.code == YYJSON_WRITE_ERROR_INVALID_STRING) {
        /** Invalid unicode in string. */
        return "ERROR_INVALID_STRING";
    }
    return "unsupported error code";
}

static const char *ptr_err2name(yyjson_ptr_err err)
{
    if (err.code == YYJSON_PTR_ERR_NONE) {
        /** No JSON pointer error. */
        return "ERR_NONE";
    } else if (err.code == YYJSON_PTR_ERR_PARAMETER) {
        /** Invalid input parameter, such as NULL input. */
        return "ERR_PARAMETER";
    } else if (err.code == YYJSON_PTR_ERR_SYNTAX) {
        /** JSON pointer syntax error, such as invalid escape, token no prefix.
         */
        return "ERR_SYNTAX";
    } else if (err.code == YYJSON_PTR_ERR_RESOLVE) {
        /** JSON pointer resolve failed, such as index out of range, key not
         * found. */
        return "ERR_RESOLVE";
    } else if (err.code == YYJSON_PTR_ERR_NULL_ROOT) {
        /** Document's root is NULL, but it is required for the function call.
         */
        return "ERR_NULL_ROOT";
    } else if (err.code == YYJSON_PTR_ERR_SET_ROOT) {
        /** Cannot set root as the target is not a document. */
        return "ERR_SET_ROOT";
    } else if (err.code == YYJSON_PTR_ERR_MEMORY_ALLOCATION) {
        /** The memory allocation failed and a new value could not be created.
         */
        return "ERR_MEMORY_ALLOCATION";
    }
    return "unsupported error code";
}

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
        char b[20]  = {};
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
static void *realloc_lua(void *ctx, void *ptr, size_t old_size, size_t size)
{
    (void)old_size;
    memalloc_t *m = (memalloc_t *)ctx;
    lua_State *L  = m->th;
    char b[20]    = {};
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
    char b[20]    = {};
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

#undef tostring_lua

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

#define TV_OK              0x0
#define TV_FOUND_CIRCULAR  0x1
#define TV_FOUND_NOSUPPORT 0x2

static yyjson_mut_val *tovalue(yyjson_mut_doc *doc, lua_State *L, int idx,
                               int refidx, int *tv_status);

static yyjson_mut_val *tovalue_array(yyjson_mut_doc *doc, lua_State *L, int idx,
                                     int refidx, int *tv_status)
{
    size_t tail         = lauxh_rawlen(L, refidx) + 1;
    yyjson_mut_val *bin = yyjson_mut_arr(doc);
    lua_Integer prev    = 0;

    if (!bin) {
        // failed to alloc array
        return NULL;
    }

    // traverse the array
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        // stack: key (-2), value (-1)
        lua_Integer i = (lauxh_isinteger(L, -2)) ? lua_tointeger(L, -2) : 0;

        // convert only array values
        if (i > 0) {
            yyjson_mut_val *val = NULL;

            // push current index to path segments table
            // path[#path + 1] = i
            lua_pushinteger(L, i);
            lua_rawseti(L, refidx, tail);

            // recursively convert the value
            val = tovalue(doc, L, lua_gettop(L), refidx, tv_status);
            if (!val) {
                if ((*tv_status & TV_FOUND_NOSUPPORT) == 0) {
                    // circular reference detected or allocation error
                    lua_pop(L, 2);
                    return NULL;
                }
                *tv_status &= ~TV_FOUND_NOSUPPORT;
            } else if (i < prev) {
                yyjson_mut_arr_insert(bin, val, (size_t)i - 1);
            } else {
                lua_Integer skip = i - prev;
                // fill spaces
                for (lua_Integer n = 1; n < skip; n++) {
                    yyjson_mut_val *nullval = yyjson_mut_null(doc);
                    if (!nullval) {
                        // failed to alloc memory
                        lua_pop(L, 2);
                        return NULL;
                    }
                    yyjson_mut_arr_append(bin, nullval);
                }
                yyjson_mut_arr_append(bin, val);
                prev = i;
            }

            // pop the path segment
            // path[#path] = nil
            lua_pushnil(L);
            lua_rawseti(L, refidx, tail);
        }
        lua_pop(L, 1);
    }

    return bin;
}

static yyjson_mut_val *tovalue_object(yyjson_mut_doc *doc, lua_State *L,
                                      int idx, int refidx, int *tv_status)
{
    size_t tail         = lauxh_rawlen(L, refidx) + 1;
    yyjson_mut_val *bin = yyjson_mut_obj(doc);

    if (!bin) {
        // failed to alloc object
        return NULL;
    }

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        // stack: key (-2), value (-1)

        // convert only object values
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t len          = 0;
            const char *str     = lua_tolstring(L, -2, &len);
            yyjson_mut_val *key = yyjson_mut_strn(doc, str, len);
            yyjson_mut_val *val = NULL;

            if (!key) {
                // failed to alloc memory
                lua_pop(L, 2);
                return NULL;
            }

            // push current key to path segments table
            // path[#path + 1] = key
            lua_pushvalue(L, -2);
            lua_rawseti(L, refidx, tail);

            val = tovalue(doc, L, lua_gettop(L), refidx, tv_status);
            if (val) {
                yyjson_mut_obj_add(bin, key, val);
            } else if ((*tv_status & TV_FOUND_NOSUPPORT) == 0) {
                // circular reference detected or allocation error
                lua_pop(L, 2);
                return NULL;
            }
            *tv_status &= ~TV_FOUND_NOSUPPORT;

            // pop the path segment
            // path[#path] = nil
            lua_pushnil(L);
            lua_rawseti(L, refidx, tail);
        }
        lua_pop(L, 1);
    }

    return bin;
}

#define push_table_ref(L, refidx, idx)                                         \
    do {                                                                       \
        lua_pushvalue(L, idx);                                                 \
        lua_pushboolean(L, 1);                                                 \
        lua_rawset(L, refidx);                                                 \
    } while (0)

#define pop_table_ref(L, refidx, idx)                                          \
    do {                                                                       \
        lua_pushvalue(L, idx);                                                 \
        lua_pushnil(L);                                                        \
        lua_rawset(L, refidx);                                                 \
    } while (0)

static yyjson_mut_val *tovalue_table(yyjson_mut_doc *doc, lua_State *L, int idx,
                                     int refidx, int *tv_status)
{
    yyjson_mut_val *bin = NULL;

    // check if the table is circular reference
    lua_pushvalue(L, idx);
    lua_rawget(L, refidx);
    if (!lua_isnil(L, -1)) {
        // circular reference detected
        lua_pop(L, 1);
        *tv_status |= TV_FOUND_CIRCULAR;
        return NULL;
    }
    lua_pop(L, 1);

    // add to circular reference tracking before any processing
    push_table_ref(L, refidx, idx);

    // if the -1st element of a table is AS_ARRAY or AS_OBJECT, the
    // table is treated as that data type.
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

    if (lauxh_rawlen(L, idx)) {
TREAT_AS_ARRAY:
        // as array
        bin = tovalue_array(doc, L, idx, refidx, tv_status);
    } else {
TREAT_AS_OBJECT:
        // as object
        bin = tovalue_object(doc, L, idx, refidx, tv_status);
    }

    pop_table_ref(L, refidx, idx);

    return bin;
}

#undef push_table_ref
#undef pop_table_ref

static yyjson_mut_val *tovalue(yyjson_mut_doc *doc, lua_State *L, int idx,
                               int refidx, int *tv_status)
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

    case LUA_TTABLE:
        return tovalue_table(doc, L, idx, refidx, tv_status);

    case LUA_TUSERDATA:
        if (lauxh_isuserdataof(L, idx, AS_NULL_MT)) {
            return yyjson_mut_null(doc);
        }

    // case LUA_TLIGHTUSERDATA:
    // case LUA_TFUNCTION:
    // case LUA_TTHREAD:
    default:
        *tv_status |= TV_FOUND_NOSUPPORT;
        return NULL;
    }
}

#define uchar(c) ((unsigned char)(c))

/**
 * NOTE: This function is used to escape strings that copy from Lua source code.
 * https://github.com/lua/lua/blob/v5.4.7/lstrlib.c#L1122-L1142
 */
static void addquoted(luaL_Buffer *b, const char *s, size_t len)
{
    luaL_addchar(b, '"');
    while (len--) {
        if (*s == '"' || *s == '\\' || *s == '\n') {
            luaL_addchar(b, '\\');
            luaL_addchar(b, *s);
        } else if (iscntrl(uchar(*s))) {
            char buff[10];
            if (!isdigit(uchar(*(s + 1)))) {
                snprintf(buff, sizeof(buff), "\\%d", (int)uchar(*s));
            } else {
                snprintf(buff, sizeof(buff), "\\%03d", (int)uchar(*s));
            }
            luaL_addstring(b, buff);
        } else
            luaL_addchar(b, *s);
        s++;
    }
    luaL_addchar(b, '"');
}

/**
 * @brief Builds a JSON path string from a table of path segments.
 * Pushes the resulting string onto the Lua stack.
 *
 * @param L Lua state.
 * @param pathidx Stack index of the table containing path segments (1-based).
 * @param prefix String to prepend to the path.
 */
static void build_path_string(lua_State *L, int pathidx, const char *prefix)
{
    lua_Integer n = lauxh_rawlen(L, pathidx);
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    // Add prefix
    luaL_addstring(&b, prefix);

    // Concatenate path segments
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, pathidx, i);
        if (lua_type(L, -1) == LUA_TNUMBER) {
            // Array index, use brackets
            luaL_addchar(&b, '[');
            // luaL_addvalue pops the value from the stack
            // luaL_addvalue automatically converts number to string
            luaL_addvalue(&b);
            luaL_addchar(&b, ']');
        } else {
            size_t klen   = 0;
            const char *k = lua_tolstring(L, -1, &klen);
            int needs_esc = klen == 0 || isdigit(uchar(*k));

            // Check if the key needs escaping
            if (needs_esc == 0) {
                // Check if the key is a valid Lua identifier
                for (size_t j = 0; j < klen; j++) {
                    if (!isalnum(uchar(k[j])) && k[j] != '_') {
                        needs_esc = 1;
                        break;
                    }
                }
            }

            if (needs_esc) {
                lua_pop(L, 1);
                luaL_addchar(&b, '[');
                addquoted(&b, k, klen);
                luaL_addchar(&b, ']');
                continue;
            }
            // Object key, use dot (assuming simple string keys for simplicity)
            luaL_addchar(&b, '.');
            luaL_addvalue(&b);
        }
    }
    luaL_pushresult(&b); // Push final string onto stack
}

#undef uchar

#define MODULE_MT "yyjson.doc"

typedef struct {
    int ref_str;
    memalloc_t mem;
    yyjson_mut_doc *mdoc;
    yyjson_doc *doc;
} lyyjson_doc_t;

static inline int absidx(lua_State *L, int idx)
{
    return (idx < 0) ? lua_gettop(L) + idx + 1 : idx;
}

static int set_lua(lua_State *L)
{
    lyyjson_doc_t *y       = luaL_checkudata(L, 1, MODULE_MT);
    size_t qlen            = 0;
    const char *qry        = lauxh_optlstr(L, 2, "", &qlen);
    yyjson_mut_val *newval = NULL;
    yyjson_ptr_err err     = {};

    if (!y->mdoc) {
        // convert immutable doc to mutable doc
        yyjson_mut_doc *doc = yyjson_doc_mut_copy(y->doc, &y->mem.alc);
        if (!doc) {
            // failed to deep-copy
            lua_pushboolean(L, 0);
            errno = ENOMEM;
            lua_errno_new(L, errno, "yyjson.doc.set");
            return 2;
        }
        yyjson_doc_free(y->doc);
        y->ref_str = lauxh_unref(L, y->ref_str);
        y->doc     = NULL;
        y->mdoc    = doc;
    }

    if (lua_isnil(L, 3)) {
        // create null value
        newval = yyjson_mut_null(y->mdoc);
    } else if (!lua_isnone(L, 3)) {
        int tv_status = 0;

        // convert specified value
        lua_newtable(L);
        lua_pushvalue(L, 3);
        newval = tovalue(y->mdoc, L, absidx(L, -1), absidx(L, -2), &tv_status);
        if (!newval) {
            const char *errmsg = NULL;

            if (tv_status & TV_FOUND_CIRCULAR) {
                // circular reference detected
                build_path_string(L, absidx(L, -2),
                                  "Circular reference detected at: $");
                errmsg = lua_tostring(L, -1);
                lua_pushboolean(L, 0);
                errno = EINVAL;
                lua_errno_new_with_message(L, errno, "yyjson.doc.set", errmsg);
                return 2;
            } else if (tv_status & TV_FOUND_NOSUPPORT) {
                // unsupported value specified
                lua_pushfstring(L, "Unsupported value type: %s",
                                luaL_typename(L, 3));
                errmsg = lua_tostring(L, -1);
                lua_pushboolean(L, 0);
                errno = EINVAL;
                lua_errno_new_with_message(L, errno, "yyjson.doc.set", errmsg);
                return 2;
            }

            lua_pushboolean(L, 0);
            errno = ENOMEM;
            lua_errno_new(L, errno, "yyjson.doc.set");
            return 2;
        }
    }

    // set value
    if (yyjson_mut_doc_ptr_setx(y->mdoc, qry, qlen, newval, 1, NULL, &err) ||
        (err.code == YYJSON_PTR_ERR_RESOLVE &&
         yyjson_mut_doc_ptr_addx(y->mdoc, qry, qlen, newval, 1, NULL, &err))) {
        lua_pushboolean(L, 1);
        return 1;
    }

    // failed to set value
    lua_pushfstring(L, "%s (%s) at %d", err.msg, ptr_err2name(err), err.pos);
    err.msg = lua_tostring(L, -1);
    errno   = EINVAL;
    if (err.code == YYJSON_PTR_ERR_MEMORY_ALLOCATION) {
        errno = ENOMEM;
    }
    lua_pushboolean(L, 0);
    lua_errno_new_with_message(L, errno, "yyjson.doc.set", err.msg);
    return 2;
}

static int get_mut_value(lua_State *L, yyjson_mut_val *val, const int with_null,
                         const int with_ref);
static int get_value(lua_State *L, yyjson_val *val, const int with_null,
                     const int with_ref);

#define GET_VALUE(L, vtype, v, with_null, with_ref)                            \
    do {                                                                       \
        switch (yyjson##vtype##get_type((v))) {                                \
        case YYJSON_TYPE_NULL:                                                 \
            if (with_null) {                                                   \
                lauxh_pushref(L, AS_NULL_REF);                                 \
                return 1;                                                      \
            }                                                                  \
        case YYJSON_TYPE_NONE:                                                 \
            lua_pushnil(L);                                                    \
            return 1;                                                          \
                                                                               \
        case YYJSON_TYPE_BOOL:                                                 \
            lua_pushboolean(L, yyjson##vtype##get_bool((v)));                  \
            return 1;                                                          \
                                                                               \
        case YYJSON_TYPE_NUM:                                                  \
            switch (yyjson##vtype##get_subtype((v))) {                         \
            case YYJSON_SUBTYPE_UINT:                                          \
                lua_pushinteger(L, yyjson##vtype##get_uint((v)));              \
                break;                                                         \
            case YYJSON_SUBTYPE_SINT:                                          \
                lua_pushinteger(L, yyjson##vtype##get_sint((v)));              \
                break;                                                         \
            case YYJSON_SUBTYPE_REAL:                                          \
                lua_pushnumber(L, yyjson##vtype##get_real((v)));               \
                break;                                                         \
            }                                                                  \
            return 1;                                                          \
                                                                               \
        case YYJSON_TYPE_RAW:                                                  \
        case YYJSON_TYPE_STR:                                                  \
            lua_pushstring(L, yyjson##vtype##get_str((v)));                    \
            return 1;                                                          \
                                                                               \
        case YYJSON_TYPE_ARR: {                                                \
            int rc                     = 0;                                    \
            yyjson##vtype##arr_iter it = {};                                   \
            typeof(v) _val             = NULL;                                 \
                                                                               \
            yyjson##vtype##arr_iter_init((v), &it);                            \
            if (!lua_checkstack(L, 2)) {                                       \
                lua_settop(L, 0);                                              \
                lua_pushnil(L);                                                \
                lua_pushliteral(L, "out of stack space");                      \
                return 2;                                                      \
            }                                                                  \
            lua_createtable(L, it.max, 0);                                     \
            if (with_ref) {                                                    \
                lauxh_pushref(L, AS_ARRAY_REF);                                \
                lua_rawseti(L, -2, -1);                                        \
            }                                                                  \
            while ((_val = yyjson##vtype##arr_iter_next(&it))) {               \
                if ((rc = get##vtype##value(L, _val, with_null, with_ref)) >   \
                    1) {                                                       \
                    return rc;                                                 \
                }                                                              \
                lua_rawseti(L, -2, it.idx);                                    \
            }                                                                  \
            return 1;                                                          \
        }                                                                      \
                                                                               \
        case YYJSON_TYPE_OBJ: {                                                \
            int rc                     = 0;                                    \
            yyjson##vtype##obj_iter it = {};                                   \
            yyjson##vtype##val *key    = NULL;                                 \
                                                                               \
            yyjson##vtype##obj_iter_init((v), &it);                            \
            if (!lua_checkstack(L, 3)) {                                       \
                lua_settop(L, 0);                                              \
                lua_pushnil(L);                                                \
                lua_pushliteral(L, "out of stack space");                      \
                return 2;                                                      \
            }                                                                  \
            lua_createtable(L, 0, it.max);                                     \
            if (with_ref) {                                                    \
                lauxh_pushref(L, AS_OBJECT_REF);                               \
                lua_rawseti(L, -2, -1);                                        \
            }                                                                  \
            while ((key = yyjson##vtype##obj_iter_next(&it))) {                \
                typeof(v) _val = yyjson##vtype##obj_iter_get_val(key);         \
                if ((rc = get##vtype##value(L, _val, with_null, with_ref)) >   \
                    1) {                                                       \
                    return rc;                                                 \
                }                                                              \
                lua_setfield(L, -2, yyjson##vtype##get_str(key));              \
            }                                                                  \
            return 1;                                                          \
        }                                                                      \
                                                                               \
        default:                                                               \
            /* unknown type */                                                 \
            lua_settop(L, 1);                                                  \
            lua_pushnil(L);                                                    \
            lua_pushfstring(L, "unknown value type %d",                        \
                            yyjson##vtype##get_type((v)));                     \
            lua_error_new(L, -1);                                              \
            return 2;                                                          \
        }                                                                      \
    } while (0)

static int get_mut_value(lua_State *L, yyjson_mut_val *val, const int with_null,
                         const int with_ref)
{
    GET_VALUE(L, _mut_, val, with_null, with_ref);
}

static int get_value(lua_State *L, yyjson_val *val, const int with_null,
                     const int with_ref)
{
    GET_VALUE(L, _, val, with_null, with_ref);
}

#undef GET_VALUE

static int get_lua(lua_State *L)
{
    lyyjson_doc_t *y     = luaL_checkudata(L, 1, MODULE_MT);
    size_t len           = 0;
    const char *qry      = lauxh_optlstr(L, 2, "", &len);
    int with_null        = lauxh_optboolean(L, 3, 0);
    int with_ref         = lauxh_optboolean(L, 4, 0);
    yyjson_ptr_err err   = {};
    yyjson_val *val      = NULL;
    yyjson_mut_val *mval = NULL;

    errno = 0;
    if (y->mdoc) {
        mval = yyjson_mut_doc_ptr_getx(y->mdoc, qry, len, NULL, &err);
    } else {
        val = yyjson_doc_ptr_getx(y->doc, qry, len, &err);
    }

    if (err.code == YYJSON_PTR_ERR_RESOLVE) {
        // key not found
        lua_pushnil(L);
        return 1;
    } else if (err.msg) {
        // failed to get value
        lua_pushfstring(L, "%s (%s) at %d", err.msg, ptr_err2name(err),
                        err.pos);
        err.msg = lua_tostring(L, -1);
        lua_pushnil(L);
        errno = EINVAL;
        if (err.code == YYJSON_PTR_ERR_MEMORY_ALLOCATION) {
            errno = ENOMEM;
        }
        lua_errno_new_with_message(L, errno, "yyjson.doc.get", err.msg);
        return 2;
    }

    lua_settop(L, 1);
    return (mval) ? get_mut_value(L, mval, with_null, with_ref) :
                    get_value(L, val, with_null, with_ref);
}

static int stringify_lua(lua_State *L)
{
    lyyjson_doc_t *y      = luaL_checkudata(L, 1, MODULE_MT);
    yyjson_write_flag flg = lauxh_optflags(L, 2);
    yyjson_write_err err  = {};
    size_t len            = 0;
    const char *str       = NULL;

    errno = 0;
    if (y->mdoc) {
        str = yyjson_mut_write_opts(y->mdoc, flg, &y->mem.alc, &len, &err);
    } else {
        str = yyjson_write_opts(y->doc, flg, &y->mem.alc, &len, &err);
    }

    if (str) {
        lua_pushlstring(L, str, len);
        y->mem.alc.free(y->mem.alc.ctx, (void *)str);
        return 1;
    }

    // got error
    lua_pushfstring(L, "%s (%s)", err.msg, write_err2name(err));
    err.msg = lua_tostring(L, -1);
    errno   = EINVAL;
    if (err.code == YYJSON_WRITE_ERROR_MEMORY_ALLOCATION) {
        errno = ENOMEM;
    }
    lua_pushnil(L);
    lua_errno_new_with_message(L, errno, "yyjson.doc.stringify", err.msg);
    return 2;
}

static int readsize_lua(lua_State *L)
{
    lyyjson_doc_t *y = luaL_checkudata(L, 1, MODULE_MT);
    lua_pushinteger(L, (y->doc) ? yyjson_doc_get_read_size(y->doc) : 0);
    return 1;
}

static int tostring_lua(lua_State *L)
{
    lua_pushfstring(L, MODULE_MT ": %p", lua_touserdata(L, 1));
    return 1;
}

static int gc_lua(lua_State *L)
{
    lyyjson_doc_t *y = lua_touserdata(L, 1);

    if (y->doc) {
        yyjson_doc_free(y->doc);
    }
    if (y->mdoc) {
        yyjson_mut_doc_free(y->mdoc);
    }
    lauxh_unref(L, y->ref_str);
    memalloc_dispose(&y->mem);
    return 0;
}

static int new_lua(lua_State *L)
{
    size_t len           = 0;
    const char *str      = lauxh_optlstr(L, 1, NULL, &len);
    lua_Integer maxsize  = lauxh_optinteger(L, 2, 0);
    yyjson_read_flag flg = lauxh_optflags(L, 3);
    lyyjson_doc_t *y     = lua_newuserdata(L, sizeof(lyyjson_doc_t));

    y->ref_str = LUA_NOREF;
    y->doc     = NULL;
    y->mdoc    = NULL;
    memalloc_init(&y->mem, L, (maxsize < 0) ? 0 : (size_t)maxsize);
    lauxh_setmetatable(L, MODULE_MT);

    if (len) {
        yyjson_read_err err = {};

        // create document with specified JSON string
        flg &= ~YYJSON_READ_INSITU;
        errno  = 0;
        y->doc = yyjson_read_opts((char *)str, len, flg, &y->mem.alc, &err);
        if (err.msg) {
            // got error
            lua_pushfstring(L, "%s (%s) at %d", err.msg, read_err2name(err),
                            err.pos);
            err.msg = lua_tostring(L, -1);
            lua_pushnil(L);
            if (err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
                errno = ENOMEM;
            } else if (!errno) {
                errno = EINVAL;
            }
            lua_errno_new_with_message(L, errno, "yyjson.doc.new", err.msg);
            return 2;
        }
        // retain parsed JSON string
        y->ref_str = lauxh_refat(L, -1);
        return 1;
    }

    // create empty document
    y->mdoc = yyjson_mut_doc_new(&y->mem.alc);
    if (!y->mdoc) {
        lua_pushnil(L);
        errno = ENOMEM;
        lua_errno_new(L, errno, "yyjson.doc.new");
        return 2;
    }
    return 1;
}

LUALIB_API int luaopen_yyjson_doc(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__gc",       gc_lua      },
        {"__tostring", tostring_lua},
        {NULL,         NULL        }
    };
    struct luaL_Reg method[] = {
        {"readsize",  readsize_lua },
        {"stringify", stringify_lua},
        {"get",       get_lua      },
        {"set",       set_lua      },
        {NULL,        NULL         }
    };

    // create metatable
    luaL_newmetatable(L, MODULE_MT);
    // add metamethods
    for (struct luaL_Reg *ptr = mmethod; ptr->name; ptr++) {
        lauxh_pushfn2tbl(L, ptr->name, ptr->func);
    }
    // add methods
    lua_newtable(L);
    for (struct luaL_Reg *ptr = method; ptr->name; ptr++) {
        lauxh_pushfn2tbl(L, ptr->name, ptr->func);
    }
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_errno_loadlib(L);
    init_aux_objects(L);

    lua_newtable(L);
    // export functions
    lauxh_pushfn2tbl(L, "new", new_lua);

    // export symbols
    lauxh_pushref(L, AS_OBJECT_REF);
    lua_setfield(L, -2, "AS_OBJECT");

    lauxh_pushref(L, AS_ARRAY_REF);
    lua_setfield(L, -2, "AS_ARRAY");

    lauxh_pushref(L, AS_NULL_REF);
    lua_setfield(L, -2, "NULL");

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

    /** Read big numbers as raw strings. These big numbers include integers that
     * cannot be represented by `int64_t` and `uint64_t`, and floating-point
     * numbers that cannot be represented by finite `double`.
     * The flag will be
     * overridden by `YYJSON_READ_NUMBER_AS_RAW` flag. */
    lauxh_pushint2tbl(L, "READ_BIGNUM_AS_RAW", YYJSON_READ_BIGNUM_AS_RAW);

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

    /** Write JSON pretty with 2 space indent.
     * This flag will override `YYJSON_WRITE_PRETTY` flag. */
    lauxh_pushint2tbl(L, "WRITE_PRETTY_TWO_SPACES",
                      YYJSON_WRITE_PRETTY_TWO_SPACES);

    /** Adds a newline character `\n` at the end of the JSON.
        This can be helpful for text editors or NDJSON. */
    lauxh_pushint2tbl(L, "WRITE_NEWLINE_AT_END", YYJSON_WRITE_NEWLINE_AT_END);

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
