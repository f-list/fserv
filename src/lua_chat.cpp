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

/**
 * @file lua_chat.cpp
 * @author Kira
 */

#include "precompiled_headers.hpp"
#include "lua_chat.hpp"
#include "server_state.hpp"
#include "unicode_tools.hpp"
#include "startup_config.hpp"
#include "server.hpp"
#include "logger_thread.hpp"
#include "status.hpp"
#include <time.h>
#include <stdio.h>
#include <string>

using std::string;

#define LUACHAT_MODULE_NAME "s"

static const luaL_Reg luachat_funcs[] = {
        {"broadcast",             LuaChat::broadcast},
        {"broadcastRaw",          LuaChat::broadcastRaw},
        {"broadcastOps",          LuaChat::broadcastOps},
        {"broadcastStaffCall",    LuaChat::broadcastStaffCall},
        {"getConfigBool",         LuaChat::getConfigBool},
        {"getConfigDouble",       LuaChat::getConfigDouble},
        {"getConfigString",       LuaChat::getConfigString},
        {"getTime",               LuaChat::getTime},
        {"getUserCount",          LuaChat::getUserCount},
        {"sendUserList",          LuaChat::sendUserList},
        {"getOpList",             LuaChat::getOpList},
        {"isOp",                  LuaChat::isOp},
        {"addOp",                 LuaChat::addOp},
        {"removeOp",              LuaChat::removeOp},
        {"addSCop",               LuaChat::addSCop},
        {"removeSCop",            LuaChat::removeSCop},
        {"addBan",                LuaChat::addBan},
        {"removeBan",             LuaChat::removeBan},
        {"isBanned",              LuaChat::isBanned},
        {"addTimeout",            LuaChat::addTimeout},
        {"removeTimeout",         LuaChat::removeTimeout},
        {"isTimedOut",            LuaChat::isTimedOut},
        {"addAltWatch",           LuaChat::addAltWatch},
        {"getAltWatch",           LuaChat::getAltWatch},
        {"addStaffCall",          LuaChat::addStaffCall},
        {"removeStaffCall",       LuaChat::removeStaffCall},
        {"getStaffCall",          LuaChat::getStaffCall},
        {"sendStaffCalls",        LuaChat::sendStaffCalls},
        {"addToStaffCallTargets", LuaChat::addToStaffCallTargets},
        {"isChanOp",              LuaChat::isChanOp},
        {"escapeHTML",            LuaChat::escapeHTML},
        {"reload",                LuaChat::reload},
        {"logMessage",            LuaChat::logMessage},
        //{"shutdown", LuaChat::shutdown},
        {"getStats",        LuaChat::getStats},
        {"logAction",       LuaChat::logAction},
        {"timeUpdate",      LuaChat::timeUpdate},
        {"toJSON",          LuaChat::toJsonString},
        {"fromJSON",        LuaChat::fromJsonString},
        {NULL, NULL}
};

int LuaChat::openChatLib(lua_State* L) {
    luaL_register(L, LUACHAT_MODULE_NAME, luachat_funcs);
    return 0;
}

int LuaChat::timeUpdate(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = nullptr;
    GETLCON(base, L, 1, con);
    bool disconnect = lua_toboolean(L, 2);
    lua_pop(L, 2);
    StatusClient::instance()->sendStatusTimeUpdate(con, disconnect);
    return 0;
}

/**
 * Adds a message to be logged to persisted storage.
 * @param string type
 * @param LUD connection from connection
 * @param LUD channel
 * @param LUD|string to_character target connection
 * @params string body
 * @return
 */
