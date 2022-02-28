# lua-yyjson

[![test](https://github.com/mah0x211/lua-yyjson/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-yyjson/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-yyjson/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-yyjson)

lua bindings for yyjson.


## Usage

```lua
local yyjson = require('yyjson')

-- treat table as an array if it contains array elements
print(yyjson.encode({
    'foo',
    hello = 'world',
    'bar',
    nil,
    qux = 'quux',
    nil,
    'baz',
})) -- ["foo","bar",null,null,"baz"]

-- treat table as an object if it contains only key/value pairs
print(yyjson.encode({
    hello = 'world',
    qux = 'quux',
})) -- {"hello":"world","qux":"quux"}
```
