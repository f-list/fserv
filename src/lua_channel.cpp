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
 * @file lua_channel.cpp
 * @author Kira
 */

#include "precompiled_headers.hpp"
#include "channel.hpp"
#include "connection.hpp"
#include "lua_channel.hpp"
#include "server.hpp"
#include "server_state.hpp"

#include <string>
#include <stdio.h>
#include <string.h>

using std::string;

#define LUACHANNEL_MODULE_NAME "c"

static const luaL_Reg luachannel_funcs[] = {
    {"sendCHA", LuaChannel::sendCHA},
    {"sendORS", LuaChannel::sendORS},
    {"getChannel", LuaChannel::getChannel},
    {"getName", LuaChannel::getName},
    {"getUserCount", LuaChannel::getUserCount},
    {"getTopUserCount", LuaChannel::getTopUserCount},
    {"createChannel", LuaChannel::createChannel},
    {"createPrivateChannel", LuaChannel::createPrivateChannel},
    {"createSpecialPrivateChannel", LuaChannel::createSpecialPrivateChannel},
    {"destroyChannel", LuaChannel::destroyChannel},
    {"sendAll", LuaChannel::sendToAll},
    {"sendAllRaw", LuaChannel::sendToAllRaw},
    {"sendChannel", LuaChannel::sendToChannel},
    {"sendChannelRaw", LuaChannel::sendToChannelRaw},
    {"sendICH", LuaChannel::sendICH},
    {"join", LuaChannel::joinChannel},
    {"part", LuaChannel::partChannel},
    {"ban", LuaChannel::ban},
    {"timeout", LuaChannel::timeout},
    {"unban", LuaChannel::unban},
    {"isBanned", LuaChannel::isBanned},
    {"getBan", LuaChannel::getBan},
    {"getBanList", LuaChannel::getBanList},
    {"invite", LuaChannel::invite},
    {"removeInvite", LuaChannel::removeInvite},
    {"isInvited", LuaChannel::isInvited},
    {"inChannel", LuaChannel::inChannel},
    {"addMod", LuaChannel::addMod},
    {"removeMod", LuaChannel::removeMod},
    {"isMod", LuaChannel::isMod},
    {"isOwner", LuaChannel::isOwner},
    {"getModList", LuaChannel::getModList},
    {"checkUpdateTimer", LuaChannel::checkUpdateTimer},
    {"getType", LuaChannel::getType},
    {"setMode", LuaChannel::setMode},
    {"getMode", LuaChannel::getMode},
    {"setPublic", LuaChannel::setPublic},
    {"getDescription", LuaChannel::getDescription},
    {"setDescription", LuaChannel::setDescription},
    {"getTitle", LuaChannel::getTitle},
    {"setTitle", LuaChannel::setTitle},
    {"setOwner", LuaChannel::setOwner},
    {"getBottleList", LuaChannel::getBottleList},
    {"canDestroy", LuaChannel::canDestroy},
    {NULL, NULL}
};

int LuaChannel::openChannelLib(lua_State* L) {
    luaL_register(L, LUACHANNEL_MODULE_NAME, luachannel_funcs);
    return 0;
}

/**
 * Sends a connection the list of public channels.
 * @param LUD connection.
 * @returns Nothing.
 */
int LuaChannel::sendCHA(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    string message = "CHA ";
    json_t* root = json_object();
    json_t* array = json_array();
    chanptrmap_t chans = ServerState::getChannels();
    for (chanptrmap_t::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        if (i->second->getType() == CT_PUBLIC) {
            json_t* channode = json_object();
            json_object_set_new_nocheck(channode, "name",
                    json_string_nocheck(i->second->getName().c_str())
                    );
            json_object_set_new_nocheck(channode, "mode",
                    json_string_nocheck(i->second->getModeString().c_str())
                    );
            json_object_set_new_nocheck(channode, "characters",
                    json_integer(i->second->getParticipantCount())
                    );
            json_array_append_new(array, channode);
        }
    }
    json_object_set_new_nocheck(root, "channels", array);
    const char* chanstring = json_dumps(root, JSON_COMPACT);
    message += chanstring;
    MessagePtr outMessage(MessageBuffer::fromString(message));
    con->send(outMessage);
    free((void*) chanstring);
    json_decref(root);
    return 0;
}

