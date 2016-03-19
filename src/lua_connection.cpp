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
 * @file lua_connection.cpp
 * @author Kira
 */

#include "precompiled_headers.hpp"
#include "lua_connection.hpp"
#include "connection.hpp"
#include "lua_chat.hpp"
#include "lua_constants.hpp"
#include "server_state.hpp"
#include "server.hpp"

#define LUACONNECTION_MODULE_NAME "u"

static const luaL_Reg luaconnection_funcs[] = {
    {"getConnection", LuaConnection::getConnection},
    {"getIPCount", LuaConnection::getIPCount},
    {"getByAccount", LuaConnection::getByAccount},
    {"getByAccountID", LuaConnection::getByAccountID},
    {"getName", LuaConnection::getName},
    {"getChannels", LuaConnection::getChannels},
    {"send", LuaConnection::send},
    {"sendRaw", LuaConnection::sendRaw},
    {"sendError", LuaConnection::sendError},
    {"close", LuaConnection::close},
    {"closef", LuaConnection::closef},
    {"setIdent", LuaConnection::setIdent},
    {"setAccountID", LuaConnection::setAccountID},
    {"setAdmin", LuaConnection::setAdmin},
    {"isAdmin", LuaConnection::isAdmin},
    {"setGlobMod", LuaConnection::setGlobalModerator},
    {"isGlobMod", LuaConnection::isGlobalModerator},
    {"isChanOp", LuaConnection::isChannelOperator},
    {"setFriends", LuaConnection::setFriends},
    {"removeFriend", LuaConnection::removeFriend},
    {"getFriendList", LuaConnection::getFriends},
    {"setIgnores", LuaConnection::setIgnores},
    {"addIgnore", LuaConnection::addIgnore},
    {"removeIgnore", LuaConnection::removeIgnore},
    {"getIgnoreList", LuaConnection::getIgnores},
    {"setKinks", LuaConnection::setKinks},
    {"getKinks", LuaConnection::getKinks},
    {"setCustomKinks", LuaConnection::setCustomKinks},
    {"getCustomKinks", LuaConnection::getCustomKinks},
    {"setInfoTags", LuaConnection::setInfoTags},
    {"getInfoTags", LuaConnection::getInfoTags},
    {"setGender", LuaConnection::setGender},
    {"getGender", LuaConnection::getGender},
    {"setStatus", LuaConnection::setStatus},
    {"getStatus", LuaConnection::getStatus},
    {"setMiscData", LuaConnection::setMiscData},
    {"getMiscData", LuaConnection::getMiscData},
    {"checkUpdateTimer", LuaConnection::checkUpdateTimer},
    {NULL, NULL}
};

int LuaConnection::openConnectionLib(lua_State* L) {
    luaL_register(L, LUACONNECTION_MODULE_NAME, luaconnection_funcs);
    return 0;
}

/**
 * Returns a connection object for a named connection.
 * @param string name
 * @returns [LUD] connection object
 */
