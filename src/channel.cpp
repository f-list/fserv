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
#include "channel.hpp"
#include "logging.hpp"

#include <time.h>

string Channel::privChanDescriptionDefault("Welcome to your private room! Invite friends with the [b]/invite[/b] command. Change this text with [b]/setdescription[/b]. You can open the room to the public with [b]/openroom[/b] For more, read [url=http://www.f-list.net/doc/chat_faq.php]the help[/url].");

Channel::Channel(string channame, ChannelType chantype)
:
LBase(),
name(channame),
description(""),
type(chantype),
chatMode(CMM_BOTH),
participantCount(0),
lastActivity(0),
canDestroy(true),
title(""),
topUsers(0),
refCount(0) {
    if (chantype == CT_PRIVATE) {
        description = privChanDescriptionDefault;
    }
}

Channel::Channel(string channame, ChannelType chantype, ConnectionPtr creator)
:
LBase(),
name(channame),
description(""),
type(chantype),
chatMode(CMM_BOTH),
participantCount(0),
lastActivity(0),
canDestroy(true),
title(""),
topUsers(0),
refCount(0) {
    invites.insert(creator->characterNameLower);
    owner = creator->characterName;
    description = privChanDescriptionDefault;
}

void intrusive_ptr_release(Channel* p) {
    if ((--p->refCount) <= 0) {
        delete p;
    }
}

void intrusive_ptr_add_ref(Channel* p) {
    ++p->refCount;
}

Channel::~Channel() {
    assert(participants.size() == 0);
}

void Channel::sendToAll(string& message) {
    MessagePtr outMessage(MessageBuffer::FromString(message));
    for (chconlist_t::iterator i = participants.begin(); i != participants.end(); ++i) {
        (*i)->send(outMessage);
    }
}

void Channel::sendToChannel(ConnectionPtr src, string& message) {
    MessagePtr outMessage(MessageBuffer::FromString(message));
    for (chconlist_t::iterator i = participants.begin(); i != participants.end(); ++i) {
        ConnectionPtr p = *i;
        if (p != src)
            p->send(outMessage);
    }
}

void Channel::join(ConnectionPtr con) {
    lastActivity = time(0);
    ++participantCount;
    participants.push_back(con);
    con->joinChannel(this);
    if (participantCount > topUsers)
        topUsers = participantCount;
}

void Channel::part(ConnectionPtr con) {
    lastActivity = time(0);
    --participantCount;
    timerMap.erase(con->characterNameLower);
    participants.remove(con);
    con->leaveChannel(this);
}

void Channel::kick(ConnectionPtr dest) {
    part(dest);
}

void Channel::ban(ConnectionPtr src, ConnectionPtr dest) {
    BanRecord ban;
    ban.banner = src->characterName;
    ban.time = time(0);
    ban.timeout = 0;
    bans[dest->characterNameLower] = ban;
}

void Channel::ban(ConnectionPtr src, string dest) {
    BanRecord ban;
    ban.banner = src->characterName;
    ban.time = time(0);
    ban.timeout = 0;
    bans[dest] = ban;
}

void Channel::timeout(ConnectionPtr src, ConnectionPtr dest, long length) {
    BanRecord ban;
    ban.banner = src->characterName;
    ban.time = time(0);
    ban.timeout = time(0) + length;
    bans[dest->characterNameLower] = ban;
}

void Channel::timeout(ConnectionPtr src, string dest, long length) {
    BanRecord ban;
    ban.banner = src->characterName;
    ban.time = time(0);
    ban.timeout = time(0) + length;
    bans[dest] = ban;
}

void Channel::unban(string& dest) {
    bans.erase(dest);
}

bool Channel::inChannel(ConnectionPtr con) {
    for (chconlist_t::iterator i = participants.begin(); i != participants.end(); ++i) {
        if (con == (*i))
            return true;
    }

    return false;
}