/**
 * Sends a connection the list of private, open channels.
 * @param LUD connection.
 * @returns Nothing.
 */
int LuaChannel::sendORS(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    lua_pop(L, 1);

    string message = "ORS ";
    json_t* root = json_object();
    json_t* array = json_array();
    chanptrmap_t chans = ServerState::getChannels();
    for (chanptrmap_t::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        if (i->second->getType() == CT_PUBPRIVATE) {
            json_t* channode = json_object();
            json_object_set_new_nocheck(channode, "name",
                    json_string_nocheck(i->second->getName().c_str())
                    );
            json_t* titlenode = json_string(i->second->getTitle().c_str());
            if (!titlenode)
                titlenode = json_string("This channel had an invalid title. This is a safe default.");
            json_object_set_new_nocheck(channode, "title", titlenode);
            json_object_set_new_nocheck(channode, "characters",
                    json_integer(i->second->getParticipantCount())
                    );
            json_array_append_new(array, channode);
        }
    }
    json_object_set_new_nocheck(root, "channels", array);
    const char* chanstring = json_dumps(root, JSON_COMPACT);
    message += chanstring;
    MessagePtr outMessage(MessageBuffer::fromString(message));
    con->send(outMessage);
    free((void*) chanstring);
    json_decref(root);
    return 0;
}

/**
 * Fetches a channel object by name.
 * @param string Channel name.
 * @returns [bool] found, [LUD] channel object
 */
