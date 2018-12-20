/*
 * Copyright (c) 2011-2018, "Kira"
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "precompiled_headers.hpp"
#include "lua_testing.hpp"
#include "connection.hpp"
#include "lua_constants.hpp"
#include "server_state.hpp"
#include "server.hpp"

#define LUATESTING_MODULE_NAME "testing"

static const luaL_Reg luatesting_funcs[] = {
        {"assert",           LuaTesting::luaAssert},
        {"runTests",         LuaTesting::runTests},
        {"createConnection", LuaTesting::createConnection},
        {"removeConnection", LuaTesting::removeConnection},
        {"killChannel",      LuaTesting::killChannel},
        {NULL, NULL}
};

int LuaTesting::openTestingLib(lua_State* L) {
    luaL_register(L, LUATESTING_MODULE_NAME, luatesting_funcs);
    return 0;
}

int LuaTesting::luaAssert(lua_State* L) {
    luaL_checkany(L, 2);
    string left, right;
    if(lua_type(L, 1) == LUA_TBOOLEAN)
        left = lua_toboolean(L, 1) ? "true" : "false";
    else
        left = lua_tostring(L, 1);
    if(lua_type(L, 2) == LUA_TBOOLEAN)
        right = lua_toboolean(L, 2) ? "true" : "false";
    else
        right = lua_tostring(L, 2);

    int equal = lua_equal(L, 1, 2);
    if (equal != 1) {
        DLOG(INFO) << "Assert failed: Left " << left << " Right: " << right;
        return luaL_error(L, "Assert failed! Left: %s Right: %s", left.data(), right.data());
    }

    lua_pop(L, 2);
    return 0;
}

int LuaTesting::runTests(lua_State* L) {
    int ret = luaL_dofile(L, "./script/tests.lua");
    if (ret) {
        LOG(WARNING) << "Failed to run tests: " << lua_tostring(L, -1);
        return luaL_error(L, "Failed to run tests file.");
    }
    return 0;
}

int LuaTesting::createConnection(lua_State* L) {
    luaL_checkany(L, 1);

    string charName = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    auto con = new ConnectionInstance();
    con->characterName = charName;
    con->characterNameLower = charName;
    con->identified = true;
    con->accountID = 1;
    con->characterID = 2;
    con->closed = true;
    ServerState::addConnection(charName, con);

    lua_pushlightuserdata(L, con);
    return 1;
}

int LuaTesting::removeConnection(lua_State* L) {
    luaL_checkany(L, 1);

    string charName = luaL_checkstring(L, 1);
    lua_pop(L, 1);
    ServerState::removeConnection(charName);

    return 0;
}

int LuaTesting::killChannel(lua_State* L) {
    luaL_checkany(L, 1);

    string chanName = luaL_checkstring(L, 1);
    lua_pop(L, 1);
    ChannelPtr chan = ServerState::getChannel(chanName);
    if (chan) {
        const chconlist_t particpants = chan->getParticipants();
        for (chconlist_t::const_iterator i = particpants.begin(); i != particpants.end(); ++i) {
            chan->part((*i).get());
        }
        ServerState::removeChannel(chanName);
    }

    return 0;
}