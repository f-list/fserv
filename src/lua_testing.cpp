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
        {"assert",           LuaTesting::assert},
        {"runTests",         LuaTesting::runTests},
        {"createConnection", LuaTesting::createConnection},
        {"removeConnection", LuaTesting::removeConnection},
        {NULL, NULL}
};

int LuaTesting::openTestingLib(lua_State* L) {
    luaL_register(L, LUATESTING_MODULE_NAME, luatesting_funcs);
    return 0;
}

int LuaTesting::assert(lua_State* L) {
    luaL_checkany(L, 2);
    auto equal = lua_equal(L, 1, 2);
    if(equal != 1)
        return luaL_error(L, "Assert failed! Left: %s Right: %s", 1, 2);
    lua_pop(L, 2);
    return 0;
}

int LuaTesting::runTests(lua_State* L) {
    int ret = luaL_dofile(sL, "./script/tests.lua");
    if(ret) {
        LOG(WARNING) << "Failed to run tests: " << lua_tostring(L, -1);
        return luaL_error(L, "Failed to run tests file.");
    }
    return 0;
}

int LuaTesting::createConnection(lua_State* L) {
    return 0;
}

int LuaTesting::removeConnection(lua_State* L) {
    return 0;
}