int LuaChannel::getChannel(lua_State* L) {
    luaL_checkany(L, 1);

    string channame = luaL_checkstring(L, 1);
    lua_pop(L, 1);
    ChannelPtr chan = ServerState::getChannel(channame);
    if (chan == 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    lua_pushlightuserdata(L, chan.get());
    return 2;
}

/**
 * Gets the name for a channel.
 * @param LUD channel
 * @returns [string] name.
 */
int LuaChannel::getName(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushstring(L, chan->getName().c_str());
    return 1;
}

/**
 * Returns the current participant count for a channel.
 * @param LUD channel
 * @returns [number] participant count.
 */
int LuaChannel::getUserCount(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushinteger(L, chan->getParticipantCount());
    return 1;
}

/**
 * Gets the maximum number of users in a public private channel.
 * @param LUD channel
 * @returns [number] maximum participant count.
 */
int LuaChannel::getTopUserCount(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushinteger(L, chan->getTopUserCount());
    return 1;
}

/**
 * Creates a new public channel with the given name.
 * @param string name
 * @returns [LUD] channel
 */
int LuaChannel::createChannel(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    Channel* chan = new Channel(name, CT_PUBLIC);
    ServerState::addChannel(name, chan);

    lua_pushlightuserdata(L, chan);
    return 1;
}

/**
 * Create a new private channel.
 * @param LUD connection
 * @returns [string] name, [LUD] channel
 */
int LuaChannel::createPrivateChannel(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCON(base, L, 1, con);
    string title = luaL_checkstring(L, 2);
    lua_pop(L, 2);
    
    string namehash = ServerState::generatePrivateChannelID(con, title);
    Channel* privchan = new Channel(namehash, CT_PRIVATE, con);
    privchan->setTitle(title);
    ServerState::addChannel(namehash, privchan);

    lua_pushstring(L, namehash.c_str());
    lua_pushlightuserdata(L, privchan);
    return 2;
}

/**
 * Creates a new private channel with a specific name and title.
 * @param string name
 * @param string title
 * @returns [LUD] channel
 */
int LuaChannel::createSpecialPrivateChannel(lua_State* L) {
    luaL_checkany(L, 2);

    string name = luaL_checkstring(L, 1);
    string title = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    Channel* privchan = new Channel(name, CT_PRIVATE);
    privchan->setTitle(title);
    privchan->setCanDestroy(false);
    ServerState::addChannel(name, privchan);

    lua_pushlightuserdata(L, privchan);
    return 1;
}

/**
 * Destroys a channel. This will send LCH messages to all channel participants if the channel exists.
 * @param string channel name
 * @returns Nothing.
 */
int LuaChannel::destroyChannel(lua_State* L) {
    luaL_checkany(L, 1);

    string name = luaL_checkstring(L, 1);
    lua_pop(L, 1);

    ChannelPtr chan = ServerState::getChannel(name);
    if (chan) {
        const ChannelType type = chan->getType();
        const char* channame = chan->getName().c_str();
        const chconlist_t particpants = chan->getParticipants();
        for (chconlist_t::const_iterator i = particpants.begin(); i != particpants.end(); ++i) {
            json_t* root = json_object();
            json_object_set_new_nocheck(root, "channel",
                    json_string_nocheck(channame)
                    );
            json_object_set_new_nocheck(root, "character",
                    json_string_nocheck((*i)->characterName.c_str())
                    );
            const char* leavestr = json_dumps(root, JSON_COMPACT);
            string msg = "LCH ";
            msg += leavestr;
            free((void*) leavestr);
            json_decref(root);
            MessagePtr outMessage(MessageBuffer::fromString(msg));
            (*i)->send(outMessage);
            chan->part((*i));
        }
        ServerState::removeChannel(name);
        if (type == CT_PUBLIC)
            ServerState::rebuildChannelOpList();
    }
    return 0;
}

/**
 * Sends a json encoded message to all participants of a channel, including the sender.
 * @param LUD channel
 * @param string message prefix
 * @param table json
 * @returns Nothing.
 */
int LuaChannel::sendToAll(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string message = luaL_checkstring(L, 2);
    if (lua_type(L, 3) != LUA_TTABLE)
        return luaL_error(L, "sendtoall expects a table as argument 3.");

    json_t* json = LuaChat::luaToJson(L);
    lua_pop(L, 3);
    message += " ";
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    chan->sendToAll(message);
    return 0;
}

/**
 * Sends a raw message to all participants of a channel, including the sender.
 * @param LUD channel
 * @param string message
 * @returns Nothing.
 */
int LuaChannel::sendToAllRaw(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string message = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    chan->sendToAll(message);
    return 0;
}

/**
 * Sends a json encoded message to all participants of a channel, excluding the sender.
 * @param LUD channel
 * @param LUD sender
 * @param string message prefix
 * @param table json
 * @returns Nothing.
 */
int LuaChannel::sendToChannel(lua_State* L) {
    luaL_checkany(L, 4);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    string message = luaL_checkstring(L, 3);
    if (lua_type(L, 4) != LUA_TTABLE)
        return luaL_error(L, "sendtochannel expects a table as argument 4.");

    json_t* json = LuaChat::luaToJson(L);
    lua_pop(L, 4);
    message += " ";
    const char* jsonstr = json_dumps(json, JSON_COMPACT);
    message += jsonstr;
    free((void*) jsonstr);
    json_decref(json);
    chan->sendToChannel(con, message);
    return 0;
}

/**
 * Sends a raw message to all participants of a channel, exclusing the sender.
 * @param LUD channel
 * @param LUD sender
 * @param string message
 * @returns Nothing.
 */
int LuaChannel::sendToChannelRaw(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    string message = luaL_checkstring(L, 3);
    lua_pop(L, 3);

    chan->sendToChannel(con, message);
    return 0;
}

/**
 * Sends the ICH message for the selected channel to the selected connection.
 * @param LUD channel
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChannel::sendICH(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    json_t* root = json_object();
    json_object_set_new_nocheck(root, "channel",
            json_string_nocheck(chan->getName().c_str())
            );
    json_object_set_new_nocheck(root, "mode",
            json_string_nocheck(chan->getModeString().c_str())
            );
    json_t* array = json_array();
    const chconlist_t participants = chan->getParticipants();
    for (chconlist_t::const_iterator i = participants.begin(); i != participants.end(); ++i) {
        json_t* charnode = json_object();
        json_object_set_new_nocheck(charnode, "identity",
                json_string_nocheck((*i)->characterName.c_str())
                );
        json_array_append_new(array, charnode);
    }

    json_object_set_new_nocheck(root, "users", array);
    string msg = "ICH ";
    const char* ichstr = json_dumps(root, JSON_COMPACT);
    msg += ichstr;
    MessagePtr outMessage(MessageBuffer::fromString(msg));
    con->send(outMessage);
    free((void*) ichstr);
    json_decref(root);
    return 0;
}

/**
 * Joins a connection to a channel.
 * @param LUD channel
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChannel::joinChannel(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    chan->join(con);

    return 0;
}

/** Parts a connection from a channel.
 * @param LUD channel
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChannel::partChannel(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    chan->part(con);

    return 0;
}

/**
 * Bans a character from a channel.
 * @param LUD channel
 * @param LUD/string connection/name
 * @returns Nothing.
 */
int LuaChannel::ban(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, src);
    int type = lua_type(L, 3);
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 3, dest);
        chan->ban(src, dest);
    } else if (type == LUA_TSTRING) {
        string dest = lua_tostring(L, 3);
        chan->ban(src, dest);
    } else
        return luaL_error(L, "ban must have a string or a ConnectionPtr as the third argument.");

    lua_pop(L, 3);
    return 0;
}

