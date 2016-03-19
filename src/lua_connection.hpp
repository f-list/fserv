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

#ifndef LUA_CONNECTION_H
#define LUA_CONNECTION_H

#include "flua.hpp"
#include "ferror.hpp"
#include "fjson.hpp"

class LuaConnection {
public:
    static int openConnectionLib(lua_State* L);

    static int getConnection(lua_State* L);
    static int getIPCount(lua_State* L);
    static int getByAccount(lua_State* L);
    static int getByAccountID(lua_State* L);
    static int getName(lua_State* L);
    static int getChannels(lua_State* L);

    static int send(lua_State* L);
    static int sendRaw(lua_State* L);
    static int sendError(lua_State* L);

    static int close(lua_State* L);
    static int closef(lua_State* L);

    static int setIdent(lua_State* L);

    static int setAccountID(lua_State* L);

    static int setAdmin(lua_State* L);
    static int isAdmin(lua_State* L);

    static int setGlobalModerator(lua_State* L);
    static int isGlobalModerator(lua_State* L);

    static int isChannelOperator(lua_State* L);

    static int setFriends(lua_State* L);
    static int removeFriend(lua_State* L);
    static int getFriends(lua_State* L);

    static int setIgnores(lua_State* L);
    static int addIgnore(lua_State* L);
    static int removeIgnore(lua_State* L);
    static int getIgnores(lua_State* L);

    static int setKinks(lua_State* L);
    static int getKinks(lua_State* L);

    static int setGender(lua_State* L);
    static int getGender(lua_State* L);

    static int setCustomKinks(lua_State* L);
    static int getCustomKinks(lua_State* L);

    static int setInfoTags(lua_State* L);
    static int getInfoTags(lua_State* L);

    static int setStatus(lua_State* L);
    static int getStatus(lua_State* L);

    static int setMiscData(lua_State* L);
    static int getMiscData(lua_State* L);

    static int checkUpdateTimer(lua_State* L);

private:

    LuaConnection() { }

    ~LuaConnection() { }
};

#endif //LUA_CONNECTION_H
