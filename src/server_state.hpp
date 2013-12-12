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

#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <list>
#include <time.h>

#include "connection.hpp"
#include "channel.hpp"

using std::list;
using std::tr1::unordered_map;
using std::tr1::unordered_set;

typedef struct {
    string character;
    time_t end;
} TimeoutRecord;

typedef struct {
    long account_id;
    string character;
    time_t end;
} AltWatchRecord;

typedef struct {
    string action;
    string character;
    string report;
    time_t timestamp;
    string callid;
    long logid;
} StaffCallRecord;


typedef unordered_map<string, ConnectionPtr> conptrmap_t; //character name lower, connection
typedef unordered_map<int, int> concountmap_t; //IP, count
typedef unordered_map<string, ChannelPtr> chanptrmap_t; //channel name lower, channel
typedef list<ConnectionPtr> conptrlist_t;
typedef list<string> oplist_t; //op name
typedef unordered_map<long, string> banlist_t; //account id, character name lower
typedef unordered_map<long, TimeoutRecord> timeoutmap_t; //account id, timeout record
typedef unordered_map<long, AltWatchRecord> altwatchmap_t; //account id, alt watch record
typedef unordered_map<string, StaffCallRecord> staffcallmap_t; //call id, staff call record
typedef unordered_set<string> chanoplist_t;

class ServerState {
public:
    static bool fsaveFile(const char* name, string& contents);
    static string floadFile(const char* name);

    static void loadChannels();
    static void saveChannels();
    static void cleanupChannels();
    static void removeUnusedChannels();

    static void loadOps();
    static void saveOps();

    static void loadBans();
    static void saveBans();

    static void sendUserListToRedis();

    static void addBan(string& character, long accountid);
    static bool removeBan(string& character);
    static bool isBanned(long accountid);

    static void addTimeout(string& character, long accountid, int length);
    static void removeTimeout(string& character);
    static bool isTimedOut(long accountid, int& end);

    static void addUnidentified(ConnectionPtr con);
    static void removedUnidentified(ConnectionPtr con);

    static void addConnection(string& name, ConnectionPtr con);
    static void removeConnection(string& name);
    static ConnectionPtr getConnection(string& name);

    static const conptrmap_t& getConnections() {
        return connectionMap;
    }
    static const int getConnectionIPCount(ConnectionPtr con);

    static const long getConnectionCount() {
        return connectionMap.size();
    }

    static void addChannel(string& name, Channel* channel);
    static void removeChannel(string& name);
    static ChannelPtr getChannel(string& name);

    static const chanptrmap_t& getChannels() {
        return channelMap;
    }

    static const long getChannelCount() {
        return channelMap.size();
    }

    static void addOp(string& op);
    static void removeOp(string& op);
    static bool isOp(string& op);

    static const oplist_t& getOpList() {
        return opList;
    }

    static void addAltWatch(long accountid, AltWatchRecord& record);
    static void removeAltWatch(long accountid);
    static AltWatchRecord getAltWatch(long accountid);
    static void cleanAltWatchList();

    static void addStaffCall(string& callid, StaffCallRecord& record);
    static void removeStaffCall(string& callid);
    static StaffCallRecord getStaffCall(string& callid);

    static const staffcallmap_t& getStaffCalls() {
        return staffCallList;
    }

    static void rebuildChannelOpList();
    static bool isChannelOp(string& name);

    static const long getUserCount() {
        return userCount;
    }

    static const long getMaxUserCount() {
        return maxUserCount;
    }
private:

    ServerState() { }

    ~ServerState() { }

    static long userCount;
    static long maxUserCount;
    static conptrmap_t connectionMap;
    static concountmap_t connectionCountMap;
    static chanptrmap_t channelMap;
    static conptrlist_t unidentifiedList;
    static oplist_t opList;
    static banlist_t banList;
    static timeoutmap_t timeoutList;
    static altwatchmap_t altWatchList;
    static staffcallmap_t staffCallList;
    static chanoplist_t channelOpList;
};
#endif //SERVER_STATE_H
