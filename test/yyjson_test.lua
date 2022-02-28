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