int LuaChat::logMessage(lua_State* L) {
    luaL_checkany(L, 5);
    if (Server::logger() == nullptr) {
        lua_pop(L, 5);
        return 0;
    }
    string type = luaL_checkstring(L, 1);

    Channel* to_channel = 0;
    string to_character_string;
    ConnectionPtr to_connection(0);

    LBase* base = 0;
    GETLCON(base, L, 2, from_connection);
    if (lua_type(L, 3) != LUA_TNIL) {
        GETLCHAN(base, L, 3, channel);
        to_channel = channel;
    }
    int arg_type = lua_type(L, 4);
    if (arg_type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 4, to_connection_temp);
        to_connection = to_connection_temp;
    } else if (arg_type == LUA_TSTRING) {
        to_character_string = luaL_checkstring(L, 4);
    } else if (arg_type != LUA_TNIL) {
        luaL_error(L, "Character target needs to be connection or string or nil.");
    }
    string body;
    if (lua_type(L, 5) != LUA_TNIL) {
        body = luaL_checkstring(L, 5);
    }
    lua_pop(L, 5);

    auto entry = new LogEntry();
    struct timeval tv{};
    gettimeofday(&tv, NULL);
    entry->time = (unsigned long long) (tv.tv_sec) * 1000 + (unsigned long long) (tv.tv_usec) / 1000;
    entry->messageType = type;
    entry->fromAccountID = from_connection->accountID;
    entry->fromCharacterID = from_connection->characterID;
    entry->fromCharacter = from_connection->characterName;
    if (to_channel) {
        entry->toChannel = to_channel->getName();
        entry->toChannelTitle = to_channel->getTitle();
    }
    if (to_connection) {
        entry->toAccountID = to_connection->accountID;
        entry->toCharacterID = to_connection->characterID;
        entry->toCharacter = to_connection->characterName;
    } else if (to_character_string.length()) {
        entry->toCharacter = to_character_string;
    }
    if (body.length()) {
        entry->messageBody = body;
    }
    Server::logger()->addLogEntry(entry);
    Server::logger()->sendWakeup();

    return 0;
}

/**
 * Sends a json message to everyone connected to the server.
 * @param string message prefix
 * @param table json
 * @returns Nothing.
 */
int LuaChat::broadcast(lua_State* L) {
    luaL_checkany(L, 2);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "broadcast requires a table as the second argument.");

    string message = luaL_checkstring(L, 1);
    message += " ";
    json_t* json = luaToJson(L);
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    lua_pop(L, 2);
    MessagePtr outMessage(MessageBuffer::fromString(message));
    const conptrmap_t conmap = ServerState::getConnections();
    for (conptrmap_t::const_iterator i = conmap.begin(); i != conmap.end(); ++i) {
        ((*i).second)->send(outMessage);
    }
    return 0;
}

/**
 * Sends a raw message to everyone connected to the server.
 * @param string message
 * @returns Nothing.
 */
int LuaChat::broadcastRaw(lua_State* L) {
    luaL_checkany(L, 1);
    string message = luaL_checkstring(L, 1);
    lua_pop(L, 1);
    MessagePtr outMessage(MessageBuffer::fromString(message));
    const conptrmap_t conmap = ServerState::getConnections();
    for (conptrmap_t::const_iterator i = conmap.begin(); i != conmap.end(); ++i) {
        ((*i).second)->send(outMessage);
    }
    return 0;
}

int LuaChat::broadcastOps(lua_State* L) {
    luaL_checkany(L, 2);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "broadcastOps requires a table as the second argument.");

    string message = luaL_checkstring(L, 1);
    message += " ";
    json_t* json = luaToJson(L);
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    lua_pop(L, 2);
    MessagePtr outMessage(MessageBuffer::fromString(message));
    const oplist_t ops = ServerState::getOpList();
    for (oplist_t::const_iterator i = ops.begin(); i != ops.end(); ++i) {
        string name(*i);
        size_t length = name.length();
        for (size_t x = 0; x < length; ++x) {
            name[x] = tolower(name[x]);
        }
        ConnectionPtr con = ServerState::getConnection(name);
        if (con != 0) {
            con->send(outMessage);
        }
    }
    return 0;
}

