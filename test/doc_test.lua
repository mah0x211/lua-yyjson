local testcase = require('testcase')
local assert = require('assert')
local yyjson_doc = require('yyjson.doc')

function testcase.new()
    -- test that yyjson_doc.new() returns a new document
    local doc, err = yyjson_doc.new()
    assert.is_nil(err)
    assert.re_match(doc, '^yyjson.doc: ')
end

function testcase.new_with_json_string()
    -- test that yyjson_doc.new() parsing the JSON string
    local doc, err = yyjson_doc.new('{"key": "value"}')
    assert.is_nil(err)
    assert.re_match(doc, '^yyjson.doc: ')

    -- test that returns nil and an error if the JSON string is invalid
    local invalid_json_string = '{"key": "value"'
    doc, err = yyjson_doc.new(invalid_json_string)
    assert.re_match(err, 'EINVAL')
    assert.is_nil(doc)
end

function testcase.get()
    local doc = assert(yyjson_doc.new('{"key": "value"}'))

    -- test that yyjson_doc:get() returns the value for a given key
    local parsed_value, err = doc:get('/key')
    assert.is_nil(err)
    assert.equal(parsed_value, 'value')

    -- test that returns nil if the key is not found
    parsed_value, err = doc:get('/unknown_key')
    assert.is_nil(err)
    assert.is_nil(parsed_value)

    -- test that returns nil and an error if the path is invalid
    parsed_value, err = doc:get('invalid/key')
    assert.re_match(err, 'ERR_SYNTAX')
    assert.is_nil(parsed_value)
end

function testcase.set()
    local doc = assert(yyjson_doc.new(
                           '{ "foo": "foo-value", "bar": { "baz": ["hello", "world"] } }'))
    assert.equal(assert(doc:get()), {
        foo = 'foo-value',
        bar = {
            baz = {
                'hello',
                'world',
            },
        },
    })

    -- test that add a new key-value pair
    assert(doc:set('/qux/quux', 'quux-value'))
    -- confirm that the value is set
    assert.equal(assert(doc:get()), {
        foo = 'foo-value',
        bar = {
            baz = {
                'hello',
                'world',
            },
        },
        qux = {
            quux = 'quux-value',
        },
    })

    -- test that add value to array
    assert(doc:set('/bar/baz/2', '!'))
    assert.equal(assert(doc:get()), {
        foo = 'foo-value',
        bar = {
            baz = {
                'hello',
                'world',
                '!',
            },
        },
        qux = {
            quux = 'quux-value',
        },
    })

    -- test that replace value of array
    assert(doc:set('/bar/baz/1', 'JSON Pointer'))
    assert.equal(assert(doc:get()), {
        foo = 'foo-value',
        bar = {
            baz = {
                'hello',
                'JSON Pointer',
                '!',
            },
        },
        qux = {
            quux = 'quux-value',
        },
    })

    -- test that set null to array if the value is nil
    assert(doc:set('/bar/baz/0', nil))
    assert.equal(assert(doc:get(nil, true)), {
        foo = 'foo-value',
        bar = {
            baz = {
                yyjson_doc.NULL,
                'JSON Pointer',
                '!',
            },
        },
        qux = {
            quux = 'quux-value',
        },
    })

    -- test that delete value if value no specified
    assert(doc:set('/qux'))
    assert.equal(assert(doc:get(nil, true)), {
        foo = 'foo-value',
        bar = {
            baz = {
                yyjson_doc.NULL,
                'JSON Pointer',
                '!',
            },
        },
    })

    -- test that replace value
    assert(doc:set('/bar', {
        1,
        2,
        3,
    }))
    assert.equal(assert(doc:get(nil, true)), {
        foo = 'foo-value',
        bar = {
            1,
            2,
            3,
        },
    })

    -- test that replace whole document
    assert(doc:set('', {
        hello = 'world',
    }))
    assert.equal(assert(doc:get(nil, true)), {
        hello = 'world',
    })

    -- test that ignore unsupported type of value
    assert(doc:set('/foo', {
        str = 'string',
        num = 123,
        bool = true,
        arr = {
            1,
            2,
            3,
        },
        obj = {
            key = 'value',
        },
        func = function()
        end,
        thread = coroutine.create(function()
        end),
    }))
    assert.equal(assert(doc:get(nil, true)), {
        hello = 'world',
        foo = {
            str = 'string',
            num = 123,
            bool = true,
            arr = {
                1,
                2,
                3,
            },
            obj = {
                key = 'value',
            },
        },
    })

    -- tst that return error if first value is unsupported value
    local ok, err = doc:set('/foo', coroutine.create(function()
    end))
    assert.match(err, 'Unsupported value type: thread')
    assert.is_false(ok)

    -- test that cannot set into existing value
    ok, err = doc:set('/hello/foo', 'foo-value')
    assert.re_match(err, 'ERR_RESOLVE')
    assert.is_false(ok)

    -- test that cannot set circular reference in object
    local t = {
        foo = {
            bar = {
                ["b'\tz"] = {},
            },
        },
    }
    t.foo.bar["b'\tz"].t = t
    ok, err = doc:set('/foo', t)
    assert.match(err, "Circular reference detected at: $.foo.bar")
    assert.is_false(ok)

    -- test that cannot set circular reference in array
    t = {
        foo = {
            bar = {},
        },
    }
    t.foo.bar[1] = t
    ok, err = doc:set('/foo', t)
    assert.match(err, "Circular reference detected at: $.foo.bar[1]")
    assert.is_false(ok)
end
