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
#include "server_state.hpp"
#include "channel.hpp"
#include "fjson.hpp"
#include "logging.hpp"
#include "redis.hpp"
#include "server.hpp"
#include "sha1.hpp"

#include <string>
#include <iostream>
#include <fstream>

conptrmap_t ServerState::connectionMap;
conidmap_t ServerState::connectionIdMap;
concountmap_t ServerState::connectionCountMap;
chanptrmap_t ServerState::channelMap;
conptrlist_t ServerState::unidentifiedList;
oplist_t ServerState::opList;
banlist_t ServerState::banList;
timeoutmap_t ServerState::timeoutList;
staffcallmap_t ServerState::staffCallList;
chanoplist_t ServerState::channelOpList;
conptrset_t ServerState::staffCallTargets;
scopset_t ServerState::superCopList;
long ServerState::userCount = 0;
long ServerState::maxUserCount = 0;
long ServerState::channelSeed = 0;

static const char* BAN_PATH = "./data/bans.json";
static const char* CHANNEL_PATH = "./data/channels.json";
static const char* OP_PATH = "./data/ops.json";

bool ServerState::fsaveFile(const char* name, string& contents) {
    std::ofstream file;
    file.open(name, std::ios::trunc);
    if (file.is_open()) {
        file << contents;
    } else {
        LOG(WARNING) << "Failed to open file " << name << " for writing.";
        return false;
    }
    file.close();
    return true;
}

string ServerState::floadFile(const char* name) {
    std::string contents;
    std::string buffer;
    std::ifstream file(name);
    while (file.good()) {
        std::getline(file, buffer);
        contents.append(buffer);
    }
    file.close();

    return contents;
}

void ServerState::loadChannels() {
    DLOG(INFO) << "Loading channels.";
    string contents = floadFile(CHANNEL_PATH);
    json_error_t jserror;
    json_t* root = json_loads(contents.c_str(), 0, &jserror);
    if (!root) {
        LOG(WARNING) << "Failed to parse channel json. Error: " << &jserror.text;
        return;
    }

    json_t* pubchans = json_object_get(root, "public");
    if (!json_is_array(pubchans)) {
        LOG(ERROR) << "Failed to find the public channels node.";
        return;
    }
    size_t size = json_array_size(pubchans);
    for (size_t i = 0; i < size; ++i) {
        json_t* chan = json_array_get(pubchans, i);
        string name = json_string_value(json_object_get(chan, "name"));
        removeChannel(name);
        Channel* chanptr = new Channel(name, CT_PUBLIC);
        chanptr->loadChannel(chan);
        addChannel(name, chanptr);
    }
    json_t* privchans = json_object_get(root, "private");
    if (!json_is_array(privchans)) {
        LOG(ERROR) << "Failed to find the private channels node.";
        return;
    }
    size = json_array_size(privchans);
    for (size_t i = 0; i < size; ++i) {
        json_t* chan = json_array_get(privchans, i);
        string name = json_string_value(json_object_get(chan, "name"));
        removeChannel(name);
        Channel* chanptr = new Channel(name, CT_PUBPRIVATE);
        chanptr->loadChannel(chan);
        addChannel(name, chanptr);
    }
    json_decref(root);
}

void ServerState::saveChannels() {
    DLOG(INFO) << "Saving channels.";
    json_t* root = json_object();
    json_t* publicarray = json_array();
    json_t* privatearray = json_array();
    for (chanptrmap_t::const_iterator i = channelMap.begin(); i != channelMap.end(); ++i) {
        ChannelPtr chan = i->second;
        if (chan->getType() == CT_PUBLIC) {
            json_array_append_new(publicarray, chan->saveChannel());
        } else if (chan->getType() == CT_PUBPRIVATE) {
            json_array_append_new(privatearray, chan->saveChannel());
        }
    }
    json_object_set_new_nocheck(root, "public", publicarray);
    json_object_set_new_nocheck(root, "private", privatearray);
    const char* chanstr = json_dumps(root, JSON_INDENT(4));
    string contents = chanstr;
    free((void*) chanstr);
    json_decref(root);
    fsaveFile(CHANNEL_PATH, contents);
}

void ServerState::cleanupChannels() {
    chanptrmap_t chans = getChannels();
    chans.clear();
}