/**
 * Times out a character from a channel.
 * @param LUD channel
 * @param LUD/string connection/name
 * @param integer length of timeout in seconds
 * @returns Nothing.
 */
int LuaChannel::timeout(lua_State* L) {
    luaL_checkany(L, 4);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, src);
    long length = luaL_checkinteger(L, 4);
    int type = lua_type(L, 3);
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 3, dest);
        chan->timeout(src, dest, length);
    } else if (type == LUA_TSTRING) {
        string dest = lua_tostring(L, 3);
        chan->timeout(src, dest, length);
    } else
        return luaL_error(L, "timeout must have a string or a ConnectionPtr as the third argument.");

    lua_pop(L, 3);
    return 0;
}

/**
 * Removes a character from a channel banlist.
 * @param LUD channel
 * @param string name
 * @returns Nothing.
 */
int LuaChannel::unban(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string dest = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    chan->unban(dest);
    return 0;
}

/**
 * Returns a boolean that describes if a connection or name is banned from a channel.
 * @param LUD channel
 * @param LUD/string connection/name
 * @returns true if the connection or name is in the channels ban list, false otherwise.
 */
int LuaChannel::isBanned(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    int type = lua_type(L, 2);
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 2, con);
        ret = chan->isBanned(con);
    } else if (type == LUA_TSTRING) {
        string name = lua_tostring(L, 2);
        ret = chan->isBanned(name);
    } else
        return luaL_error(L, "isBanned expects string or ConnectionPtr as argument 2.");

    lua_pop(L, 2);
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Returns a ban structure for a given ban in the given channel.
 * @param LUD channel
 * @param LUD/string connection/name
 * @returns [boolean] true if the connection or name is in the channels ban list, false otherwise, [table] ban record
 */
int LuaChannel::getBan(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    int type = lua_type(L, 2);
    BanRecord ban;
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 2, con);
        ret = chan->getBan(con, ban);
    } else if (type == LUA_TSTRING) {
        string name = lua_tostring(L, 2);
        ret = chan->getBan(name, ban);
    } else
        return luaL_error(L, "getBan expects string or ConnectionPtr as argument 2.");

    lua_pop(L, 2);
    lua_pushboolean(L, ret);
    if (ret == false) {
        lua_pushnil(L);
    } else {
        lua_newtable(L);
        lua_pushinteger(L, ban.timeout);
        lua_setfield(L, -2, "timeout");
        lua_pushinteger(L, ban.time);
        lua_setfield(L, -2, "time");
        lua_pushstring(L, ban.banner.c_str());
        lua_setfield(L, -2, "banner");
    }
    return 2;
}

/**
 * Gets the ban list for the channel.
 * @param LUD channel
 * @returns [table] List of banned members, lowercased.
 */
