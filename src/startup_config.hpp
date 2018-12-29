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

#ifndef STARTUP_CONFIG_H
#define STARTUP_CONFIG_H

#include <string>
#include <vector>
#include <tr1/unordered_map>

using std::string;
using std::vector;
using std::tr1::unordered_map;

struct lua_State;

class StartupConfig {
public:
    //Class startup
    static void init();

    static bool getString(const char* name, string& value);
    static string getString(const char* name);
    static bool getBool(const char* name, bool& value);
    static bool getBool(const char* name);
    static bool getDouble(const char* name, double& value);
    static double getDouble(const char* name);
    static bool getStringList(const char* name, vector<string>& value);

private:

    StartupConfig() { }

    ~StartupConfig() { }

    static unordered_map<string, string> stringMap;
    static unordered_map<string, bool> boolMap;
    static unordered_map<string, double> doubleMap;
    static unordered_map<string, vector<string> > stringListMap;

};

#endif //STARTUP_CONFIG_H
