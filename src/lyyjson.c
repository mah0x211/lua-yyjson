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
#include <lauxhlib.h>

static int pushvalue(lua_State *L, yyjson_val *val)
{
    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NONE:
    case YYJSON_TYPE_NULL:
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
        while ((val = yyjson_arr_iter_next(&it))) {
            if ((rc = pushvalue(L, val)) > 1) {
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
        while ((key = yyjson_obj_iter_next(&it))) {
            val = yyjson_obj_iter_get_val(key);
            if ((rc = pushvalue(L, val)) > 1) {
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
    yyjson_read_flag flg = lauxh_optflags(L, 2);
    yyjson_read_err err  = {0};
    yyjson_doc *doc      = NULL;

    lua_settop(L, 1);
    if ((doc = yyjson_read_opts((char *)str, len, flg, NULL, &err))) {
        int rc = pushvalue(L, yyjson_doc_get_root(doc));
        yyjson_doc_free(doc);
        return rc;
    }

    lua_pushnil(L);
    lua_pushfstring(L, "%s at %d", err.msg, err.pos);
    lua_pushinteger(L, err.code);
    return 3;
}

static inline yyjson_mut_val *tovalue(yyjson_mut_doc *doc, lua_State *L,
                                      int idx)
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
        size_t len          = lauxh_rawlen(L, idx);
        yyjson_mut_val *bin = NULL;

        // as array
        if (len) {
            lua_Integer prev = 0;
            bin              = yyjson_mut_arr(doc);
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                lua_Integer i       = 0;
                yyjson_mut_val *val = NULL;

                if (lauxh_isinteger(L, -2) && (i = lua_tointeger(L, -2)) > 0 &&
                    (val = tovalue(doc, L, lua_gettop(L)))) {
                    if (i < prev) {
                        yyjson_mut_arr_insert(bin, val, (size_t)i);
                    } else {
                        lua_Integer skip = i - prev;
                        for (lua_Integer n = 1; n < skip; n++) {
                            yyjson_mut_arr_append(bin, yyjson_mut_null(doc));
                        }
                        yyjson_mut_arr_append(bin, val);
                        prev = i;
                    }
                }
                lua_pop(L, 1);
            }
            return bin;
        }

        // as object
        bin = yyjson_mut_obj(doc);
        lua_pushnil(L);
        while (lua_next(L, idx) != 0) {
            yyjson_mut_val *val = NULL;

            if (lua_type(L, -2) == LUA_TSTRING &&
                (val = tovalue(doc, L, lua_gettop(L)))) {
                yyjson_mut_val *key = tovalue(doc, L, lua_gettop(L) - 1);
                yyjson_mut_obj_add(bin, key, val);
            }
            lua_pop(L, 1);
        }
        return bin;
    }

    // case LUA_TLIGHTUSERDATA:
    // case LUA_TFUNCTION:
    // case LUA_TUSERDATA:
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

    luaL_checkany(L, 1);
    flg = lauxh_optflags(L, 2);
    doc = yyjson_mut_doc_new(NULL);

    lua_settop(L, 1);
    val = tovalue(doc, L, 1);
    if (!val) {
        val = yyjson_mut_null(doc);
    }
    yyjson_mut_doc_set_root(doc, val);

    str = yyjson_mut_write_opts(doc, flg, NULL, &len, &err);
    if (!str) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, err.msg);
        lua_pushinteger(L, err.code);
        yyjson_mut_doc_free(doc);
        return 3;
    }

    lua_pushlstring(L, str, len);
    yyjson_mut_doc_free(doc);
    return 1;
}

LUALIB_API int luaopen_yyjson(lua_State *L)
{
    lua_createtable(L, 0, 2);
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