bool Channel::isBanned(ConnectionPtr con) {
    // no reason to rewrite the function: just pass it through
    // to the other overload
    return isBanned(con->characterNameLower);
}

bool Channel::isBanned(string& name) {
    chbanmap_t::const_iterator itr = bans.find(name);
    if (itr != bans.end()) {
        BanRecord b = itr->second;
        if (b.timeout == 0 || b.timeout >= time(0)) {
            return true;
        } else {
            bans.erase(name);
        }
    }

    return false;
}

bool Channel::getBan(ConnectionPtr con, BanRecord& ban) {
    return getBan(con->characterNameLower, ban);
}

bool Channel::getBan(string& name, BanRecord& ban) {
    chbanmap_t::const_iterator itr = bans.find(name);
    if (itr != bans.end()) {
        BanRecord tmp = itr->second;
        ban.banner = tmp.banner;
        ban.time = tmp.time;
        ban.timeout = tmp.timeout;
        return true;
    }
    return false;
}

void Channel::addMod(ConnectionPtr src, string& dest) {
    ModRecord mod;
    mod.modder = src->characterName;
    mod.time = time(0);
    moderators[dest] = mod;
}

void Channel::addMod(string& dest) {
    ModRecord mod;
    mod.modder = "[System]";
    mod.time = time(0);
    moderators[dest] = mod;
}

void Channel::remMod(string& dest) {
    moderators.erase(dest);
}

bool Channel::isMod(ConnectionPtr con) {
    if (con->globalModerator || con->admin || (owner == con->characterName) || moderators.find(con->characterName) != moderators.end())
        return true;

    return false;
}

bool Channel::isMod(string& name) {
    if ((owner == name) || moderators.find(name) != moderators.end())
        return true;

    return false;
}

bool Channel::isOnlyMod(ConnectionPtr con) const {
    return isOnlyMod(con->characterNameLower);
}

bool Channel::isOnlyMod(const string& name) const {
    return moderators.find(name) != moderators.end();
}

bool Channel::isOwner(ConnectionPtr con) {
    if (con->globalModerator || con->admin || (owner == con->characterName))
        return true;

    return false;
}

bool Channel::isOwner(string& name) {
    if (owner == name)
        return true;

    return false;
}

const double Channel::getTimerEntry(ConnectionPtr con) {
    if (timerMap.find(con->characterNameLower) != timerMap.end())
        return timerMap[con->characterNameLower];

    return -1;
}

void Channel::setTimerEntry(ConnectionPtr con, double newvalue) {
    timerMap[con->characterNameLower] = newvalue;
}

string Channel::getTypeString() {
    switch (type) {
        case CT_PRIVATE:
            return "private";
        case CT_PUBLIC:
        default:
            return "public";
        case CT_PUBPRIVATE:
            return "pubprivate";
    }
    return "public";
}

void Channel::invite(ConnectionPtr dest) {
    invites.insert(dest->characterNameLower);
}

void Channel::removeInvite(string& dest) {
    invites.erase(dest);
}

bool Channel::isInvited(ConnectionPtr con) {
    if (invites.find(con->characterNameLower) != invites.end())
        return true;

    return false;
}

void Channel::setPublic(bool newstatus) {
    if (newstatus)
        type = CT_PUBPRIVATE;
    else
        type = CT_PRIVATE;
}