int LuaChat::broadcastStaffCall(lua_State* L) {
    luaL_checkany(L, 2);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "broadcastStaffCall requires a table as the second argument.");

    string message = luaL_checkstring(L, 1);
    message += " ";
    json_t* json = luaToJson(L);
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    lua_pop(L, 2);
    MessagePtr outMessage(MessageBuffer::fromString(message));
    auto targets = ServerState::getStaffCallTargets();
    for (auto i = targets.begin(); i != targets.end(); ++i) {
        ConnectionPtr con(*i);
        if (con != 0) {
            con->send(outMessage);
        }
    }
    return 0;
}

int LuaChat::addToStaffCallTargets(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = nullptr;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    ServerState::addStaffCallTarget(con);

    return 0;
}

/**
 * Returns the value for config key, or nil if not found.
 * @param string key
 * @returns [bool]/nil value.
 */
int LuaChat::getConfigBool(lua_State* L) {
    luaL_checkany(L, 1);
    const char* key = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    lua_pushboolean(L, StartupConfig::getBool(key));
    return 1;
}

/**
 * Returns the value for config key, or nil if not found.
 * @param string key
 * @returns [number]/nil value.
 */
int LuaChat::getConfigDouble(lua_State* L) {
    luaL_checkany(L, 1);
    const char* key = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    lua_pushnumber(L, StartupConfig::getDouble(key));
    return 1;
}

/**
 * Returns the value for config key, or nil if not found.
 * @param string key
 * @returns [string]/nil value.
 */
int LuaChat::getConfigString(lua_State* L) {
    luaL_checkany(L, 1);
    const char* key = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    lua_pushstring(L, StartupConfig::getString(key).c_str());
    return 1;
}

/**
 * Gets the current unix time.
 * @returns current unix time as a number.
 */
int LuaChat::getTime(lua_State* L) {
    lua_pushinteger(L, time(0));
    return 1;
}

/**
 * Gets the number of currently connected and identified characters on the server.
 * @returns [number] number of connected users.
 */
int LuaChat::getUserCount(lua_State* L) {
    lua_pushinteger(L, ServerState::getUserCount());
    return 1;
}

/**
 * Sends a connection the list of online users, split every N characters.
 * @param LUD connection
 * @param string prefix
 * @param number split
 * @returns Nothing.
 */
int LuaChat::sendUserList(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string prefix = luaL_checkstring(L, 2);
    prefix += " ";
    int split = luaL_checkinteger(L, 3);
    lua_pop(L, 3);

    conptrmap_t cons = ServerState::getConnections();
    int n = 0;
    json_t* rootnode = json_object();
    json_t* arraynode = json_array();
    for (conptrmap_t::const_iterator i = cons.begin(); i != cons.end(); ++i) {
        json_t* cha = json_array();
        json_array_append_new(cha,
                              json_string_nocheck(i->second->characterName.c_str())
        );
        json_array_append_new(cha,
                              json_string_nocheck(i->second->gender.c_str())
        );
        json_array_append_new(cha,
                              json_string_nocheck(i->second->status.c_str())
        );
        json_t* status = json_string(i->second->statusMessage.c_str());
        if (!status)
            status = json_string_nocheck("");
        json_array_append_new(cha, status);

        json_array_append_new(arraynode, cha);
        if ((++n % split) == 0) {
            json_object_set_new_nocheck(rootnode, "characters", arraynode);
            string s = prefix;
            const char* message = json_dumps(rootnode, JSON_COMPACT);
            s += message;
            MessagePtr outMessage(MessageBuffer::fromString(s));
            con->send(outMessage);
            free((void*) message);
            json_decref(rootnode);
            arraynode = json_array();
            rootnode = json_object();
        }
    }
    json_object_set_new_nocheck(rootnode, "characters", arraynode);
    string s = prefix;
    const char* message = json_dumps(rootnode, JSON_COMPACT);
    s += message;
    MessagePtr outMessage(MessageBuffer::fromString(s));
    con->send(outMessage);
    free((void*) message);
    json_decref(rootnode);
    return 0;
}