void ServerState::removeUnusedChannels() {
    const chanptrmap_t chans = getChannels();
    list<string> toremove;
    time_t timeout = time(NULL)-(60 * 60 * 24);
    for (chanptrmap_t::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        i->second->updateParticipantCount();
        if (i->second->getType() == CT_PUBPRIVATE) {
            if ((i->second->getParticipantCount() <= 0) && (i->second->getLastActivity() < timeout)) {
                toremove.push_back(i->first);
            }
        }
    }
    for (list<string>::const_iterator i = toremove.begin(); i != toremove.end(); ++i) {
        string channel = (*i);
        removeChannel(channel);
    }
    DLOG(INFO) << "Removed " << toremove.size() << " unused channels.";
}

void ServerState::loadStringList(string filename, stringFunctionTarget target, clearFunction clear) {
    string contents = floadFile(filename.c_str());
    json_error_t jserror;
    json_t* root = json_loads(contents.c_str(), 0, &jserror);
    if (!json_is_array(root)) {
        LOG(WARNING) << "Failed to parse the ops list json. Error: " << jserror.text;
        return;
    }
    clear();
    size_t size = json_array_size(root);
    for (size_t i = 0; i < size; ++i) {
        json_t* jop = json_array_get(root, i);
        if (!json_is_string(jop)) {
            LOG(WARNING) << "Failed to parse an op value because it was not a string.";
            continue;
        }
        string op = json_string_value(jop);
        target(op);
    }
    json_decref(root);
}

void ServerState::loadOps() {
    DLOG(INFO) << "Loading ops.";
    loadStringList("./ops.json", &addOp, &clearOps);
    DLOG(INFO) << "Loading super cops.";
    loadStringList("./scops.json", &addSuperCop, &clearSuperCops);
}

void ServerState::saveSCops() {
    DLOG(INFO) << "Saving super cops.";

    json_t* root = json_array();
    for(auto itr = superCopList.begin(); itr != superCopList.end(); ++itr) {
        json_array_append_new(root, json_string_nocheck(itr->c_str()));
    }
    const char* opstr = json_dumps(root, JSON_INDENT(4));
    string contents = opstr;
    free((void*) opstr);
    json_decref(root);
    fsaveFile("./scops.json", contents);
}

void ServerState::saveOps() {
    DLOG(INFO) << "Saving ops.";
    json_t* root = json_array();
    for (oplist_t::const_iterator i = opList.begin(); i != opList.end(); ++i) {
        json_array_append_new(root, json_string_nocheck(i->c_str()));
    }
    const char* opstr = json_dumps(root, JSON_INDENT(4));
    string contents = opstr;
    free((void*) opstr);
    json_decref(root);
    fsaveFile(OP_PATH, contents);
    saveSCops();
}

void ServerState::loadBans() {
    DLOG(INFO) << "Loading bans.";
    string contents = floadFile(BAN_PATH);
    json_t* root = json_loads(contents.c_str(), 0, 0);
    if (!json_is_object(root)) {
        LOG(WARNING) << "Could not load the ban list from disk.";
        return;
    }

    json_t* bans = json_object_get(root, "bans");
    if (!json_is_array(bans)) {
        LOG(WARNING) << "Could not find the bans node for the ban list.";
        return;
    }
    size_t size = json_array_size(bans);
    for (size_t i = 0; i < size; ++i) {
        json_t* ban = json_array_get(bans, i);
        if (!json_is_object(ban)) {
            LOG(WARNING) << "Ban node is not an object.";
            continue;
        }
        json_t* banid = json_object_get(ban, "id");
        if (!json_is_integer(banid)) {
            LOG(WARNING) << "Ban failed to parse because the ban id was not an integer.";
            continue;
        }
        json_t* banchar = json_object_get(ban, "character");
        if (!json_is_string(banchar)) {
            LOG(WARNING) << "Ban failed to parse because the ban character was not a string.";
            continue;
        }
        banList[json_integer_value(banid)] = json_string_value(banchar);
    }
    json_t* timeouts = json_object_get(root, "timeouts");
    if (!json_is_array(timeouts)) {
        LOG(ERROR) << "Could not find the timeouts node for the ban list.";
        return;
    }
    size = json_array_size(timeouts);
    for (size_t i = 0; i < size; ++i) {
        json_t* timeout = json_array_get(timeouts, i);
        if (!json_is_object(timeout)) {
            LOG(WARNING) << "Timeout node is not an object.";
            continue;
        }
        TimeoutRecord to;
        json_t* tochar = json_object_get(timeout, "character");
        if (!json_is_string(tochar)) {
            LOG(WARNING) << "Timeout character is not a string.";
            continue;
        }
        json_t* toend = json_object_get(timeout, "end");
        if (!json_is_integer(toend)) {
            LOG(WARNING) << "Timeout end is not an integer.";
            continue;
        }
        json_t* toid = json_object_get(timeout, "id");
        if (!json_is_integer(toid)) {
            LOG(WARNING) << "Timeout id is not an integer.";
            continue;
        }
        to.character = json_string_value(tochar);
        to.end = json_integer_value(toend);
        timeoutList[json_integer_value(toid)] = to;
    }
    json_decref(root);
}