json_t* Channel::saveChannel() {
    json_t* ret = json_object();
    json_object_set_new_nocheck(ret, "name",
            json_string_nocheck(name.c_str())
            );
    json_object_set_new_nocheck(ret, "description",
            json_string_nocheck(description.c_str())
            );
    json_object_set_new_nocheck(ret, "mode",
            json_string_nocheck(modeToString().c_str())
            );
    json_object_set_new_nocheck(ret, "owner",
            json_string_nocheck(owner.c_str())
            );
    json_object_set_new_nocheck(ret, "users",
            json_integer(participants.size())
            );
    json_object_set_new_nocheck(ret, "title",
            json_string_nocheck(title.c_str())
            );
    json_object_set_new_nocheck(ret, "top",
            json_integer(topUsers)
            );
    json_object_set_new_nocheck(ret, "type",
            json_string_nocheck(typeToString().c_str())
            );
    {
        json_t* bansnode = json_array();
        for (chbanmap_t::const_iterator i = bans.begin(); i != bans.end(); ++i) {
            json_t* ban = json_object();
            BanRecord br = (*i).second;
            json_object_set_new_nocheck(ban, "name",
                    json_string_nocheck((*i).first.c_str())
                    );
            json_object_set_new_nocheck(ban, "banner",
                    json_string_nocheck(br.banner.c_str())
                    );
            json_object_set_new_nocheck(ban, "timeout",
                    json_integer(br.timeout)
                    );
            json_object_set_new_nocheck(ban, "time",
                    json_integer(br.time)
                    );
            json_array_append_new(bansnode, ban);
        }
        json_object_set_new_nocheck(ret, "banlist", bansnode);
    }
    {
        json_t* mods = json_array();
        for (chmodmap_t::const_iterator i = moderators.begin(); i != moderators.end(); ++i) {
            json_t* mod = json_object();
            ModRecord mr = (*i).second;
            json_object_set_new_nocheck(mod, "name",
                    json_string_nocheck((*i).first.c_str())
                    );
            json_object_set_new_nocheck(mod, "modder",
                    json_string_nocheck(mr.modder.c_str())
                    );
            json_object_set_new_nocheck(mod, "time",
                    json_integer(mr.time)
                    );
            json_array_append_new(mods, mod);
        }
        json_object_set_new_nocheck(ret, "modlist", mods);
    }


    return ret;
}