/**
 * Gets the op list.
 * @returns [table] op list
 */
int LuaChat::getOpList(lua_State* L) {
    lua_newtable(L);
    int n = 1;
    const oplist_t ops = ServerState::getOpList();
    for (oplist_t::const_iterator i = ops.begin(); i != ops.end(); ++i) {
        lua_pushstring(L, i->c_str());
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

/**
 * Checks if a name is on the global op list.
 * @param string name
 * @returns [boolean] on op list
 */
int LuaChat::isOp(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    if (ServerState::isOp(name))
        lua_pushboolean(L, true);
    else
        lua_pushboolean(L, false);
    return 1;
}

/**
 * Adds a name to the global op list.
 * @param string name
 * @returns Nothing.
 */
int LuaChat::addOp(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::addOp(name);
    ServerState::saveOps();
    return 0;
}

/**
 * Removes a name from the global op list.
 * @param string name
 * @returns Nothing.
 */
int LuaChat::removeOp(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::removeOp(name);
    ServerState::saveOps();
    return 0;
}

int LuaChat::addSCop(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::addSuperCop(name);
    ServerState::saveOps();
    return 0;
}

int LuaChat::removeSCop(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::removeSuperCop(name);
    ServerState::saveOps();
    return 0;
}

/**
 * Adds a ban.
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChat::addBan(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    ServerState::addBan(con->characterNameLower, con->accountID);
    ServerState::saveBans();
    return 0;
}

/**
 * Removes a ban.
 * @param string character name
 * @returns [boolean] If the ban could be found and removed.
 */
int LuaChat::removeBan(lua_State* L) {
    luaL_checkany(L, 1);

    string character = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    bool ret = ServerState::removeBan(character);
    ServerState::saveBans();
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Checks if a given connection is banned or not.
 * @param LUD connection
 * @returns [boolean] banned
 */
int LuaChat::isBanned(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    ret = ServerState::isBanned(con->accountID);
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Adds a timeout to an account.
 * @param LUD connection
 * @param number length in seconds
 * @returns Nothing.
 */
int LuaChat::addTimeout(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    long length = luaL_checkinteger(L, 2);
    lua_pop(L, 2);

    ServerState::addTimeout(con->characterNameLower, con->accountID, length);
    ServerState::saveBans();
    return 0;
}

/**
 * Removes the timeout for an account.
 * @param string character name
 * @returns Nothing.
 */
int LuaChat::removeTimeout(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::removeTimeout(name);
    ServerState::saveBans();
    return 0;
}

/**
 * Checks if a connection has a time out.
 * @param LUD connection
 * @returns [boolean] is timed out, [number] remaining length in seconds.
 */
int LuaChat::isTimedOut(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    int end = 0;
    ret = ServerState::isTimedOut(con->accountID, end);
    if (end < time(0)) {
        ret = false;
    }
    lua_pushboolean(L, ret);
    lua_pushinteger(L, (end - time(0)));
    return 2;
}

/**
 * Creates a new staff report.
 * @param string callid
 * @param string character
 * @param string report body
 * @param number logid(-1 if not present)
 * @returns Nothing.
 */
int LuaChat::addStaffCall(lua_State* L) {
    luaL_checkany(L, 5);

    string callid = luaL_checkstring(L, 1);
    string character = luaL_checkstring(L, 2);
    string report = luaL_checkstring(L, 3);
    long logid = luaL_checkinteger(L, 4);
    string tab = luaL_checkstring(L, 5);
    lua_pop(L, 5);

    StaffCallRecord r;
    r.callid = callid;
    r.character = character;
    r.report = report;
    r.logid = logid;
    r.action = "report";
    r.tab = tab;
    r.timestamp = time(NULL);
    ServerState::addStaffCall(callid, r);
    return 0;
}

/**
 * Removes a staff call by its call id.
 * @param string call id
 * @returns Nothing.
 */
int LuaChat::removeStaffCall(lua_State* L) {
    luaL_checkany(L, 1);

    string callid = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ServerState::removeStaffCall(callid);
    return 0;
}

int LuaChat::getStaffCall(lua_State* L) {
    luaL_checkany(L, 1);

    string callid = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    lua_newtable(L);
    StaffCallRecord r = ServerState::getStaffCall(callid);
    if (r.callid != callid) {
        lua_pop(L, 1);
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushstring(L, r.callid.c_str());
    lua_setfield(L, -2, "callid");
    lua_pushstring(L, r.character.c_str());
    lua_setfield(L, -2, "character");
    lua_pushstring(L, r.report.c_str());
    lua_setfield(L, -2, "report");
    lua_pushinteger(L, r.logid);
    lua_setfield(L, -2, "logid");
    lua_pushinteger(L, r.timestamp);
    lua_setfield(L, -2, "timestamp");
    lua_pushstring(L, r.tab.c_str());
    lua_setfield(L, -2, "tab");
    return 1;
}

/**
 * Sends all unconfirmed staff calls to a connection.
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChat::sendStaffCalls(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    const staffcallmap_t calls = ServerState::getStaffCalls();
    for (staffcallmap_t::const_iterator i = calls.begin(); i != calls.end(); ++i) {
        StaffCallRecord r = i->second;
        string message("SFC ");
        json_t* rootnode = json_object();
        json_object_set_new_nocheck(rootnode, "action",
                                    json_string_nocheck("report")
        );
        json_object_set_new_nocheck(rootnode, "callid",
                                    json_string_nocheck(r.callid.c_str())
        );
        json_object_set_new_nocheck(rootnode, "character",
                                    json_string_nocheck(r.character.c_str())
        );
        json_object_set_new_nocheck(rootnode, "old", json_true());
        json_object_set_new_nocheck(rootnode, "timestamp",
                                    json_integer(r.timestamp)
        );
        json_t* report = json_string(r.report.c_str());
        if (!report)
            report = json_string_nocheck(
                    "Report contained invalid UTF-8 and could not be encoded. Please contact the sender about this!");
        json_object_set_new_nocheck(rootnode, "report", report);
        json_t* tab = json_string(UnicodeTools::escapeHTML(r.tab).c_str());
        if (!tab)
            tab = json_string_nocheck("Invalid Tab");
        json_object_set_new_nocheck(rootnode, "tab", tab);
        if (r.logid != -1) {
            json_object_set_new_nocheck(rootnode, "logid",
                                        json_integer(r.logid)
            );
        }
        const char* sfcstring = json_dumps(rootnode, JSON_COMPACT);
        message += sfcstring;
        MessagePtr outMessage(MessageBuffer::fromString(message));
        con->send(outMessage);
        free((void*) sfcstring);
        json_decref(rootnode);
    }
    return 0;
}

int LuaChat::isChanOp(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    bool ret = false;
    ret = ServerState::isChannelOp(con->characterName);
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Escapes '<' '>' and '&' in a string and returns the string.
 *
 * This is expensive to do, as the string must be converted from UTF-8 to UTF-16/32 and back to UTF-8
 * after having escaped the characters.
 * @param string message
 * @returns [string] html escaped string.
 */
int LuaChat::escapeHTML(lua_State* L) {
    luaL_checkany(L, 1);

    string message = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    lua_pushstring(L, UnicodeTools::escapeHTML(message).c_str());
    return 1;
}

/**
 * Reloads the ops/bans/config variables.
 * @param bool save first
 * @returns Nothing
 */
int LuaChat::reload(lua_State* L) {
    luaL_checkany(L, 1);

    bool savefirst = lua_toboolean(L, 1);
    lua_pop(L, 1);

    if (savefirst) {
        ServerState::saveBans();
        ServerState::saveOps();
        ServerState::saveChannels();
    }
    ServerState::loadBans();
    ServerState::loadOps();
    StartupConfig::init();
    return 0;
}

/**
 * For future expansion. This will initiate a server shutdown.
 * @returns Nothing.
 */
int LuaChat::shutdown(lua_State* L) {
    return 0;
}

/**
 * Returns various stats about the server process.
 * @returns [number] User count, [number] Maximum user count, [number] Channel count, [number] Start time,
 * [number] Current time, [number] Total accepted connections, [string/nil] Server start time string.
 */
int LuaChat::getStats(lua_State* L) {
    lua_pushinteger(L, ServerState::getUserCount());
    lua_pushinteger(L, ServerState::getMaxUserCount());
    lua_pushinteger(L, ServerState::getChannelCount());
    lua_pushinteger(L, Server::getStartTime());
    lua_pushinteger(L, time(NULL));
    lua_pushinteger(L, Server::getAcceptedConnections());
    char buffer[200];
    bzero(&buffer, sizeof(buffer));
    time_t t = (time_t) Server::getStartTime();
    struct tm* tmp = gmtime(&t);
    if (tmp == 0) {
        lua_pushnil(L);
    }
    if (strftime(&buffer[0], sizeof(buffer), "%a, %d %b %Y %T %z", tmp) == 0) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, &buffer[0]);
    }
    return 7;
}

/**
 * Logs an action to the action log.
 * @param LUD connection
 * @param string message type
 * @param table arguments
 * @returns Nothing.
 */
int LuaChat::logAction(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string type = luaL_checkstring(L, 2);
    json_t* args = luaToJson(L);
    lua_pop(L, 3);
    json_t* root = json_object();
    json_object_set_new_nocheck(root, "args", args);
    json_object_set_new_nocheck(root, "name",
                                json_string_nocheck(con->characterName.c_str())
    );
    json_object_set_new_nocheck(root, "type",
                                json_string_nocheck(type.c_str())
    );
    json_object_set_new_nocheck(root, "time",
                                json_integer(time(0))
    );
    const char* logstr = json_dumps(root, JSON_COMPACT);
    string message(logstr);
    free((void*) logstr);
    json_decref(root);

    char buffer[255];
    bzero(&buffer[0], sizeof(buffer));
    snprintf(&buffer[0], sizeof(buffer), "./oplogs/%s.%d.log", type.c_str(), (int) time(NULL));
    if (!ServerState::fsaveFile(&buffer[0], message))
        LOG(WARNING) << "Failed to save log message with contents " << message;

    RedisRequest* req = new RedisRequest;
    req->key = "chat.adminlog";
    req->method = REDIS_LPUSH;
    req->values.push(message);
    Redis::addRequest(req);
    return 0;
}

/**
 * Converts a Lua table to a json string.
 * @param table json
 * @returns string containing the encoded json, or an empty string when something goes wrong.
 */
int LuaChat::toJsonString(lua_State* L) {
    luaL_checkany(L, 1);

    if (lua_type(L, 1) != LUA_TTABLE)
        return luaL_error(L, "tojson expects a table.");

    json_t* n = luaToJson(L);
    lua_pop(L, 1);

    const char* jsonvalue = json_dumps(n, JSON_COMPACT);
    lua_pushstring(L, jsonvalue);
    free((void*) jsonvalue);
    json_decref(n);
    return 1;
}

/**
 * Converts a json string to a Lua table.
 * @param string json
 * @returns lua table containing the contents of the json string.
 */
int LuaChat::fromJsonString(lua_State* L) {
    luaL_checkany(L, 1);
    string message = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    json_t* n = json_loads(message.c_str(), 0, 0);
    if (!n)
        n = json_object();
    jsonToLua(L, n);
    json_decref(n);
    return 1;
}

/**
 * Converts a Lua table to a json node, leaving the table on the stack.
 *
 * @warning NOTE This functions expects that there is a table at the top of the stack ready to be parsed.
 * It ALSO expects that any table that needs to be expressed as an array in JSON, has the prefix of
 * "array_" as the name. Converting tables with numbers as keys is NOT supported outside of JSON
 * arrays. The table is NOT popped from the stack.
 */
json_t* LuaChat::luaToJson(lua_State* L) {
    json_t* ret = json_object();
    // If there is not a table at the top of the stack calling lua_next will CRASH!
    if (lua_type(L, -1) != LUA_TTABLE)
        return ret;
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        string key;
        json_t* item = l2jParseItem(L, key);
        json_object_set_new(ret, key.c_str(), item);
    }
    return ret;
}

json_t* LuaChat::l2jParseItem(lua_State* L, string &key) {
    lua_checkstack(L, 20);
    json_t* n = 0;
    if (lua_type(L, -2) == LUA_TSTRING)
        key = lua_tostring(L, -2);

    int type = lua_type(L, -1);
    switch (type) {
        case LUA_TBOOLEAN:
            n = (bool) lua_toboolean(L, -1) ? json_true() : json_false();
            break;
        case LUA_TNUMBER:
            n = json_real(lua_tonumber(L, -1));
            break;
        case LUA_TSTRING:
            n = json_string(lua_tostring(L, -1));
            if (!n)
                n = json_string_nocheck("");
            break;
        case LUA_TTABLE: {
            bool is_array = false;
            if (key.find("array_") == 0) {
                is_array = true;
                key = key.substr(6).c_str();
                n = json_array();
            } else {
                n = json_object();
            }
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                string itemkey;
                json_t* item = l2jParseItem(L, itemkey);
                if (is_array) {
                    json_array_append_new(n, item);
                } else {
                    json_object_set_new(n, itemkey.c_str(), item);
                }
            }
            break;
        }
        case LUA_TNIL:
        default:
            n = json_null();
            break;
    }
    lua_pop(L, 1);
    return n;
}

/**
 * Converts a json node to a Lua table and leaves it at the top of the stack.
 * @warning NOTE This function creates a new table, fills it with the contents from json, and returns it on the top of the stack.
 * JSON arrays are given the named prefix of "array_" so that converting them back to JSON works correctly, and so that
 * the type of the original item is known.
 */
void LuaChat::jsonToLua(lua_State* L, json_t* json) {
    lua_newtable(L);
    const char* key;
    json_t* value;

    json_object_foreach(json, key, value)
    {
        j2lParseItem(L, key, value);
    }
}

void LuaChat::j2lParseItem(lua_State* L, const char* key, json_t* json, int index) {
    lua_checkstack(L, 20);
    switch (json_typeof(json)) {
        case JSON_TRUE: {
            lua_pushboolean(L, 1);
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_FALSE: {
            lua_pushboolean(L, 0);
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_REAL: {
            lua_pushnumber(L, json_real_value(json));
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_INTEGER: {
            lua_pushinteger(L, json_integer_value(json));
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_STRING: {
            lua_pushstring(L, json_string_value(json));
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_ARRAY: {
            lua_newtable(L);
            size_t len = json_array_size(json);
            for (size_t i = 0; i < len; ++i) {
                json_t* item = json_array_get(json, i);
                j2lParseItem(L, 0, item, i);
            }
            if (index == -1) {
                string newname = "array_";
                newname += key;
                lua_setfield(L, -2, newname.c_str());
            } else
                lua_rawseti(L, -2, index);
            break;
        }
        case JSON_OBJECT: {
            lua_newtable(L);
            const char* itemkey;
            json_t* itemvalue;

            json_object_foreach(json, itemkey, itemvalue)
            {
                j2lParseItem(L, itemkey, itemvalue);
            }
            if (index == -1)
                lua_setfield(L, -2, key);
            else
                lua_rawseti(L, -2, index);
            break;
        }
        default:
            break;
    }
}