int LuaChannel::getBanList(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_newtable(L);
    const chbanmap_t bans = chan->getBanRecords();
    int i = 1;
    for (chbanmap_t::const_iterator itr = bans.begin(); itr != bans.end(); ++itr) {
        lua_pushstring(L, itr->first.c_str());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

/**
 * Adds a connection to a channels invite list if the channel is private.
 * @param LUD channel
 * @param LUD connection
 * @returns Nothing.
 */
int LuaChannel::invite(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, dest);
    lua_pop(L, 2);

    if (chan->getType() != CT_PRIVATE)
        return luaL_error(L, "Channel provided was not a private channel.");

    chan->invite(dest);

    return 0;
}

/**
 * Removes a connection from a channels invite list if the channel is private.
 * @param LUD channel
 * @param string character
 * @returns Nothing
 */
int LuaChannel::removeInvite(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string dest = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    if (chan->getType() != CT_PRIVATE)
        return luaL_error(L, "Channel provided was not a private channel.");

    chan->removeInvite(dest);

    return 0;
}

/**
 * Checks if a connection is invited to a channel.
 * @param LUD channel
 * @param LUD connection
 * @returns [boolean] If the connection was invited to the channel.
 */
int LuaChannel::isInvited(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    if (chan->getType() != CT_PRIVATE)
        return luaL_error(L, "Channel provided was not a private channel.");

    ret = chan->isInvited(con);

    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Returns a boolean that describes if a connection is in a channel.
 * @param LUD channel
 * @param LUD connection
 * @returns true if the connection is in the channel, false otherwise.
 */
int LuaChannel::inChannel(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    if (chan->inChannel(con))
        lua_pushboolean(L, true);
    else
        lua_pushboolean(L, false);

    return 1;
}

/**
 * Adds a name to the channels moderator list.
 * @param LUD channel
 * @param LUD source connection
 * @param string moderator name
 * @returns Nothing.
 */
int LuaChannel::addMod(lua_State* L) {
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, src);
    string dest = luaL_checkstring(L, 3);
    lua_pop(L, 3);

    chan->addMod(src, dest);
    if (chan->getType() == CT_PUBLIC)
        ServerState::rebuildChannelOpList();

    return 0;
}

/**
 * Removes a name from the channels moderator list.
 * @param LUD channel
 * @param string moderator name
 * @returns Nothing.
 */
int LuaChannel::removeMod(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string dest = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    chan->remMod(dest);
    if (chan->getType() == CT_PUBLIC)
        ServerState::rebuildChannelOpList();

    return 0;
}

/**
 * Returns a boolean that describes if a connection or name is a moderator in the channel.
 * @param LUD channel
 * @param LUD/string connection/name
 * @returns true if the connection or name is in the channels moderator list, false otherwise.
 */
int LuaChannel::isMod(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    int type = lua_type(L, 2);
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 2, con);
        ret = chan->isMod(con);
    } else if (type == LUA_TSTRING) {
        string name = lua_tostring(L, 2);
        ret = chan->isMod(name);
    } else
        return luaL_error(L, "isMod expects string or ConnectionPtr as argument 2.");

    lua_pop(L, 2);
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Returns a boolean that describes if a connection or name is the channel owner.
 * @param LUD channel
 * @param LUD/string connection/name
 * @param bool check_only Checks only against the channel owner.
 * @returns true if the connection or name is in the channels owner, false otherwise.
 */
