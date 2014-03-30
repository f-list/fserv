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
#include "lua_constants.hpp"

lconstantmap_t LuaConstants::errorMap;

#define LUACONSTANTS_MODULE_NAME "const"

void LuaConstants::initClass() {
#define E(name, message) errorMap[name] = std::make_pair( #name, message )
    E(FERR_OK, "Operation completed successfully.");
    E(FERR_BAD_SYNTAX, "Syntax error.");
    E(FERR_SERVER_FULL, "Server full.");
    E(FERR_REQUIRES_IDENT, "This command requires that you have logged in.");
    E(FERR_IDENT_FAILED, "Identification failed.");
    E(FERR_THROTTLE_MESSAGE, "You must wait one second between sending channel/private messages.");
    E(FERR_USER_NOT_FOUND, "The character requested was not found.");
    E(FERR_THROTTLE_PROFILE, "You must wait ten seconds between requesting profiles.");
    E(FERR_UNKNOWN_COMMAND, "Unknown command.");
    E(FERR_BANNED_FROM_SERVER, "You are banned from the server.");
    E(FERR_NOT_ADMIN, "This command requires that you be an administrator.");
    E(FERR_ALREADY_IDENT, "Already identified.");

    E(FERR_THROTTLE_KINKS, "You must wait ten seconds between requesting kinks.");

    E(FERR_MESSAGE_TOO_LONG, "Message exceeded the maximum length.");
    E(FERR_ALREADY_OP, "This character is already a global moderator.");
    E(FERR_NOT_AN_OP, "This character is not a global moderator.");
    E(FERR_NO_SEARCH_RESULTS, "There were no search results.");
    E(FERR_NOT_OP, "This command requires that you be a moderator.");
    E(FERR_IGNORED, "This user does not wish to receive messages from you.");
    E(FERR_DENIED_ON_OP, "This action can not be used on a moderator or administrator.");

    E(FERR_CHANNEL_NOT_FOUND, "Could not locate the requested channel.");

    E(FERR_ALREADY_IN_CHANNEL, "You are already in the requested channel.");

    E(FERR_TOO_MANY_FROM_IP, "There are too many connections from your IP.");
    E(FERR_LOGGED_IN_AGAIN, "You have been disconnected because this character has been logged in at another location.");
    E(FERR_ALREADY_BANNED, "That account is already banned.");
    E(FERR_UNKNOWN_AUTH_METHOD, "Unknown authentication method requested.");

    E(FERR_BAD_ROLL_FORMAT, "There was a problem with your roll command.");

    E(FERR_BAD_TIMEOUT_FORMAT, "The time given for the timeout was invalid. It must be a number between 1 and 90 minutes.");
    E(FERR_TIMED_OUT, "You have been timed out from chat.");
    E(FERR_KICKED, "You have been kicked from chat.");
    E(FERR_ALREADY_CHANNEL_BANNED, "This character is already banned from the channel.");
    E(FERR_NOT_CHANNEL_BANNED, "This character is not currently banned from the channel.");

    E(FERR_NOT_INVITED, "You may only join the requested channel with an invite.");
    E(FERR_NOT_IN_CHANNEL, "You must be in a channel to send messages to it.");

    E(FERR_INVITE_TO_PUBLIC, "You may not invite others to a public channel.");
    E(FERR_CHANNEL_BANNED, "You are banned from the requested channel.");
    E(FERR_USER_NOT_IN_CHANNEL, "That character was not found in the channel.");
    E(FERR_THROTTLE_SEARCH, "You must wait five seconds between searches.");

    E(FERR_THROTTLE_STAFF_CALL, "Please wait two minutes between calling moderators. If you need to make an addition or a correction to a report, please contact a moderator directly.");

    E(FERR_THROTTLE_AD, "You may only post a role play ad to a channel every ten minutes.");

    E(FERR_CHAT_ONLY, "This channel does not allow role play ads, only chat messages.");
    E(FERR_ADS_ONLY, "This channel does not allow chat messages, only role play ads.");

    E(FERR_TOO_MANY_SEARCH_TERMS, "There were too many search terms.");
    E(FERR_NO_LOGIN_SLOT, "There are currently no free login slots.");

    E(FERR_TOO_MANY_SEARCH_RESULTS, "There are too many search results, please narrow your search.");

    E(FERR_FATAL_INTERNAL, "Fatal internal error.");
    E(FERR_LUA, "An error occurred while processing your command.");
    E(FERR_NOT_IMPLEMENTED, "This command has not been implemented yet.");
    E(FERR_LOGIN_TIMED_OUT, "A connection to the login server timed out. Please try again in a moment.");
    E(FERR_UNKNOWN, "An unknown error occurred.");
    E(FERR_WRONG_TICKET_VERSION, "You are attempting to log in using an outdated API ticket version. Please contact Kira for details.");

#undef E
}

static const luaL_Reg luaconstants_funcs[] = {
    {"getErrorMessage", LuaConstants::getErrorMessage},
    {NULL, NULL}
};

int LuaConstants::openConstantsLib(lua_State* L) {
    luaL_register(L, LUACONSTANTS_MODULE_NAME, luaconstants_funcs);
    for (lconstantmap_t::const_iterator i = errorMap.begin(); i != errorMap.end(); ++i) {
        lua_pushinteger(L, i->first);
        lua_setfield(L, -2, i->second.first.c_str());
    }
    return 0;
}

/**
 * Gets the error message that goes with an error code.
 * @param number errorcode
 * @returns [string] error message.
 */
int LuaConstants::getErrorMessage(lua_State* L) {
    luaL_checkany(L, 1);
    int errorcode = luaL_checkinteger(L, 1);
    lua_pop(L, 1);

    if (errorMap.find(errorcode) == errorMap.end())
        lua_pushstring(L, errorMap[FERR_UNKNOWN].second.c_str());
    else
        lua_pushstring(L, errorMap[errorcode].second.c_str());

    return 1;
}

const std::string& LuaConstants::getErrorMessage(FReturnCode errorcode) {
    if (errorMap.find(errorcode) == errorMap.end())
        return errorMap[FERR_UNKNOWN].second;

    return errorMap[errorcode].second;
}
