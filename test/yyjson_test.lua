local testcase = require('testcase')
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

        local dec_act, _, _, len = yyjson.decode(enc_act)
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
    local act, err, errno, len = yyjson.decode(s)
    assert.is_nil(act)
    assert.is_string(err)
    assert.equal(errno, yyjson.READ_ERROR_EMPTY_CONTENT)
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
        local act, err, errno, len = assert(
                                         yyjson.decode(s, nil, nil, nil,
                                                       yyjson.READ_STOP_WHEN_DONE))
        assert.equal(act, exp[i])
        assert.is_nil(err)
        assert.is_nil(errno)
        assert.equal(len, #ndjson[i])
        s = string.sub(s, len + 1)
    end

    -- test that return error without READ_STOP_WHEN_DONE flag
    s = table.concat(ndjson, '')
    local act, err, errno, len = yyjson.decode(s)
    assert.is_nil(act)
    assert.is_string(err)
    assert.equal(errno, yyjson.READ_ERROR_UNEXPECTED_CONTENT)
    assert.is_nil(len)
end

function testcase.decode_insitu()
    -- test that decode value with READ_INSITU flag
    local exp = {
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
    }
    local s =
        '{"baz":{"qux":[true,false,1,1.05,null,"hello",{"foo":"bar"}]}}' ..
            string.rep(string.char(0), yyjson.PADDING_SIZE)
    local act = assert(yyjson.decode(s, nil, nil, nil, yyjson.READ_INSITU))
    assert.equal(act, exp)
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

function testcase.memory_limit()
    -- test that limit memory usage for encoding
    local v, err, errno = yyjson.encode({
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
    assert.match(err, 'memory')
    assert.equal(errno, yyjson.WRITE_ERROR_MEMORY_ALLOCATION)

    -- test that limit memory usage for dencoding
    local s = '{"foo": null, "bar":[true,null,"hello",{"baz":"qux"}]}'
    v, err, errno = yyjson.decode(s, nil, nil, 100)
    assert.is_nil(v)
    assert.match(err, 'memory')
    assert.equal(errno, yyjson.READ_ERROR_MEMORY_ALLOCATION)
end
