local testcase = require('testcase')
local assert = require('assert')
local yyjson = require('yyjson')

function testcase.encode_decode()
    -- test that returns the decoded value
    for _, v in ipairs({
        {
            val = nil,
            exp = 'null',
        },
        {
            val = true,
            exp = 'true',
        },
        {
            val = false,
            exp = 'false',
        },
        {
            val = 1,
            exp = '1',
        },
        {
            val = -567,
            exp = '-567',
        },
        {
            val = 1.05,
            exp = '1.05',
        },
        {
            val = 1.05,
            exp = '1.05',
        },
        {
            val = 'hello',
            exp = '"hello"',
        },
        {
            val = {
                true,
                false,
                1,
                1.05,
                nil,
                'hello',
            },
            exp = '[true,false,1,1.05,null,"hello"]',
        },
        {
            val = {
                baz = {
                    qux = {
                        true,
                        false,
                        1,
                        1.05,
                        nil,
                        'hello',
                        {
                            foo = 'bar',
                        },
                    },
                },
            },
            exp = '{"baz":{"qux":[true,false,1,1.05,null,"hello",{"foo":"bar"}]}}',
        },
    }) do
        local enc_act = assert(yyjson.encode(v.val))
        assert.equal(enc_act, v.exp)

        local dec_act, _, len = yyjson.decode(enc_act)
        assert.equal(dec_act, v.val)
        assert.equal(len, #enc_act)
    end
end

function testcase.decode_empty_content()
    -- test that decode empty content
    local s = table.concat({
        string.rep('\t', 2),
        string.rep(' ', 4),
        string.rep('\n', 5),
        string.rep(' ', 10),
    })
    local act, err, len = yyjson.decode(s)
    assert.is_nil(act)
    assert.match(err, 'ERROR_EMPTY_CONTENT')
    assert.is_nil(len)
end

function testcase.decode_ndjson()
    -- test that decode NDJSON with READ_STOP_WHEN_DONE flag
    local ndjson = {
        '[true,false,1,1.05,null,"hello"]',
        '{"baz":{"qux":[true,false,1,1.05,null,"hello",{"foo":"bar"}]}}',
    }
    local exp = {
        {
            true,
            false,
            1,
            1.05,
            nil,
            'hello',
        },
        {
            baz = {
                qux = {
                    true,
                    false,
                    1,
                    1.05,
                    nil,
                    'hello',
                    {
                        foo = 'bar',
                    },
                },
            },
        },
    }
    local s = table.concat(ndjson, '')
    for i = 1, #ndjson do
        local act, err, len = assert(yyjson.decode(s, nil, nil, nil,
                                                   yyjson.READ_STOP_WHEN_DONE))
        assert.equal(act, exp[i])
        assert.is_nil(err)
        assert.equal(len, #ndjson[i])
        s = string.sub(s, len + 1)
    end

    -- test that return error without READ_STOP_WHEN_DONE flag
    s = table.concat(ndjson, '')
    local act, err, len = yyjson.decode(s)
    assert.is_nil(act)
    assert.match(err, 'ERROR_UNEXPECTED_CONTENT')
    assert.is_nil(len)
end

function testcase.encode_null()
    -- test that encode yyjson.NULL value to null
    local exp = {
        foo = yyjson.NULL,
        bar = {
            true,
            yyjson.NULL,
            'hello',
            {
                baz = 'qux',
            },
        },
    }
    local s = assert(yyjson.encode(exp))
    local act = assert(yyjson.decode(s, true))
    assert.equal(act, exp)
end

function testcase.decode_with_null()
    -- test that decode null value to yyjson.NULL
    local exp = {
        foo = yyjson.NULL,
        bar = {
            true,
            yyjson.NULL,
            'hello',
            {
                baz = 'qux',
            },
        },
    }
    local s = '{"foo": null, "bar":[true,null,"hello",{"baz":"qux"}]}'
    local act = assert(yyjson.decode(s, true))
    assert.equal(act, exp)
end

function testcase.decode_with_ref()
    -- test that decode null value to yyjson.NULL
    local exp = {
        [-1] = yyjson.AS_OBJECT,
        bar = {
            [-1] = yyjson.AS_ARRAY,
            true,
            nil,
            'hello',
            {
                [-1] = yyjson.AS_OBJECT,
                baz = 'qux',
            },
        },
    }
    local s = '{"foo": null, "bar":[true,null,"hello",{"baz":"qux"}]}'
    local act = assert(yyjson.decode(s, nil, true))
    assert.equal(act, exp)
end

function testcase.memory_limit_encode()
    -- test that limit memory usage for encoding
    local v, err = yyjson.encode({
        [-1] = yyjson.AS_OBJECT,
        foo = yyjson.NULL,
        bar = {
            [-1] = yyjson.AS_ARRAY,
            true,
            yyjson.NULL,
            'hello',
            {
                [-1] = yyjson.AS_OBJECT,
                baz = 'qux',
            },
        },
    }, 200)
    assert.is_nil(v)
    assert.match(err, 'ENOMEM')

    -- test that limit memory usage for encoding
    v, err = yyjson.encode('foo', 10)
    assert.is_nil(v)
    assert.match(err, 'ENOMEM')
end

function testcase.memory_limit_decode()
    -- test that limit memory usage for dencoding
    local s = '{"foo": null, "bar":[true,null,"hello",{"baz":"qux"}]}'
    local v, err = yyjson.decode(s, nil, nil, 100)
    assert.is_nil(v)
    assert.match(err, 'ENOMEM')
end

function testcase.decode_with_bom()
    -- test that READ_ALLOW_BOM consumes a UTF-8 BOM before the JSON document
    local s = '\239\187\191{"a":1}'

    -- without the flag, the BOM byte is rejected
    local v, err = yyjson.decode(s)
    assert.is_nil(v)
    assert.match(err, 'EINVAL')

    -- with the flag, the BOM is ignored and parsing succeeds
    v, err = yyjson.decode(s, nil, nil, nil, yyjson.READ_ALLOW_BOM)
    assert.is_nil(err)
    assert.equal(v, {
        a = 1,
    })
end

function testcase.decode_single_quoted_string()
    local s = "{\"key\":'value'}"

    -- without the flag, single-quoted strings are rejected
    local v, err = yyjson.decode(s)
    assert.is_nil(v)
    assert.match(err, 'EINVAL')

    -- with the flag, single-quoted strings are accepted
    v, err =
        yyjson.decode(s, nil, nil, nil, yyjson.READ_ALLOW_SINGLE_QUOTED_STR)
    assert.is_nil(err)
    assert.equal(v, {
        key = 'value',
    })
end

function testcase.decode_unquoted_key()
    local s = '{key:"value"}'

    -- without the flag, unquoted keys are rejected
    local v, err = yyjson.decode(s)
    assert.is_nil(v)
    assert.match(err, 'EINVAL')

    -- with the flag, unquoted keys are accepted
    v, err = yyjson.decode(s, nil, nil, nil, yyjson.READ_ALLOW_UNQUOTED_KEY)
    assert.is_nil(err)
    assert.equal(v, {
        key = 'value',
    })
end

function testcase.decode_ext_number()
    local s = '{"hex":0x7B,"plus":+1.5}'

    -- without the flag, hex and explicit positive sign are rejected
    local v, err = yyjson.decode(s)
    assert.is_nil(v)
    assert.match(err, 'EINVAL')

    -- with the flag, extended number formats are accepted
    v, err = yyjson.decode(s, nil, nil, nil, yyjson.READ_ALLOW_EXT_NUMBER)
    assert.is_nil(err)
    assert.equal(v.hex, 123)
    assert.equal(v.plus, 1.5)
end

function testcase.decode_json5()
    local s = "{ a: 'hello', b: 1, /* comment */ }"

    -- without the flag, JSON5 syntax is rejected
    local v, err = yyjson.decode(s)
    assert.is_nil(v)
    assert.match(err, 'EINVAL')

    -- READ_JSON5 enables comments, trailing commas, single-quoted strings,
    -- unquoted keys, extended numbers/escapes/whitespace and inf/nan literals
    -- in one combined flag
    v, err = yyjson.decode(s, nil, nil, nil, yyjson.READ_JSON5)
    assert.is_nil(err)
    assert.equal(v, {
        a = 'hello',
        b = 1,
    })
end

function testcase.encode_fp_to_float()
    -- pick a value with enough precision to show the difference between
    -- double and float output
    local v = 3.14159265358979

    -- default encoding writes the value with double precision
    local s = assert(yyjson.encode(v))
    assert.equal(s, '3.14159265358979')

    -- WRITE_FP_TO_FLOAT downcasts to single precision, producing the
    -- shorter representation that round-trips through float
    s = assert(yyjson.encode(v, nil, yyjson.WRITE_FP_TO_FLOAT))
    assert.equal(s, '3.1415927')
end
