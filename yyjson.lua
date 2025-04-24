--
-- Copyright (C) 2025 Masatoshi Fukunaga
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--
-- module
local yyjson_doc = require('yyjson.doc')
local new_doc = yyjson_doc.new

local _M = {}
-- export the constants of yyjson.doc module
for k, v in pairs(yyjson_doc) do
    if type(k) == 'string' and type(v) ~= 'function' and k:match('^[A-Z]') then
        _M[k] = v
    end
end

--- decode a JSON string to a Lua table
--- @param str string the JSON string to decode
--- @param with_null boolean? whether to decode null elements to yyjson.NULL
--- @param with_ref boolean? whether to insert yyjson.AS_OBJECT and yyjson.AS_ARRAY to the 0th element of the table
--- @param mlimit integer? the maximum memory limit for decoding
--- @param ... integer the flags for decoding
--- @return any value decoded lua value from the JSON string
--- @return any err
--- @return integer? readsize the size of the JSON string decoded
local function decode(str, with_null, with_ref, mlimit, ...)
    assert(type(str) == 'string', 'str must be a string')

    local doc, err = new_doc(str, mlimit, ...)
    if not doc then
        return nil, err
    end
    local readsize = doc:readsize()

    local v
    v, err = doc:get(nil, with_null, with_ref)
    return v, err, readsize
end
_M.decode = decode

--- encode a Lua value to a JSON string
--- @param val any the Lua value to encode
--- @param mlimit integer? the maximum memory limit for encoding
--- @param ... integer the flags for encoding
local function encode(val, mlimit, ...)
    local doc, err = new_doc(nil, mlimit)
    if not doc then
        return nil, err
    end

    local ok
    ok, err = doc:set('', val)
    if not ok then
        return nil, err
    end

    return doc:stringify(...)
end
_M.encode = encode

return _M
