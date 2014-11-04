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

#ifndef FLUA_H
#define FLUA_H

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#define LUA_ABSINDEX(L, i) ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
                                        lua_gettop(L) + (i) + 1)

#define GETLCHAN(lbase, lstate, index, varname) if(lua_type(lstate, index) != LUA_TLIGHTUSERDATA) \
                                                        return luaL_error(lstate, "#1 Argument "#index" was not a Channel was a %s", lua_typename(lstate, index)); \
                                                lbase = static_cast<LBase*>(lua_touserdata(lstate, index)); \
                                                Channel* varname = dynamic_cast<Channel*>( lbase ); \
                                                lbase = 0; \
                                                if( varname == 0 ) \
                                                        return luaL_error(lstate, "#2 Argument "#index" was not a Channel.")

#define GETLCON(lbase, lstate, index, varname) if(lua_type(lstate, index) != LUA_TLIGHTUSERDATA) \
                                                        return luaL_error(lstate, "#1 Argument "#index" was not a ConnectionPtr was a %s", lua_typename(lstate, index)); \
                                                lbase = static_cast<LBase*>(lua_touserdata(lstate, index)); \
                                                ConnectionPtr varname(dynamic_cast<ConnectionInstance*>( lbase )); \
                                                lbase = 0; \
                                                if( varname == 0 ) \
                                                        return luaL_error(lstate, "#2 Argument "#index" was not a ConnectionPtr.")
    

namespace lua {

// let's try some extra functions and templates...
// this struct allows us to overload based on a type we're given
// (partial specialization)
template <typename T>
struct getter {};

// Each specialization allows us to decide what to return
// if a specialization doesn't exist, then there's a compile-time
// error telling us we need to add another specialization
// for the argument type we want to return
template <>
struct getter<std::string> {
    inline static std::string get (lua_State* L, int index) {
        return luaL_checkstring(L, index);
    }
};

template <>
struct getter<const char*> {
    inline static std::string get (lua_State* L, int index) {
        return luaL_checkstring(L, index);
    }
};

template <>
struct getter<int> {
    inline static int get (lua_State* L, int index) {
        return luaL_checkinteger(L, index);
    }
};

template <>
struct getter<bool> {
    inline static bool get (lua_State* L, int index) {
        return lua_toboolean(L, index);
    }
};

// now, we write a function that will call this thing for us
template <typename T>
inline T get(lua_State* L, int index) {
    getter<T> g;
    return g.get(L, index);
}

// we don't have to use the same specialization technique
// for the pusher, rather we just write overloads
// if one of these functions can't handle the return,
// we'll get a compile-time error, which is nice
// when coding new functionality
inline void push (lua_State* L, bool value) {
    lua_pushboolean(L, value);
}

inline void push (lua_State* L, int value) {
    lua_pushinteger(L, value);
}

inline void push (lua_State* L, const char* value) {
    lua_pushstring(L, value);
}

inline void push (lua_State* L, const std::string& value) {
    lua_pushstring(L, value.c_str());
}

} // lua

#endif //FLUA_H