int LuaChannel::isOwner(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    int type = lua_type(L, 2);
    if (type == LUA_TLIGHTUSERDATA) {
        GETLCON(base, L, 2, con);
        ret = chan->isOwner(con);
    } else if (type == LUA_TSTRING) {
        string name = lua_tostring(L, 2);
        ret = chan->isOwner(name);
    } else
        return luaL_error(L, "isOwner expects string or ConnectionPtr as argument 2.");

    lua_pop(L, 2);
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Gets a the list of moderators in a channel.
 * @param LUD channel
 * @returns [table of string] moderators
 */
int LuaChannel::getModList(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_newtable(L);
    const chmodmap_t mods = chan->getModRecords();
    int i = 1;
    lua_pushstring(L, chan->getOwner().c_str());
    lua_rawseti(L, -2, i++);
    for (chmodmap_t::const_iterator itr = mods.begin(); itr != mods.end(); ++itr) {
        lua_pushstring(L, itr->first.c_str());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

/**
 * Updates a channel timer entry with a new value.
 * @param LUD channel
 * @param LUD connection
 * @param number new value
 * @returns [boolean] true if the timer has not expired, false otherwise
 */
int LuaChannel::checkUpdateTimer(lua_State* L) {
    bool ret = false;
    luaL_checkany(L, 3);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    double timeout = luaL_checknumber(L, 3);
    lua_pop(L, 3);

    double time = Server::getEventTime();

    if (chan->getTimerEntry(con) > (time - timeout))
        ret = true;
    else
        chan->setTimerEntry(con, time);

    lua_pushboolean(L, ret);
    return 1;
}

/**
 * Marks a private channel as public or private. Private channels marked as public show in the open rooms list.
 * @param LUD channel
 * @param bool new public status
 * @returns Nothing.
 */
int LuaChannel::setPublic(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    if (lua_type(L, 2) != LUA_TBOOLEAN)
        return luaL_error(L, "setpublic expects boolean for argument 2.");

    bool bepublic = lua_toboolean(L, 2);
    lua_pop(L, 2);

    if (chan->getType() != CT_PRIVATE && chan->getType() != CT_PUBPRIVATE)
        return luaL_error(L, "Channel provided was not a private channel.");

    chan->setPublic(bepublic);
    return 0;
}

/**
 * Retrieves a channels type. Public, Private, PublicPrivate.
 * @param LUD channel
 * @returns string containing "public", "private" or "pubprivate"
 */
int LuaChannel::getType(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushstring(L, chan->getTypeString().c_str());
    return 1;
}

/**
 * Sets the message mode for a channel.
 * @param LUD channel
 * @param string new message mode
 * @returns Nothing.
 */
int LuaChannel::setMode(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string mode = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    ChannelMessageMode newmode = CMM_BOTH;
    if (mode == "ads")
        newmode = CMM_ADS_ONLY;
    else if (mode == "chat")
        newmode = CMM_CHAT_ONLY;

    chan->setMode(newmode);
    return 0;
}

/**
 * Gets the current channel message mode.
 * @param LUD channel
 * @returns [string] channel message mode.
 */
int LuaChannel::getMode(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushstring(L, chan->getModeString().c_str());
    return 1;
}

/**
 * Retrieves a channels description.
 * @param LUD channel
 * @returns [string] channel description.
 */
int LuaChannel::getDescription(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushstring(L, chan->getDescription().c_str());
    return 1;
}

/**
 * Sets a channels description.
 * @param LUD channel
 * @param string new description
 * @returns Nothing.
 */
int LuaChannel::setDescription(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string newdesc = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    chan->setDescription(newdesc);
    return 0;
}

/**
 * Gets the title for a private channel.
 * @param LUD channel
 * @returns [string] channel title.
 */
int LuaChannel::getTitle(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    lua_pushstring(L, chan->getTitle().c_str());
    return 1;
}

/**
 * Sets the title for a private channel.
 * @param LUD channel
 * @param string new title
 * @returns Nothing.
 */
int LuaChannel::setTitle(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    string newtitle = luaL_checkstring(L, 2);
    lua_pop(L, 2);

    chan->setTitle(newtitle);
    return 0;
}

/**
 * Sets the owner for a channel.
 * @param LUD channel
 * @param LUD new owner connection
 * @returns Nothing.
 */
int LuaChannel::setOwner(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 2);

    chan->setOwner(con->characterName);
    return 0;
}

/**
 * Pulls a list of users in a room that can have a bottle spin land on them.
 * @param LUD channel
 * @returns [table of strings] List of possible bottle users.
 */
int LuaChannel::getBottleList(lua_State* L) {
    luaL_checkany(L, 2);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    GETLCON(base, L, 2, con);
    lua_pop(L, 1);

    const chconlist_t participants = chan->getParticipants();
    lua_newtable(L);
    int n = 1;
    for (chconlist_t::const_iterator i = participants.begin(); i != participants.end(); ++i) {
        if (((*i)->status != "busy") && ((*i)->status != "dnd") && ((*i)->characterNameLower != con->characterNameLower)) {
            lua_pushstring(L, (*i)->characterName.c_str());
            lua_rawseti(L, -2, n++);
        }
    }

    return 1;
}

int LuaChannel::canDestroy(lua_State* L) {
    luaL_checkany(L, 1);

    LBase* base = 0;
    GETLCHAN(base, L, 1, chan);
    lua_pop(L, 1);

    bool ret = true;
    ret = chan->getCanDestroy();
    lua_pushboolean(L, ret);
    return 1;
}
