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

#ifndef CONNECTION_H
#define CONNECTION_H

#include <boost/intrusive_ptr.hpp>
#include <boost/functional/hash.hpp>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <string>
#include <deque>
#include <ev.h>
#include <netinet/in.h>
#include "websocket.hpp"
#include "ferror.hpp"
#include "lua_base.hpp"
#include "messagebuffer.hpp"

using std::string;
using std::tr1::unordered_map;
using std::tr1::unordered_set;
using std::deque;
using boost::intrusive_ptr;

struct lua_State;

class Channel;


typedef unordered_set< intrusive_ptr<Channel>, boost::hash< intrusive_ptr<Channel> > > chanlist_t;
typedef unordered_set<int> intlist_t;
typedef unordered_set<string> stringset_t;
typedef unordered_map<string, string> stringmap_t;
typedef unordered_map<string, double> timermap_t;
typedef deque<MessagePtr> messagelist_t;

class ConnectionInstance : public LBase {
public:
    ConnectionInstance();
    ~ConnectionInstance();

    bool send(MessagePtr message);
    bool sendRaw(string& message);
    void sendError(int error);
    void sendError(int error, string message);
    void sendDebugReply(string message);

    void setDelayClose();

    void leaveChannel(Channel* channel);
    void joinChannel(Channel* channel);

    FReturnCode reloadIsolation(string& output);
    FReturnCode isolateLua(string& output);
    void deisolateLua();

    const stringset_t& getFriends() const {
        return friends;
    }

    const stringset_t& getIgnores() const {
        return ignores;
    }

    const stringmap_t& getCustomKinks() const {
        return customKinkMap;
    }

    const stringmap_t& getInfoTags() const {
        return infotagMap;
    }

    const stringmap_t& getMiscData() const {
        return miscMap;
    }

public:
    long accountID;
    long characterID;
    string characterName;
    string characterNameLower;
    bool authStarted;
    bool identified;
    bool admin;
    bool globalModerator;
    ProtocolVersion protocol;
    struct sockaddr_in clientAddress;
    bool closed;
    bool delayClose;

    string statusMessage;
    string status;
    string gender;
    stringset_t friends;
    stringset_t ignores;

    chanlist_t channelList;

    stringmap_t miscMap;
    stringmap_t customKinkMap;
    stringmap_t infotagMap;
    intlist_t kinkList;


    //Buffers
    string readBuffer;
    messagelist_t writeQueue;
    size_t writePosition;

    //Timers
    timermap_t timers;

    //Event loop items
    struct ev_loop* loop;
    ev_timer* pingEvent;
    ev_timer* timerEvent;
    ev_io* readEvent;
    ev_io* writeEvent;
    ev_tstamp lastActivity;

    //Lua
    struct lua_State* debugL;

protected:
    int refCount;

    friend inline void intrusive_ptr_release(ConnectionInstance* p)
    {
        if (__sync_sub_and_fetch(&p->refCount, 1) <= 0) {
            delete p;
        }
    }
    friend inline void intrusive_ptr_add_ref(ConnectionInstance* p) { __sync_fetch_and_add(&p->refCount, 1); }
};

typedef intrusive_ptr<ConnectionInstance> ConnectionPtr;

namespace std {
    namespace tr1 {
        template <>
        struct hash<ConnectionPtr> : public unary_function<ConnectionPtr, size_t> {
            size_t operator()(const ConnectionPtr& v) const {
                return reinterpret_cast<size_t>(v.get());
            }
        };
    }
}

#endif //CONNECTION_H