void ServerState::saveBans() {
    DLOG(INFO) << "Saving bans.";
    for (timeoutmap_t::iterator i = timeoutList.begin(); i != timeoutList.end(); ) {
        if (i->second.end < time(0)) {
            timeoutList.erase(i++);
        } else {
            ++i;
        }
    }

    json_t* root = json_object();
    json_t* array = json_array();
    for (banlist_t::const_iterator i = banList.begin(); i != banList.end(); ++i) {
        json_t* ban = json_object();
        json_object_set_new_nocheck(ban, "id",
                json_integer(i->first)
                );
        json_object_set_new_nocheck(ban, "character",
                json_string_nocheck(i->second.c_str())
                );
        json_array_append_new(array, ban);
    }
    json_object_set_new_nocheck(root, "bans", array);
    array = json_array();
    for (timeoutmap_t::const_iterator i = timeoutList.begin(); i != timeoutList.end(); ++i) {
        json_t* timeout = json_object();
        json_object_set_new_nocheck(timeout, "id",
                json_integer(i->first)
                );
        json_object_set_new_nocheck(timeout, "end",
                json_integer(i->second.end)
                );
        json_object_set_new_nocheck(timeout, "character",
                json_string_nocheck(i->second.character.c_str())
                );
        json_array_append_new(array, timeout);
    }
    json_object_set_new_nocheck(root, "timeouts", array);
    const char* banstr = json_dumps(root, JSON_INDENT(4));
    string contents = banstr;
    free((void*) banstr);
    json_decref(root);
    fsaveFile(BAN_PATH, contents);
}

void ServerState::sendUserListToRedis() {
    RedisRequest* req = new RedisRequest;
    req->key = Redis::onlineUsersKey;
    req->method = REDIS_DEL;
    req->updateContext = RCONTEXT_ONLINE;
    if (!Redis::addRequest(req)) {
        delete req;
        return;
    }

    req = new RedisRequest;
    req->key = Redis::onlineUsersKey;
    req->method = REDIS_SADD;
    req->updateContext = RCONTEXT_ONLINE;
    for (conptrmap_t::const_iterator i = connectionMap.begin(); i != connectionMap.end(); ++i) {
        req->values.push(i->second->characterName);
    }
    if (!Redis::addRequest(req))
        delete req;
}

void ServerState::addUnidentified(ConnectionPtr con) {
    unidentifiedList.push_back(con);
}

void ServerState::removedUnidentified(ConnectionPtr con) {
    unidentifiedList.remove(con);
}

void ServerState::addConnection(string& name, ConnectionPtr con) {
    connectionCountMap[(int) con->clientAddress.sin_addr.s_addr] += 1;
    //DLOG(INFO) << "IP " << (int)con->clientAddress.sin_addr.s_addr << " now has " << connectionCountMap[(int)con->clientAddress.sin_addr.s_addr] << " connections.";
    connectionMap[name] = con;
    connectionIdMap[con->characterID] = con;
    ++userCount;
    if (userCount > maxUserCount)
        maxUserCount = userCount;
}

void ServerState::removeConnection(string& name) {
    if (connectionMap.find(name) != connectionMap.end()) {
        auto con = connectionMap[name];
        connectionIdMap.erase(con->characterID);
        int addr = (int) con->clientAddress.sin_addr.s_addr;
        connectionCountMap[addr] -= 1;
        //DLOG(INFO) << "IP " << addr << " now has " << connectionCountMap[addr] << " connections.";
        if (connectionCountMap[addr] <= 0) {
            //DLOG(INFO) << "IP " << addr << " has been removed because it no longer has any connections.";
            connectionCountMap.erase(addr);
        }
        // Need to remove staff call target if there is one, or connections get leaked..
        staffCallTargets.erase(connectionMap[name]);
        connectionMap.erase(name);
        --userCount;
    }
}

ConnectionPtr ServerState::getConnection(string& name) {
    if (connectionMap.find(name) != connectionMap.end())
        return connectionMap[name];

    return nullptr;
}

ConnectionPtr ServerState::getConnectionById(uint32_t id) {
    if (connectionIdMap.find(id) != connectionIdMap.end())
        return connectionIdMap[id];

    return nullptr;
}

