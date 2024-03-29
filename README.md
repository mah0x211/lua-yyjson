# lua-yyjson

[![test](https://github.com/mah0x211/lua-yyjson/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-yyjson/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-yyjson/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-yyjson)

lua bindings for https://github.com/ibireme/yyjson.


## Installation

```sh
luarocks install yyjson
```


## Usage

```lua
local yyjson = require('yyjson')
local dump = require('dump')

-- treat table as an object if it contains only key/value pairs
local s = assert(yyjson.encode({
    hello = 'world',
    qux = 'quux',
}))
print(s) -- {"hello":"world","qux":"quux"}

-- treat table as an array if the -1st element of a table is yyjson.AS_ARRAY
s = assert(yyjson.encode({
    [-1] = yyjson.AS_ARRAY,
    hello = 'world',
    qux = 'quux',
}))
print(s) -- []

-- treat table as an object if the -1st element of a table is yyjson.AS_OBJECT
s = assert(yyjson.encode({
    [-1] = yyjson.AS_OBJECT,
    'foo',
    hello = 'world',
    'bar',
    nil,
    qux = 'quux',
    nil,
    'baz',
}))
print(s) -- {"hello":"world","qux":"quux"}

-- treat table as an array if #table > 0
s = assert(yyjson.encode({
    'foo',
    hello = 'world',
    'bar',
    nil,
    qux = 'quux',
    nil,
    'baz',
}))
print(s) -- ["foo","bar",null,null,"baz"]

-- null elements are decoded to nil
local v = assert(yyjson.decode(s))
print(dump(v))
-- {
--     [1] = "foo",
--     [2] = "bar",
--     [5] = "baz"
-- }

-- null elements are decoded to yyjson.NULL
v = assert(yyjson.decode(s, true))
print(dump(v))
-- {
--     [1] = "foo",
--     [2] = "bar",
--     [3] = "yyjson.null",
--     [4] = "yyjson.null",
--     [5] = "baz"
-- }

-- decoded with the type reference
v = assert(yyjson.decode(s, nil, true))
print(dump(v))
-- {
--     [-1] = "yyjson.as_array",
--     [1] = "foo",
--     [2] = "bar",
--     [5] = "baz"
-- }

-- decode multiple json
local mjson = '[1,2,3]\n[4,5,6] {"a":"b"}[7, 8, 9] '
while #mjson > 0 do
    local val, err, errno, len = yyjson.decode(mjson, nil, nil, nil,
                                               yyjson.READ_STOP_WHEN_DONE)
    if not len then
        assert(errno == yyjson.READ_ERROR_EMPTY_CONTENT, err)
        break
    end
    print(dump(val))
    mjson = string.sub(mjson, len + 1)
end
-- {
--     [1] = 1,
--     [2] = 2,
--     [3] = 3
-- }
-- {
--     [1] = 4,
--     [2] = 5,
--     [3] = 6
-- }
-- {
--     a = "b"
-- }
-- {
--     [1] = 7,
--     [2] = 8,
--     [3] = 9
-- }
```

## About Memory Allocation

`lua-yyjson` uses the `lua_Alloc` function to allocate memory.  
Therefore, the amount of memory available depends on the `lua_Alloc` function associated with `lua_State*`.


## s, err, errno = yyjson.encode( v [, mlimit [, ...]])

encode a Lua value `v` to a JSON string.

**Parameters**

- `v:boolean|string|number|table`: a value to encode to a JSON string.
- `mlimit:integer`: if a value greater than `0` is specified, the maximum memory usage is limited to this value.
- `...integer`: the following flags can be specified;

| flag | description |
|:-----|:------------|
| `yyjson.WRITE_NOFLAG` | default flag:<br>- Write JSON minify.<br>- Report error on `inf` or `nan` number.<br>- Do not validate string encoding.<br>- Do not escape unicode or slash. |
| `yyjson.WRITE_PRETTY` | Write JSON pretty with `4` space indent. |
| `yyjson.WRITE_ESCAPE_UNICODE` | Escape unicode as `uXXXX`, make the output ASCII only. |
| `yyjson.WRITE_ESCAPE_SLASHES` | Escape `/` as `\/`. |
| `yyjson.WRITE_ALLOW_INF_AND_NAN` | Write `inf` and `nan` number as `Infinity` and `NaN` literal (non-standard). |
| `yyjson.WRITE_INF_AND_NAN_AS_NULL` | Write `inf` and `nan` number as `null` literal.<br>This flag will override `yyjson.WRITE_ALLOW_INF_AND_NAN` flag. |
| `yyjson.WRITE_ALLOW_INVALID_UNICODE` | Allow invalid unicode when encoding string values (non-standard).<br>Invalid characters in string value will be copied byte by byte.<br>If `WRITE_ESCAPE_UNICODE` flag is also set, invalid character will be escaped as `U+FFFD` (replacement character).<br>This flag does not affect the performance of correctly encoded strings. |
| `yyjson.WRITE_PRETTY_TWO_SPACES` | Write JSON pretty with 2 space indent.<br>This flag will override 


**Returns**

- `s:string`: a JSON string
- `err:string`: error message.
- `errno:integer`: the following error number;
    - `yyjson.WRITE_ERROR_INVALID_PARAMETER`: Invalid parameter, such as `NULL` document.
    - `yyjson.WRITE_ERROR_MEMORY_ALLOCATION`: Memory allocation failure occurs.
    - `yyjson.WRITE_ERROR_INVALID_VALUE_TYPE`: Invalid value type in JSON document.
    - `yyjson.WRITE_ERROR_NAN_OR_INF`: `NaN` or `Infinity` number occurs.
    - `yyjson.WRITE_ERROR_FILE_OPEN`: Failed to open a file.
    - `yyjson.WRITE_ERROR_FILE_WRITE`: Failed to write a file.

**NOTE:** 

The `table` value will be handling as follows;

- if the `-1st` element is `yyjson.AS_OBJECT`, treat table as an object.
- if the `-1st` element is `yyjson.AS_ARRAY`, treat table as an array.
- if the length of table (`#table`) is greater than `0`, treat table as an array.


## v, err, errno, len = yyjson.decode( s [, with_null [, with_ref [, mlimit [, ...]]]])

decode a JSON string `s` to a Lua value.

**Parameters**

- `s:string`: a JSON string.
- `with_null:boolean`: `true` to decode a `null` to `yyjson.NULL`.
- `with_ref:boolean`: `true` to add `yyjson.AS_OBJECT` and `yyjson.AS_ARRAY` to the array element `-1` of the table.
- `mlimit:integer`: if a value greater than `0` is specified, the maximum memory usage is limited to this value.
- `...:integer`: the following flags can be specified;

| flag | description |
|:-----|:------------|
| `yyjson.READ_NOFLAG` | default flag (RFC 8259 compliant):<br>- Read positive integer as `uint64_t`.<br>- Read negative integer as `int64_t`.<br>- Read floating-point number as double with correct rounding.<br>- Read integer which cannot fit in `uint64_t` or `int64_t` as `double`.<br>- Report error if real number is `infinity`.<br>- Report error if string contains invalid `UTF-8` character or `BOM`.<br>- Report error on `trailing commas`, `comments`, `inf` and `nan` literals. |
| `yyjson.READ_INSITU` | Read the input data in-situ.<br>This flag allows the reader to modify and use input data to store string values, which can increase reading speed slightly.<br>The caller should hold the input data before free the document.<br>The input data must be padded by at least `yyjson.PADDING_SIZE` byte.<br>For example: "[1,2]" should be "[1,2]\0\0\0\0", length should be 5. |
| `yyjson.READ_STOP_WHEN_DONE` | Stop when done instead of issues an error if there's additional content after a JSON document. This flag may used to parse small pieces of JSON in larger data, such as `NDJSON (Newline Delimited JSON)`. |
| `yyjson.READ_ALLOW_TRAILING_COMMAS` | Allow single trailing comma at the end of an object or array, such as `[1,2,3,]` `{"a":1,"b":2,}`. |
| `yyjson.READ_ALLOW_COMMENTS` | Allow C-style single line and multiple line comments. |
| `yyjson.READ_ALLOW_INF_AND_NAN` | Allow `inf`/`nan` number and literal, case-insensitive, such as `1e999`, `NaN`, `inf`, `-Infinity`. |
| `yyjson.READ_NUMBER_AS_RAW` | Read number as raw string (value with `YYJSON_TYPE_RAW` type), `inf`/`nan` literal is also read as raw with `ALLOW_INF_AND_NAN` flag. |
| `yyjson.READ_ALLOW_INVALID_UNICODE` | Allow reading invalid unicode when parsing string values.<br>Invalid characters will be allowed to appear in the string values, but invalid escape sequences will still be reported as errors.<br>This flag does not affect the performance of correctly encoded strings.<br>**@warning** Strings in JSON values may contain incorrect encoding when this option is used, you need to handle these strings carefully to avoid security risks. |
| `yyjson.READ_BIGNUM_AS_RAW` | Read big numbers as raw strings.<br>These big numbers include integers that cannot be represented by `int64_t` and `uint64_t`, and floating-point numbers that cannot be represented by finite `double`.<br>The flag will be overridden by `yyjson.READ_NUMBER_AS_RAW` flag. |

**Returns**

- `v:boolean|string|number|table`: a decoded value.
- `err:string`: error message.
- `errno:integer`: the following error number;
    - `yyjson.READ_ERROR_INVALID_PARAMETER`: Invalid parameter, such as `NULL` string or invalid file path.
    - `yyjson.READ_ERROR_MEMORY_ALLOCATION`: Memory allocation failure occurs.
    - `yyjson.READ_ERROR_EMPTY_CONTENT`: Input JSON string is empty.
    - `yyjson.READ_ERROR_UNEXPECTED_CONTENT`: Unexpected content after document, such as `"[1]#"`.
    - `yyjson.READ_ERROR_UNEXPECTED_END`: Unexpected ending, such as `"[123"`.
    - `yyjson.READ_ERROR_UNEXPECTED_CHARACTER`: Unexpected character inside the document, such as `"[#]"`.
    - `yyjson.READ_ERROR_JSON_STRUCTURE`: Invalid JSON structure, such as `"[1,]"`.
    - `yyjson.READ_ERROR_INVALID_COMMENT`: Invalid comment, such as unclosed multi-line comment.
    - `yyjson.READ_ERROR_INVALID_NUMBER`: Invalid number, such as `"123.e12"`, `"000"`.
    - `yyjson.READ_ERROR_INVALID_STRING`: Invalid string, such as invalid escaped character inside a string.
    - `yyjson.READ_ERROR_LITERAL`: Invalid JSON literal, such as `"truu"`.
    - `yyjson.READ_ERROR_FILE_OPEN`: Failed to open a file.
    - `yyjson.READ_ERROR_FILE_READ`: Failed to read a file.
- `len:integer`: read size of JSON string.

