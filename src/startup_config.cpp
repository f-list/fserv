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

#include "precompiled_headers.h"

#include "startup_config.h"

#include "logging.h"
#include "flua.h"


unordered_map<string, string> StartupConfig::stringMap;
unordered_map<string, bool> StartupConfig::boolMap;
unordered_map<string, double> StartupConfig::doubleMap;

void StartupConfig::init()
{
	DLOG(INFO) << "Loading startup config.";
	lua_State* L = luaL_newstate();
	int ret = luaL_dofile(L, "./script/startup_config.lua");
	if(ret)
	{
		LOG(WARNING) << "Failed to load the startup config file. Reason: " << lua_tostring(L, -1);
		return;
	}
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_pushnil(L);
	while(lua_next(L, -2))
	{
		if(lua_type(L, -2) != LUA_TSTRING)
		{
			lua_pop(L, 1);
			continue;
		}

		int type = lua_type(L, -1);
		switch (type)
		{
			case LUA_TSTRING:
			{
				const char* value = lua_tostring(L, -1);
				const char* key = lua_tostring(L, -2);
				stringMap[key] = value;
				break;
			}
			case LUA_TBOOLEAN:
			{
				bool value = lua_toboolean(L, -1);
				const char* key = lua_tostring(L, -2);
				boolMap[key] = value;
				break;
			}
			case LUA_TNUMBER:
			{
				double value = lua_tonumber(L, -1);
				const char* key = lua_tostring(L, -2);
				doubleMap[key] = value;
				break;
			}
			default:
				break;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	lua_close(L);
	L = 0;
}

bool StartupConfig::getBool(const char* name, bool& value)
{
	bool found = false;
	string key = name;
	if(boolMap.count(key) != 0)
	{
		value = boolMap[key];
		found = true;
	}
	else
	{
		LOG(DFATAL) << "Could not find boolean config value '" << key << "'";
	}
	return found;
}

bool StartupConfig::getBool(const char* name)
{
	bool ret = false;
	string key = name;
	if(boolMap.count(key) != 0)
	{
		ret = boolMap[key];
	}
	else
	{
		LOG(DFATAL) << "Could not find boolean config value '" << key << "', returning false";
	}
	return ret;
}

bool StartupConfig::getDouble(const char* name, double& value)
{
	bool found = false;
	string key = name;
	if(doubleMap.count(key) != 0)
	{
		value = doubleMap[key];
		found = true;
	}
	else
	{
		LOG(DFATAL) << "Could not find double config value '" << key << "'";
	}
	return found;
}

double StartupConfig::getDouble(const char* name)
{
	double ret = 0;
	string key = name;
	if(doubleMap.count(key) != 0)
	{
		ret = doubleMap[key];
	}
	else
	{
		LOG(DFATAL) << "Could not find double config value '" << key << "', returning 0.0";
	}
	return ret;
}

bool StartupConfig::getString(const char* name, string& value)
{
	bool found = false;
	string key = name;
	if(stringMap.count(key) != 0)
	{
		value = stringMap[key];
		found = true;
	}
	else
	{
		LOG(DFATAL) << "Could not find string config value '" << key << "'";
	}
	return found;
}

string StartupConfig::getString(const char* name)
{
	string ret;
	string key = name;
	if(stringMap.count(key) != 0)
	{
		ret = stringMap[key];
	}
	else
	{
		LOG(DFATAL) << "Could not find string config value '" << key << "', returning empty string";
	}
	return ret;
}