const int ServerState::getConnectionIPCount(ConnectionPtr con) {
    return connectionCountMap[(int) con->clientAddress.sin_addr.s_addr];
}

string ServerState::generatePrivateChannelID(ConnectionPtr con, string& title) {
    while (true) {
        char buf[1024];
        bzero(&buf[0], sizeof (buf));
        int size = snprintf(&buf[0], sizeof (buf), "%s%s%f%ld", title.c_str(),
                con->characterName.c_str(), (float) Server::getEventTime(),
                ++ServerState::channelSeed);

        string namehash(&buf[0], size);
        bzero(&buf[0], sizeof (buf));
        namehash = thirdparty::SHA1HashString(namehash);
        snprintf(&buf[0], sizeof (buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                (unsigned char) namehash[0], (unsigned char) namehash[1],
                (unsigned char) namehash[2], (unsigned char) namehash[3],
                (unsigned char) namehash[4], (unsigned char) namehash[5],
                (unsigned char) namehash[6], (unsigned char) namehash[7],
                (unsigned char) namehash[8], (unsigned char) namehash[9]);
        namehash = string(&buf[0], 20);
        namehash = "ADH-" + namehash;
        string lname = namehash;
        int nameSize = lname.size();
        for (int i = 0; i < nameSize; ++i) {
            lname[i] = (char) tolower(lname[i]);
        }
        Channel* chan = ServerState::getChannel(lname).get();
        if (chan == nullptr) {
            return namehash;
        }
    }
}

void ServerState::addChannel(string& name, Channel* channel) {
    string lname = name;
    int size = lname.size();
    for (int i = 0; i < size; ++i) {
        lname[i] = (char) tolower(lname[i]);
    }
    ChannelPtr chan(channel);
    channelMap[lname] = chan;
}

void ServerState::removeChannel(string& name) {
    if (channelMap.find(name) != channelMap.end()) {
        channelMap.erase(name);
    }
}

ChannelPtr ServerState::getChannel(string& name) {
    if (channelMap.find(name) != channelMap.end())
        return channelMap[name];

    return 0;
}

void ServerState::addOp(string& op) {
    opList.insert(op);
}

void ServerState::removeOp(string& op) {
    opList.erase(op);
}

bool ServerState::isOp(string& op) {
    return opList.count(op) > 0;
}

void ServerState::addBan(string& character, long accountid) {
    banList[accountid] = character;
}

bool ServerState::removeBan(string& character) {
    for (banlist_t::iterator i = banList.begin(); i != banList.end(); ++i) {
        if (i->second == character) {
            banList.erase(i);
            return true;
        }
    }
    return false;
}

bool ServerState::isBanned(long accountid) {
    return banList.count(accountid) > 0;
}

void ServerState::addTimeout(string& character, long accountid, int length) {
    TimeoutRecord to;
    to.character = character;
    to.end = time(0) + length;
    timeoutList[accountid] = to;
}

void ServerState::removeTimeout(string& character) {
    for (timeoutmap_t::iterator i = timeoutList.begin(); i != timeoutList.end(); ++i) {
        if (i->second.character == character) {
            timeoutList.erase(i);
            return;
        }
    }
}

bool ServerState::isTimedOut(long accountid, int& end) {
    if (timeoutList.find(accountid) != timeoutList.end()) {
        end = timeoutList[accountid].end;
        return true;
    }

    return false;
}

void ServerState::addStaffCall(string& callid, StaffCallRecord& record) {
    staffCallList[callid] = record;
}

void ServerState::removeStaffCall(string& callid) {
    staffCallList.erase(callid);
}

StaffCallRecord ServerState::getStaffCall(string& callid) {
    if (staffCallList.find(callid) != staffCallList.end())
        return staffCallList[callid];
    StaffCallRecord record;
    record.action = "invalid";
    return record;
}

void ServerState::addStaffCallTarget(ConnectionPtr con) {
    staffCallTargets.insert(con);
}

void ServerState::removeStaffCallTarget(ConnectionPtr con) {
    staffCallTargets.erase(con);
}

void ServerState::rebuildChannelOpList() {
    channelOpList.clear();
    const chanptrmap_t chans = getChannels();
    for (chanptrmap_t::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        if (i->second->getType() == CT_PUBLIC) {
            ChannelPtr chan = i->second;
            const chmodmap_t mods = chan->getModRecords();
            for (chmodmap_t::const_iterator m = mods.begin(); m != mods.end(); ++m) {
                if (m->first != "") {
                    channelOpList.insert(m->first);
                }
            }
        }
    }
}

bool ServerState::isChannelOp(string& name) {
    return channelOpList.count(name) > 0;
}