int LuaConnection::getConnection(lua_State* L) {
    luaL_checkany(L, 1);
    string conname = luaL_checkstring(L, 1);
    lua_pop(L, 1);
    ConnectionPtr con = ServerState::getConnection(conname);
    if (con == 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    lua_pushlightuserdata(L, con.get());
    return 2;
}

/**
 * Gets the number of connections from the ip associated with a connection.
 * @param LUD connection
 * @returns [number] number of connections on the same ip address.
 */
int LuaConnection::getIPCount(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushinteger(L, ServerState::getConnectionIPCount(con));
    return 1;
}

/**
 * Returns a list of connections that have the same account as the provided connection.
 * @param LUD connection
 * @returns [table of LUD] connections with same account, including provided.
 */
int LuaConnection::getByAccount(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_newtable(L);
    int n = 1;
    const conptrmap_t cons = ServerState::getConnections();
    for (conptrmap_t::const_iterator i = cons.begin(); i != cons.end(); ++i) {
        if (i->second->accountID == con->accountID) {
            lua_pushlightuserdata(L, i->second.get());
            lua_rawseti(L, -2, n++);
        }
    }

    return 1;
}

/**
 * Returns a list of connections that have the same account as the provided connection.
 * @param LUD connection
 * @returns [table of LUD] connections with same account, including provided.
 */
int LuaConnection::getByAccountID(lua_State* L) {
    luaL_checkany(L, 1);

    long accountid = (long) luaL_checkinteger(L, 1);
    lua_pop(L, 1);

    lua_newtable(L);
    int n = 1;
    const conptrmap_t cons = ServerState::getConnections();
    for (conptrmap_t::const_iterator i = cons.begin(); i != cons.end(); ++i) {
        if (i->second->accountID == accountid) {
            lua_pushlightuserdata(L, i->second.get());
            lua_rawseti(L, -2, n++);
        }
    }

    return 1;
}

/**
 * Returns the character name associated with a connection.
 * @param LUD connection
 * @returns [string] character name.
 */
int LuaConnection::getName(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushstring(L, con->characterName.c_str());
    return 1;
}

/**
 * Returns a list of joined channels associated with a connection.
 * @param LUD connection
 * @returns [table of LUD] list of channels
 */
int LuaConnection::getChannels(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_newtable(L);
    int i = 1;
    for (chanlist_t::const_iterator itr = con->channelList.begin(); itr != con->channelList.end(); ++itr) {
        lua_pushlightuserdata(L, (*itr).get());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

/**
 * Sends a json message to a connection.
 * @param LUD connection
 * @param string prefix
 * @param table json
 * @returns Nothing.
 */
int LuaConnection::send(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string message = luaL_checkstring(L, 2);
    message += " ";
    json_t* json = LuaChat::luaToJson(L);
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    lua_pop(L, 3);
    
    MessagePtr outMessage(MessageBuffer::fromString(message));
    con->send(outMessage);
    return 0;
}

/**
 * Sends a raw message to a connection.
 * @param LUD connection
 * @param string message
 * @returns Nothing.
 */
int LuaConnection::sendRaw(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string message = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    MessagePtr outMessage(MessageBuffer::fromString(message));
    con->send(outMessage);
    return 0;
}

/**
 * Sends an error to a connection, with optional customized message.
 * @warning Sending errors to the connection that originates an event is done through the return value. Use this only if you need to send
 * a message to another connection, send more than one error, or customize the error sent(in which case return that the event had no error
 * or multiple errors will be sent.)
 * @param LUD connection
 * @param number error code
 * @param string? message
 * @returns Nothing.
 */
int LuaConnection::sendError(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    int code = luaL_checknumber(L, 2);
    if (lua_gettop(L) == 3) {
        string message = luaL_checkstring(L, 3);
        lua_pop(L, 3);
        con->sendError(code, message);
    } else {
        con->sendError(code);
        lua_pop(L, 2);
    }
    return 0;
}

/**
 * Closes the connection.
 * @warning This does not generate any type of error or notification to the recipient.
 * Connections closed by this stop reading new data from the connection, but continue to send data until the queue is empty.
 * The connection is then closed 1(one) second after.
 * @param LUD connection
 * @returns Nothing.
 */
int LuaConnection::close(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    con->setDelayClose();
    return 0;
}

/**
 * Closes the connection and allow for another under the same character name.
 * @warning This does not generate any type of error or notification to the recipient.
 * Connections closed by this stop reading new data from the connection, but continue to send data until the queue is empty.
 * The connection is then closed 1(one) second after.
 * @param LUD connection
 * @returns Nothing.
 */
int LuaConnection::closef(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    con->setDelayClose();
    con->identified = false;
    ServerState::addUnidentified(con);
    ServerState::removeConnection(con->characterNameLower);
    return 0;
}

/**
 * Sets the connection as identified, and the character name.
 *
 * You may only call this function once. Once identified a connection can not be assigned a new character name.
 * @warning This function does no sanity checking. If you set two different values
 * for the proper cased name and the lower cased name, you will make the connection difficult to find by name.
 * @param LUD connection
 * @param string name
 * @param string name lowercase
 * @returns [boolean] If the identification could be set.
 */
int LuaConnection::setIdent(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string name = luaL_checkstring(L, 2);
    string lowername = luaL_checkstring(L, 3);
    lua_pop(L, 3);

    if (con->identified) {
        ret = false;
    } else {
        con->identified = true;
        con->characterName = name;
        con->characterNameLower = lowername;
        ServerState::addConnection(lowername, con);
        ServerState::removedUnidentified(con);
        ret = true;

        RedisRequest* req = new RedisRequest;
        req->key = Redis::onlineUsersKey;
        req->method = REDIS_SADD;
        req->updateContext = RCONTEXT_ONLINE;
        req->values.push(con->characterName);
        if (!Redis::addRequest(req))
            delete req;
    }
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Sets the account id for a connection.
 * @param LUD connection
 * @param int account id
 * @returns Nothing.
 */
int LuaConnection::setAccountID(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    long accountid = luaL_checkinteger(L, 2);
    lua_pop(L, 2);

    con->accountID = accountid;
    return 0;
}

/**
 * Sets the admin flag for the connection.
 * @param LUD connection
 * @param boolean admin status
 * @returns Nothing.
 */
int LuaConnection::setAdmin(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TBOOLEAN)
        return luaL_error(L, "Expected boolean for argument 2.");
    bool newflag = lua_toboolean(L, 2);
    lua_pop(L, 2);

    con->admin = newflag;
    return 0;
}

/**
 * Returns the status of the admin flag on a connection.
 * @param LUD connection
 * @returns [boolean] value of admin flag.
 */
int LuaConnection::isAdmin(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushboolean(L, con->admin);
    return 1;
}

int LuaConnection::setGlobalModerator(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TBOOLEAN)
        return luaL_error(L, "Expected boolean for argument 2.");
    bool newflag = lua_toboolean(L, 2);
    lua_pop(L, 2);

    con->globalModerator = newflag;
    return 0;
}

int LuaConnection::isGlobalModerator(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushboolean(L, con->globalModerator);
    return 1;
}

int LuaConnection::isChannelOperator(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushboolean(L, ServerState::isChannelOp(con->characterName));
    return 1;
}

int LuaConnection::setFriends(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "Expected table for argument 2.");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        con->friends.insert(lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    return 0;
}

int LuaConnection::removeFriend(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string name = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    con->friends.erase(name);
    return 0;
}

int LuaConnection::getFriends(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_newtable(L);
    const stringset_t friends = con->getFriends();
    int n = 1;
    for (stringset_t::const_iterator i = friends.begin(); i != friends.end(); ++i) {
        lua_pushstring(L, i->c_str());
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

int LuaConnection::setIgnores(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "Expected table for argument 2.");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        con->ignores.insert(lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    return 0;
}

int LuaConnection::addIgnore(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string name = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    con->ignores.insert(name);
    string redis_key;
    lua_pushinteger(L, con->accountID);
    lua_pushstring(L, ".ignores");
    lua_concat(L, 2);
    redis_key = lua_tostring(L, -1);
    lua_pop(L, 1);

    RedisRequest* req = new RedisRequest;
    req->key = redis_key;
    req->method = REDIS_DEL;
    req->updateContext = RCONTEXT_IGNORE;
    if (!Redis::addRequest(req)) {
        delete req;
        return 0;
    }

    req = new RedisRequest;
    req->key = redis_key;
    req->method = REDIS_LPUSH;
    req->updateContext = RCONTEXT_IGNORE;
    for (stringset_t::const_iterator i = con->ignores.begin(); i != con->ignores.end(); ++i) {
        req->values.push(*i);
    }
    if (!Redis::addRequest(req))
        delete req;
    return 0;
}

int LuaConnection::removeIgnore(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string name = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    con->ignores.erase(name);
    string redis_key;
    lua_pushinteger(L, con->accountID);
    lua_pushstring(L, ".ignores");
    lua_concat(L, 2);
    redis_key = lua_tostring(L, -1);
    lua_pop(L, 1);

    RedisRequest* req = new RedisRequest;
    req->key = redis_key;
    req->method = REDIS_DEL;
    req->updateContext = RCONTEXT_IGNORE;
    if (!Redis::addRequest(req)) {
        delete req;
        return 0;
    }

    req = new RedisRequest;
    req->key = redis_key;
    req->method = REDIS_LPUSH;
    req->updateContext = RCONTEXT_IGNORE;
    for (stringset_t::const_iterator i = con->ignores.begin(); i != con->ignores.end(); ++i) {
        req->values.push(*i);
    }
    if (!Redis::addRequest(req))
        delete req;
    return 0;
}

int LuaConnection::getIgnores(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_newtable(L);
    const stringset_t ignores = con->getIgnores();
    int n = 1;
    for (stringset_t::const_iterator i = ignores.begin(); i != ignores.end(); ++i) {
        lua_pushstring(L, i->c_str());
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

/**
 * Populates a connections kink list.
 * @warning No attempt to verify the uniqueness or validity of kinks is done.
 * @param LUD connection
 * @param table kinks
 * @returns Nothing.
 */
int LuaConnection::setKinks(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "Expected table for argument 2.");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        con->kinkList.insert(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    return 0;
}

/**
 * Not implemented.
 * This functions is currently not implemented because there is no known use for it.
 * @returns Throws an error.
 */
int LuaConnection::getKinks(lua_State* L) {
    return luaL_error(L, "Not implemented.");
}

/**
 * Sets the gender for a connection.
 * @param LUD connection
 * @param string gender
 * @returns Nothing.
 */
int LuaConnection::setGender(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string gender = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    con->gender = gender;
    return 0;
}

/**
 * Retrieves the gender string for a connection.
 * @param LUD connection
 * @returns [string] gender string.
 */
int LuaConnection::getGender(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushstring(L, con->gender.c_str());
    return 1;
}

/**
 * Sets the custom kinks for a connection.
 * @param LUD connection
 * @param table custon kinks table
 * @returns Nothing.
 */
int LuaConnection::setCustomKinks(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "Expected table for argument 2.");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        con->customKinkMap[luaL_checkstring(L, -2)] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    return 0;
}

/**
 * Returns a table with a users custom kinks.
 * @param LUD connection
 * @returns [table] custom kinks
 */
int LuaConnection::getCustomKinks(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    const stringmap_t kinks = con->getCustomKinks();
    lua_newtable(L);
    for (stringmap_t::const_iterator i = kinks.begin(); i != kinks.end(); ++i) {
        lua_pushstring(L, i->second.c_str());
        lua_setfield(L, -2, i->first.c_str());
    }

    return 1;
}

/**
 * Sets the info tags for a connection.
 * @param LUD connection
 * @param table info tags table
 * @returns Nothing.
 */
int LuaConnection::setInfoTags(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    if (lua_type(L, 2) != LUA_TTABLE)
        return luaL_error(L, "Expected table for argument 2.");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        con->infotagMap[luaL_checkstring(L, -2)] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    return 0;
}

/**
 * Returns a table with a users info tags.
 * @param LUD connection
 * @returns [table] info tags
 */
int LuaConnection::getInfoTags(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    const stringmap_t infotags = con->getInfoTags();
    lua_newtable(L);
    for (stringmap_t::const_iterator i = infotags.begin(); i != infotags.end(); ++i) {
        lua_pushstring(L, i->second.c_str());
        lua_setfield(L, -2, i->first.c_str());
    }

    return 1;
}

/**
 * Sets the status for a connection.
 * @param LUD connection
 * @param string status
 * @param string/nil statusmessage
 * @returns Nothing.
 */
int LuaConnection::setStatus(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string status = luaL_checkstring(L, 2);
    string statusmessage;
    bool setmessage = false;
    if (lua_type(L, 3) == LUA_TSTRING) {
        statusmessage = luaL_checkstring(L, 3);
        setmessage = true;
    }
    lua_pop(L, 3);

    con->status = status;
    if (setmessage)
        con->statusMessage = statusmessage;

    return 0;
}

/**
 * Returns the status for a connection.
 * @param LUD connection
 * @returns [string] status, [string] statusmessage
 */
int LuaConnection::getStatus(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    lua_pushstring(L, con->status.c_str());
    lua_pushstring(L, con->statusMessage.c_str());
    return 2;
}

/**
 * Sets a connections misc data value.
 * @param LUD connection
 * @param string key
 * @param string value
 * @returns Nothing.
 */
int LuaConnection::setMiscData(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string key = luaL_checkstring(L, 2);
    string data = luaL_checkstring(L, 3);
    lua_pop(L, 3);

    con->miscMap[key] = data;
    return 0;
}

/**
 * Returns a connections misc data value.
 * @param LUD connection
 * @param string key
 * @returns [nil] on failure/ [string] value for key.
 */
int LuaConnection::getMiscData(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string key = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    if (con->miscMap.find(key) != con->miscMap.end()) {
        lua_pushstring(L, con->miscMap[key].c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/**
 * Checks, and updates a connection timer.
 * @param LUD connection
 * @param string timer
 * @param number timeout length
 * @returns [boolean] true if the timer has not expired, false otherwise
 */
int LuaConnection::checkUpdateTimer(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string timer = luaL_checkstring(L, 2);
    double timeout = luaL_checknumber(L, 3);
    lua_pop(L, 3);

    double time = Server::getEventTime();
    if (con->timers[timer] > (time - timeout))
        ret = true;
    else
        con->timers[timer] = time;

    lua_pushboolean(L, ret);
    return 1;
}
