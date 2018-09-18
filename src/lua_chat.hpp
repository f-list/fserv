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

#ifndef LUA_CHAT_H
#define LUA_CHAT_H

#include "flua.hpp"
#include "ferror.hpp"
#include "fjson.hpp"
#include <string>

using std::string;

class LuaChat {
public:

    static int openChatLib(lua_State* L);

    static int broadcast(lua_State* L);
    static int broadcastRaw(lua_State* L);
    static int broadcastOps(lua_State* L);

    static int getConfigBool(lua_State* L);
    //static int setConfigBool(lua_State* L);
    static int getConfigDouble(lua_State* L);
    //static int setConfigNumber(lua_State* L);
    static int getConfigString(lua_State* L);
    //static int setConfigString(lua_State* L);

    static int getTime(lua_State* L);
    static int getUserCount(lua_State* L);

    static int sendUserList(lua_State* L);

    static int getOpList(lua_State* L);
    static int isOp(lua_State* L);
    static int addOp(lua_State* L);
    static int removeOp(lua_State* L);

    static int addSCop(lua_State* L);
    static int removeSCop(lua_State* L);

    static int addBan(lua_State* L);
    static int removeBan(lua_State* L);
    static int isBanned(lua_State* L);

    static int addTimeout(lua_State* L);
    static int removeTimeout(lua_State* L);
    static int isTimedOut(lua_State* L);

    static int addStaffCall(lua_State* L);
    static int removeStaffCall(lua_State* L);
    static int getStaffCall(lua_State* L);
    static int sendStaffCalls(lua_State* L);
    static int broadcastStaffCall(lua_State* L);

    static int addToStaffCallTargets(lua_State* L);

    static int isChanOp(lua_State* L);

    static int escapeHTML(lua_State* L);

    static int reload(lua_State* L);
    static int shutdown(lua_State* L);

    static int getStats(lua_State* L);

    static int logAction(lua_State* L);

    static int toJsonString(lua_State* L);
    static int fromJsonString(lua_State* L);

    static int logMessage(lua_State* L);

    static int timeUpdate(lua_State* L);
    static int subUpdate(lua_State* L);
    static int forcedSubUpdate(lua_State* L);

    static json_t* luaToJson(lua_State* L);
    static void jsonToLua(lua_State* L, json_t* json);
private:
    static json_t* l2jParseItem(lua_State* L, string& key);
    static void j2lParseItem(lua_State* L, const char* key, json_t* json, int index = -1);
};

#endif //LUA_CHAT_H
