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

#ifndef LUA_CHANNEL_H
#define LUA_CHANNEL_H

#include "lua_chat.hpp"

class LuaChannel
{
public:
	static int openChannelLib(lua_State* L);

	static int sendCHA(lua_State* L);
	static int sendORS(lua_State* L);

	static int getChannel(lua_State* L);
	static int getName(lua_State* L);
	static int getUserCount(lua_State* L);
	static int getTopUserCount(lua_State* L);

	static int createChannel(lua_State* L);
	static int createPrivateChannel(lua_State* L);
	static int createSpecialPrivateChannel(lua_State* L);
	static int destroyChannel(lua_State* L);

	static int sendToAll(lua_State* L);
	static int sendToAllRaw(lua_State* L);
	static int sendToChannel(lua_State* L);
	static int sendToChannelRaw(lua_State* L);

	static int sendICH(lua_State* L);

	static int joinChannel(lua_State* L);
	static int partChannel(lua_State* L);

	static int ban(lua_State* L);
	static int timeout(lua_State* L);
	static int unban(lua_State* L);
	static int isBanned(lua_State* L);
	static int getBan(lua_State* L);
	static int getBanList(lua_State* L);

	static int invite(lua_State* L);
	static int removeInvite(lua_State* L);
	static int isInvited(lua_State* L);

	static int inChannel(lua_State* L);

	static int addMod(lua_State* L);
	static int removeMod(lua_State* L);
	static int isMod(lua_State* L);
	static int isOwner(lua_State* L);
	static int getModList(lua_State* L);

	static int checkUpdateTimer(lua_State* L);

	static int getType(lua_State* L);
	static int getMode(lua_State* L);
	static int setMode(lua_State* L);

	static int setPublic(lua_State* L);

	static int getDescription(lua_State* L);
	static int setDescription(lua_State* L);

	static int getTitle(lua_State* L);
	static int setTitle(lua_State* L);

	static int setOwner(lua_State* L);

	static int getBottleList(lua_State* L);

	static int canDestroy(lua_State* L);
private:
	LuaChannel() {}
	~LuaChannel() {}
};

#endif //LUA_CHANNEL_H
