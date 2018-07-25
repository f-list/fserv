role_names = { "owner", "admin", "global", "super", "cop", "normal" }

OK = const.FERR_OK
NOT_OP = const.FERR_NOT_OP

cons = {}

function globalSetUp()
    for _, name in ipairs(role_names) do
        cons[name] = testing.createConnection(name)
    end
    u.setGlobMod(cons.global, true)
    u.setAdmin(cons.admin, true)
    u.addRole(cons.super, "super-cop")
    u.addRole(cons.super, "cop")
    u.addRole(cons.cop, "cop")
end

function globalTearDown()
    for _, con in ipairs(role_names) do
        testing.removeConnection(con)
    end
    cons = {}
end

function channelSetUp()
    c.createChannel("public")
    local pchan = c.createSpecialPrivateChannel("private", "private channel")
    local _, chan = c.getChannel("public")
    c.setOwner(chan, cons.owner)
    c.addMod(chan, cons.owner, "cop")
    c.setOwner(pchan, cons.owner)
    c.addMod(pchan, cons.owner, "cop")
    c.setPublic(pchan, true)
    for _, con in pairs(cons) do
        c.join(chan, con)
        c.join(pchan, con)
    end
end

function channelTearDown()
    testing.killChannel("private")
    testing.killChannel("test")
end

CALL_ALLOWED = { { "public", "normal", OK }, { "private", "normal", OK } }
PUBLIC_ALLOWED = { { "public", "normal", OK }, { "private", "normal", NOT_OP } }
NONE_ALLOWED = { { "public", "normal", NOT_OP }, { "private", "normal", NOT_OP } }

PRIV_CALL_ALLOWED = { { "private", "normal", OK} }
PRIV_NONE_ALLOWED = { { "private", "normal", NOT_OP} }

NormalMatrix = {
    owner = CALL_ALLOWED,
    admin = CALL_ALLOWED,
    global = CALL_ALLOWED,
    super = PUBLIC_ALLOWED,
    cop = CALL_ALLOWED,
    normal = NONE_ALLOWED
}

OwnerOnlyMatrix = {
    owner = CALL_ALLOWED,
    admin = CALL_ALLOWED,
    global = CALL_ALLOWED,
    super = NONE_ALLOWED,
    cop = NONE_ALLOWED,
    normal = NONE_ALLOWED
}

PrivateOwnerOnlyMatrix = {
    owner = PRIV_CALL_ALLOWED,
    admin = PRIV_CALL_ALLOWED,
    global = PRIV_CALL_ALLOWED,
    super = PRIV_NONE_ALLOWED,
    cop = PRIV_NONE_ALLOWED,
    normal = PRIV_NONE_ALLOWED
}

function ChannelTest(callEvent, matrix, paramsFunc)
    for role, testParams in pairs(matrix) do
        for _, params in ipairs(testParams) do
            channelSetUp()
            print("Running " .. callEvent .. " event with role " .. role .. " on channel " .. params[1])
            local ret = event[callEvent](cons[role], paramsFunc(params[1], params[2]))
            testing.assert(ret, params[3])
            channelTearDown()
        end
    end
end

function CBUTest()
    ChannelTest("CBU", NormalMatrix, function(channel, target)
        return { channel = channel, character = target }
    end)
end

function CKUTest()
    ChannelTest("CKU", NormalMatrix, function(channel, target)
        return { channel = channel, character = target }
    end)
end

function CTUTest()
    ChannelTest("CTU", NormalMatrix, function(channel, target)
        return { channel = channel, character = target, length = 600 }
    end)
end

function COATest()
    ChannelTest("COA", OwnerOnlyMatrix, function(channel, target)
        return { channel = channel, character = target }
    end)
end

function CORTest()
    ChannelTest("COR", OwnerOnlyMatrix, function(channel, target)
        return { channel = channel, character = "cop" }
    end)
end

function CSOTest()
    ChannelTest("CSO", OwnerOnlyMatrix, function(channel, target)
        return { channel = channel, character = target }
    end)
end

function RMOTest()
    ChannelTest("RMO", OwnerOnlyMatrix, function(channel, target)
        return { channel = channel, mode = "both" }
    end)
end

function RSTTest()
    ChannelTest("RST", PrivateOwnerOnlyMatrix, function(channel, target)
        return { channel = channel, status = "public" }
    end)
end

tests = {
    CBUTest,
    CKUTest,
    CTUTest,
    COATest,
    CORTest,
    CSOTest,
    RMOTest,
    RSTTest
}

function runTests()
    print("Starting tests")
    for _, v in ipairs(tests) do
        globalSetUp()
        v()
        globalTearDown()
        print("Test passed")
    end
    print("Done with tests")
end
