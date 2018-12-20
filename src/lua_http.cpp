/*
 * Copyright (c) 2011-2013, "Kira"
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


#include "lua_http.hpp"
#include "http_client.hpp"

#define LUAHTTP_MODULE_NAME "http"

static const luaL_Reg luahttp_funcs[] = {
        {"get",    LuaHTTP::getRequest},
        {"post",   LuaHTTP::postRequest},
        {"escape", LuaHTTP::escapeString},
        {NULL, NULL}
};

int LuaHTTP::openHTTPLib(lua_State* L) {
    luaL_register(L, LUAHTTP_MODULE_NAME, luahttp_funcs);
    return 0;
}

static void tableToMap(lua_State* L, int index, httpStringMap &target) {
    string key, value;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "Table must contain only string keys and values.");
            return;
        }
        key = lua_tostring(L, -2);
        value = lua_tostring(L, -1);
        target[key] = value;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/**
 * Makes a GET http request.
 * @param string callback
 * @param string url
 * @param LUD|nil connection
 * @param table extras
 * @return
 */
int LuaHTTP::getRequest(lua_State* L) {
    luaL_checkany(L, 4);

    string callback = luaL_checkstring(L, 1);
    string url = luaL_checkstring(L, 2);

    LBase* base = nullptr;
    ConnectionInstance* con = nullptr;
    if(lua_type(L, 3) == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 4, temp_con);
        con = temp_con;
    }
    httpStringMap extras;
    if(lua_type(L, 4) == LUA_TTABLE) {
        tableToMap(L, 4, extras);
    }

    HTTPRequest* request = new HTTPRequest();
    request->method("GET");
    request->url(url);
    request->callbackName(callback);
    request->extras(extras);
    if(con)
        request->connection(con);

    if(!HTTPClient::addRequest(request))
        delete request;
    return 0;
}

/**
 * Sends off a POST http request.
 * @param string callback
 * @param string url
 * @param table POST params
 * @param LUD|nil connection
 * @param table extras
 * @return
 */
int LuaHTTP::postRequest(lua_State* L) {
    luaL_checkany(L, 5);

    string callback = luaL_checkstring(L, 1);
    string url = luaL_checkstring(L, 2);

    LBase* base = nullptr;
    ConnectionInstance* con = nullptr;
    int arg_type = lua_type(L, 4);
    if (arg_type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 4, temp_con);
        con = temp_con;
    }
    httpStringMap postData;

    arg_type = lua_type(L, 3);
    if (arg_type != LUA_TTABLE) {
        luaL_error(L, "POST data must be a table of strings");
    }
    tableToMap(L, 3, postData);

    httpStringMap extras;
    if (lua_type(L, 5) == LUA_TTABLE) {
        tableToMap(L, 5, extras);
    }

    HTTPRequest* request = new HTTPRequest();
    request->callbackName(callback);
    request->url(url);
    request->method("POST");
    request->extras(extras);
    if (con)
        request->connection(con);
    for (auto itr = postData.begin(); itr != postData.end(); itr++) {
        request->postField(itr->first, itr->second);
    }

    if(!HTTPClient::addRequest(request))
        delete request;
    return 0;
}

int LuaHTTP::escapeString(lua_State* L) {
    luaL_checkany(L, 1);

    string input = luaL_checkstring(L, 1);
    HTTPClient::escapeSegment(input);
    lua_pushstring(L, input.c_str());

    return 1;
}