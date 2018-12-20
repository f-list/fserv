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

#ifndef CHANNEL_H
#define CHANNEL_H

#include <boost/intrusive_ptr.hpp>
#include <boost/functional/hash.hpp>
#include <string>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include "connection.hpp"
#include "fjson.hpp"
#include "lua_base.hpp"

using std::string;
using std::tr1::unordered_map;
using std::tr1::unordered_set;
using std::list;
using boost::intrusive_ptr;

enum ChannelType {
    CT_PUBLIC, //Global public channels.
    CT_PRIVATE, //Invite only channels.
    CT_PUBPRIVATE, //Public channels that are privately run.
    CT_MAX
};

enum ChannelMessageMode {
    CMM_CHAT_ONLY,
    CMM_ADS_ONLY,
    CMM_BOTH,
    CMM_MAX
};

typedef struct {
    string modder;
    time_t time;
} ModRecord;

typedef struct {
    string banner;
    time_t time;
    time_t timeout;
} BanRecord;

typedef unordered_set<ConnectionPtr, boost::hash<ConnectionPtr> > chconlist_t;
typedef unordered_set<string> chstringset_t;
typedef unordered_map<string, BanRecord> chbanmap_t;
typedef unordered_map<string, ModRecord> chmodmap_t;
typedef unordered_map<string, double> chtimermap_t;

class Channel : public LBase {
public:
    Channel(string channame, ChannelType chantype);
    Channel(string channame, ChannelType chantype, ConnectionInstance* creator);
    virtual ~Channel();

    void sendToAll(string& message); //Sends to everyone, including source.
    void sendToChannel(ConnectionInstance* src, string& message); //Sends to everyone, excluding source.

    void join(ConnectionInstance* con);
    void part(ConnectionInstance* con);
    bool inChannel(ConnectionInstance* con);

    void kick(ConnectionInstance* dest);
    void ban(ConnectionInstance* src, ConnectionInstance* dest);
    void ban(ConnectionInstance* src, string dest);
    void timeout(ConnectionInstance* src, ConnectionInstance* dest, long length);
    void timeout(ConnectionInstance* src, string dest, long length);
    void unban(string& dest);
    bool isBanned(ConnectionInstance* con);
    bool isBanned(string& name);
    bool getBan(ConnectionInstance* con, BanRecord& ban);
    bool getBan(string& name, BanRecord& ban);
    void cleanExpiredTimeouts();

    const chbanmap_t& getBanRecords() const {
        return bans;
    }

    const size_t getBanCount() {
        cleanExpiredTimeouts();
        return bans.size();
    }

    void addMod(ConnectionInstance* src, string& dest);
    void addMod(string& dest);
    void remMod(string& dest);

    const chmodmap_t& getModRecords() const {
        return moderators;
    }
    bool isMod(ConnectionInstance* con);
    bool isMod(string& name);

    const string& getOwner() const {
        return owner;
    }
    bool isOwner(ConnectionInstance* con);
    bool isOwner(string& name);

    void setOwner(string& name) {
        owner = name;
    }

    const string& getDescription() const {
        return description;
    }

    void setDescription(string& newdesc) {
        description = newdesc;
    }

    const string& getName() const {
        return name;
    }

    const double getTimerEntry(ConnectionInstance* con);
    void setTimerEntry(ConnectionInstance* con, double newvalue);

    string getTypeString();

    const ChannelType getType() const {
        return type;
    }

    const string getModeString() {
        return modeToString();
    }

    void setMode(ChannelMessageMode newmode) {
        chatMode = newmode;
    }

    const int getParticipantCount() const {
        return participantCount;
    }

    void updateParticipantCount() {
        participantCount = participants.size();
    }

    const chconlist_t& getParticipants() const {
        return participants;
    }

    const time_t getLastActivity() const {
        return lastActivity;
    }

    bool getCanDestroy() const {
        return canDestroy;
    }

    void setCanDestroy(bool destroyable) {
        canDestroy = destroyable;
    }

    void invite(ConnectionInstance* dest);
    void removeInvite(string& dest);
    bool isInvited(ConnectionInstance* con);

    void setPublic(bool newstatus);

    const string& getTitle() const {
        return title;
    }

    void setTitle(string newtitle) {
        title = newtitle;
    }

    const int getTopUserCount() const {
        return topUsers;
    }

    json_t* saveChannel();
    void loadChannel(const json_t* channode);

protected:
    string modeToString();
    ChannelMessageMode stringToMode(string modestring);
    string typeToString();
    ChannelType stringToType(string typestring);
    string name;
    string description;
    ChannelType type;
    ChannelMessageMode chatMode;
    chconlist_t participants;
    chmodmap_t moderators;
    string owner;
    chbanmap_t bans;
    int participantCount;
    chtimermap_t timerMap;
    time_t lastActivity;
    bool canDestroy;

    string title;
    chstringset_t invites;
    int topUsers;

    int refCount;

    friend inline void intrusive_ptr_release(Channel* p)
    {
        if (__sync_sub_and_fetch(&p->refCount, 1) <= 0) {
            delete p;
        }
    }
    friend inline void intrusive_ptr_add_ref(Channel* p) { __sync_fetch_and_add(&p->refCount, 1); }
private:
    static string privChanDescriptionDefault;
};

typedef intrusive_ptr<Channel> ChannelPtr;

#endif //CHANNEL_H
