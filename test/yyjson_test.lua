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
        local act, err = yyjson.encode(v.val)
        assert(act, err)
        assert.equal(act, v.exp)

        act, err = yyjson.decode(act)
        assert(not err, err)
        assert.equal(act, v.val)
    end
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