void Channel::loadChannel(const json_t* channode) {
    lastActivity = time(0);
    {
        json_t* descnode = json_object_get(channode, "description");
        if (descnode) {
            const char* descstring = json_string_value(descnode);
            if (descstring)
                description = descstring;
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no description item.";
        }
    }

    {
        json_t* modenode = json_object_get(channode, "mode");
        if (modenode) {
            const char* modestring = json_string_value(modenode);
            if (modestring)
                chatMode = stringToMode(modestring);
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no mode item.";
        }
    }

    {
        json_t* ownernode = json_object_get(channode, "owner");
        if (ownernode) {
            const char* ownerstring = json_string_value(ownernode);
            if (ownerstring)
                owner = ownerstring;
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no owner item.";
        }
    }

    {
        json_t* bansnode = json_object_get(channode, "banlist");
        if (json_is_array(bansnode)) {
            int bansize = json_array_size(bansnode);
            for (int l = 0; l < bansize; ++l) {
                json_t* ban = json_array_get(bansnode, l);
                if (!ban) {
                    LOG(WARNING) << "Calculation error in ban list loop. l:" << l;
                    break;
                }
                BanRecord b;

                {
                    json_t* bannernode = json_object_get(ban, "banner");
                    if (bannernode) {
                        const char* bannerstring = json_string_value(bannernode);
                        if (bannerstring)
                            b.banner = bannerstring;
                    } else {
                        LOG(WARNING) << "Ban json for channel " << name << " contains no banner item.";
                        continue;
                    }
                }

                {
                    json_t* timenode = json_object_get(ban, "time");
                    if (timenode) {
                        b.time = json_integer_value(timenode);
                    } else {
                        LOG(WARNING) << "Ban json for channel " << name << " contains no time item.";
                        continue;
                    }
                }

                {
                    json_t* timeoutnode = json_object_get(ban, "timeout");
                    if (timeoutnode) {
                        b.timeout = json_integer_value(timeoutnode);
                    } else {
                        b.timeout = 0;
                        LOG(WARNING) << "Ban json for channel " << name << " contains no timeout item.";
                    }
                }

                {
                    json_t* namenode = json_object_get(ban, "name");
                    if (namenode) {
                        const char* namestring = json_string_value(namenode);
                        if (namestring) {
                            string lowername = namestring;
                            int len = lowername.length();
                            for (int i = 0; i < len; ++i) {
                                lowername[i] = (char) tolower(lowername[i]);
                            }
                            bans[lowername] = b;
                        }
                    } else {
                        LOG(WARNING) << "Ban json for channel " << name << " contains no name item.";
                        continue;
                    }
                }
            }
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no banlist item.";
        }
    }

    {
        json_t* modsnode = json_object_get(channode, "modlist");
        if (json_is_array(modsnode)) {
            int modsize = json_array_size(modsnode);
            for (int l = 0; l < modsize; ++l) {
                json_t* mod = json_array_get(modsnode, l);
                if (!mod) {
                    LOG(WARNING) << "Calculation error in mod list loop. l:" << l;
                    break;
                }
                ModRecord m;

                {
                    json_t* moddernode = json_object_get(mod, "modder");
                    if (moddernode) {
                        const char* modderstring = json_string_value(moddernode);
                        if (modderstring) {
                            m.modder = modderstring;
                        } else {
                            m.modder = "";
                        }
                    } else {
                        LOG(WARNING) << "Mod json for channel " << name << " contains no modder item.";
                        continue;
                    }
                }

                {
                    json_t* timenode = json_object_get(mod, "time");
                    if (timenode) {
                        m.time = json_integer_value(timenode);
                    } else {
                        LOG(WARNING) << "Mod json for channel " << name << " contains no time item.";
                        continue;
                    }
                }

                {
                    json_t* namenode = json_object_get(mod, "name");
                    if (namenode) {
                        const char* namestring = json_string_value(namenode);
                        if (namestring) {
                            moderators[namestring] = m;
                        }
                    } else {
                        LOG(WARNING) << "Mod json for channel " << name << " contains no name item.";
                        continue;
                    }
                }
            }
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no banlist item.";
        }
    }

    {
        json_t* titlenode = json_object_get(channode, "title");
        if (titlenode) {
            const char* titlestring = json_string_value(titlenode);
            if (titlestring)
                title = titlestring;
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no title item.";
        }
    }

    {
        json_t* topnode = json_object_get(channode, "top");
        if (topnode) {
            topUsers = json_integer_value(topnode);
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no top item.";
        }
    }

    {
        json_t* typenode = json_object_get(channode, "type");
        if (typenode) {
            const char* typestring = json_string_value(typenode);
            if (typestring) {
                type = stringToType(typestring);
            } else {
                LOG(WARNING) << "Channel json for " << name << " fails to provide a string for a type.";
            }
        } else {
            LOG(WARNING) << "Channel json for " << name << " contains no type item.";
        }
    }
}

string Channel::modeToString() {
    switch (chatMode) {
        default:
        case CMM_BOTH:
            return "both";
        case CMM_ADS_ONLY:
            return "ads";
        case CMM_CHAT_ONLY:
            return "chat";
    }
    return "both";
}

ChannelMessageMode Channel::stringToMode(string modestring) {
    if (modestring == "both")
        return CMM_BOTH;
    else if (modestring == "ads")
        return CMM_ADS_ONLY;
    else if (modestring == "chat")
        return CMM_CHAT_ONLY;

    return CMM_BOTH;
}

string Channel::typeToString() {
    switch (type) {
        default:
        case CT_PRIVATE:
            return "private";
        case CT_PUBPRIVATE:
            return "pubprivate";
        case CT_PUBLIC:
            return "public";
    }
    return "private";
}

ChannelType Channel::stringToType(string typestring) {
    if (typestring == "public")
        return CT_PUBLIC;
    else if (typestring == "pubprivate")
        return CT_PUBPRIVATE;
    else
        return CT_PRIVATE;
}
