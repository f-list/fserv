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

#include "native_command.hpp"
#include "logging.hpp"
#include "connection.hpp"
#include "fjson.hpp"
#include "login_evhttp.hpp"
#include "server.hpp"
#include "startup_config.hpp"
#include "server_state.hpp"

#include <google/profiler.h>

FReturnCode NativeCommand::DebugCommand(ConnectionPtr& con, string& payload) {
    if (con->admin != true)
        return FERR_NOT_ADMIN;

    json_t* topnode = json_loads(payload.c_str(), 0, 0);
    if (!topnode)
        return FERR_BAD_SYNTAX;

    DLOG(INFO) << "Debug event with payload: " << payload;
    json_t* cmdnode = json_object_get(topnode, "command");
    if (!json_is_string(cmdnode)) {
        json_decref(topnode);
        return FERR_BAD_SYNTAX;
    }

    string command = json_string_value(cmdnode);
    if (command == "isolate") {
        if (con->debugL) {
            con->sendDebugReply("Your connection is already running an isolated Lua state.");
        } else {
            string luamessage;
            FReturnCode isolateret = con->isolateLua(luamessage);
            if (isolateret == FERR_OK) {
                con->sendDebugReply("Your connection is now running an isolated Lua state.");
            } else if (isolateret == FERR_LUA) {
                string errmsg = "Failed to move your connection into an isolated Lua state. Lua returned the error: " + luamessage;
                con->sendDebugReply(errmsg);
            } else {
                con->sendDebugReply("Failed to move your connection into an isolated Lua state. An unknown error happened.");
            }
        }
    } else if (command == "deisolate") {
        if (!con->debugL) {
            con->sendDebugReply("Your connection is not currently running an isolated Lua state.");
        } else {
            con->deisolateLua();
            con->sendDebugReply("Your connection has been returned to the global Lua state.");
        }
    } else if (command == "profile-start") {
        ProfilerStart("./cpu.out");
    } else if (command == "profile-stop") {
        ProfilerStop();
    } else if (command == "logger-start") {
        Server::loggerStart();
    } else if (command == "logger-stop") {
        Server::loggerStop();
    } else if (command == "status") {
        //TODO: Make this command do something.
        string statusmessage = "Status: ";
        if (con->debugL)
            statusmessage += "Running isolated. ";
        //statusmessage += "Connected from: ";
        con->sendDebugReply(statusmessage);
    } else if (command == "reload") {
        if (con->debugL) {
            string luamessage;
            FReturnCode reloadret = con->reloadIsolation(luamessage);
            if (reloadret == FERR_OK) {
                con->sendDebugReply("The isolated Lua state for your connection is now running the latest on disk Lua.");
            } else if (reloadret == FERR_LUA) {
                string errormsg = "It was not possible to reload the isolated Lua state for your connection because"
                        " of a Lua error. Lua returned the error: " + luamessage + "\nYou are still running the previous version.";
                con->sendDebugReply(errormsg);
            } else {
                con->sendDebugReply("It was not possible to reload the isolated Lua state for your connection because of an unknown error.\n"
                        "You are still running the previous version."
                        );
            }
        } else {
            json_t* argnode = json_object_get(topnode, "arg");
            if (!json_is_string(argnode)) {
                json_decref(topnode);
                return FERR_BAD_SYNTAX;
            }
            string arg = json_string_value(argnode);
            if (arg == "yes") {
                string luamessage;
                FReturnCode reloadret = Server::reloadLuaState(luamessage);
                if (reloadret == FERR_OK) {
                    con->sendDebugReply("The global Lua state has been reloaded.");
                } else if (reloadret == FERR_LUA) {
                    string errormsg = "It was not possible to reload the global Lua state because of a Lua error. Lua returned the error: "
                            + luamessage + "\nEveryone is still running the previous version.";
                    con->sendDebugReply(errormsg);
                } else {
                    con->sendDebugReply("It was not possible to reload the global Lua state because of an unknown error.\n"
                            "Everyone is still running the previous version."
                            );
                }
            } else {
                con->sendDebugReply("You must confirm reloading the global Lua state by providing the argument of \"yes\".");
            }
        }
    } else if (command == "halt") {
        Server::startShutdown();
    } else if (command == "help") {
        con->sendDebugReply(
                "Possible commands are: help, status, isolate, deisolate, reload, halt\n"
                "help: Prints this message.\n"
                "status: Prints information about your connection, including if you are in an isolated Lua state.\n"
                "isolate: Attempts to place your connection in an isolated Lua state. Returns a detailed error message upon failure.\n"
                "deisolate: Removes your connection from an isolated Lua state if you are in one. Returns a detailed error message upon failure.\n"
                "reload: Reloads the Lua code for a state. In a global state you are required to confirm reloading. Returns a detailed error message upon failure.\n"
                "halt: This will immediately bring the server down after saving important data."
                );
    } else {
        con->sendDebugReply("Unknown debug command. Try 'help'?");
    }

    json_decref(topnode);
    return FERR_OK;
}

FReturnCode NativeCommand::IdentCommand(ConnectionPtr& con, string& payload) {
    LOG(INFO) << "Ident command with payload: " << payload;

    if (con->identified || con->authStarted)
        return FERR_ALREADY_IDENT;

    con->authStarted = true;

    if (ServerState::getConnectionCount() >= StartupConfig::getDouble("maxusers"))
        return FERR_SERVER_FULL;

    json_t* tempnode = nullptr;
    json_t* topnode = json_loads(payload.c_str(), 0, 0);
    if (!topnode)
        return FERR_BAD_SYNTAX;

    LoginRequest* request = new LoginRequest;
    request->connection = con;

    json_t* methodnode = json_object_get(topnode, "method");
    if (!json_is_string(methodnode)) {
        json_decref(topnode);
        delete request;
        return FERR_BAD_SYNTAX;
    }

    string method = json_string_value(methodnode);
    if (method == "ticket") {
        request->method = LOGIN_METHOD_TICKET;
        tempnode = json_object_get(topnode, "account");
        if (!json_is_string(tempnode))
            goto fail;
        request->account = json_string_value(tempnode);
        tempnode = json_object_get(topnode, "ticket");
        if (!json_is_string(tempnode))
            goto fail;
        request->ticket = json_string_value(tempnode);
        tempnode = json_object_get(topnode, "character");
        if (!json_is_string(tempnode))
            goto fail;
        request->characterName = json_string_value(tempnode);
        tempnode = json_object_get(topnode, "cname");
        if(!json_is_string(tempnode))
            goto fail;
        request->clientName = json_string_value(tempnode);
        tempnode = json_object_get(topnode, "cversion");
        if(!json_is_string(tempnode))
            goto fail;
        request->clientVersion = json_string_value(tempnode);
        tempnode = nullptr;
    } else {
        json_decref(topnode);
        delete request;
        return FERR_UNKNOWN_AUTH_METHOD;
    }

    if (!LoginEvHTTPClient::addRequest(request)) {
        json_decref(topnode);
        delete request;
        return FERR_NO_LOGIN_SLOT;
    }

    LoginEvHTTPClient::sendWakeup();
    json_decref(topnode);
    return FERR_OK;

fail:
    json_decref(topnode);
    delete request;
    return FERR_BAD_SYNTAX;
}

void SearchFilterList(const json_t* node, unordered_set<ConnectionPtr>& conlist, string item) {
    unordered_set<string> items;
    size_t size = json_array_size(node);
    for (size_t i = 0; i < size; ++i) {
        json_t* jn = json_array_get(node, i);
        if (json_is_string(jn))
            items.insert(json_string_value(jn));
    }

    for (auto i = conlist.begin(); i != conlist.end();) {
        if (items.find((*i)->infotagMap[item]) == items.end())
            i = conlist.erase(i);
        else
            ++i;
    }
}

void SearchFilterListF(const json_t* node, unordered_set<ConnectionPtr>& conlist) {
    list<int> items;
    size_t size = json_array_size(node);
    for (size_t i = 0; i < size; ++i) {
        json_t* jn = json_array_get(node, i);
        if (json_is_string(jn)) {
            items.push_back((int) atoi(json_string_value(jn)));
        } else if (json_is_integer(jn)) {
            items.push_back((int) json_integer_value(jn));
        }
    }

    for (auto i = conlist.begin(); i != conlist.end();) {
        bool found = true;
        for (list<int>::const_iterator n = items.begin(); n != items.end(); ++n) {
            if ((*i)->kinkList.find((*n)) == (*i)->kinkList.end()) {
                found = false;
                break;
            }
        }

        if (found)
            ++i;
        else
            i = conlist.erase(i);
    }
}

FReturnCode NativeCommand::SearchCommand(intrusive_ptr< ConnectionInstance >& con, string& payload) {
    //DLOG(INFO) << "Starting search with payload " << payload;
    static string FKSstring("FKS");
    static double timeout = 5.0;
    double time = Server::getEventTime();
    if (con->timers[FKSstring] > (time - timeout))
        return FERR_THROTTLE_SEARCH;
    else
        con->timers[FKSstring] = time;

    typedef unordered_set<ConnectionPtr> clist_t;
    clist_t tosearch;
    const conptrmap_t cons = ServerState::getConnections();
    for (conptrmap_t::const_iterator i = cons.begin(); i != cons.end(); ++i) {
        if ((i->second != con) && (i->second->kinkList.size() != 0) && (i->second->status == "online" || i->second->status == "looking"))
            tosearch.insert(i->second);
    }

    json_t* rootnode = json_loads(payload.c_str(), 0, 0);
    if (!rootnode)
        return FERR_BAD_SYNTAX;
    json_t* kinksnode = json_object_get(rootnode, "kinks");
    if (!json_is_array(kinksnode))
        return FERR_BAD_SYNTAX;

    if (json_array_size(kinksnode) > 5)
        return FERR_TOO_MANY_SEARCH_TERMS;

    json_t* gendersnode = json_object_get(rootnode, "genders");
    if (json_is_array(gendersnode))
        SearchFilterList(gendersnode, tosearch, "Gender");

    json_t* orientationsnode = json_object_get(rootnode, "orientations");
    if (json_is_array(orientationsnode))
        SearchFilterList(orientationsnode, tosearch, "Orientation");

    json_t* languagesnode = json_object_get(rootnode, "languages");
    if (json_is_array(languagesnode))
        SearchFilterList(languagesnode, tosearch, "Language preference");

    json_t* furryprefsnode = json_object_get(rootnode, "furryprefs");
    if (json_is_array(furryprefsnode))
        SearchFilterList(furryprefsnode, tosearch, "Furry preference");

    json_t* rolesnode = json_object_get(rootnode, "roles");
    if (json_is_array(rolesnode))
        SearchFilterList(rolesnode, tosearch, "Dom/Sub Role");

    json_t* positionsnode = json_object_get(rootnode, "positions");
    if (json_is_array(positionsnode))
        SearchFilterList(positionsnode, tosearch, "Position");

    if (json_array_size(kinksnode) > 0)
        SearchFilterListF(kinksnode, tosearch);

    int num_found = tosearch.size();
    if (num_found == 0)
        return FERR_NO_SEARCH_RESULTS;
    else if (num_found > 350)
        return FERR_TOO_MANY_SEARCH_RESULTS;

    json_t* newroot = json_object();
    json_t* chararray = json_array();
    for (clist_t::const_iterator i = tosearch.begin(); i != tosearch.end(); ++i) {
        json_array_append_new(chararray,
                json_string_nocheck((*i)->characterName.c_str())
                );
    }
    json_object_set_new_nocheck(newroot, "characters", chararray);
    json_object_set_new_nocheck(newroot, "kinks", kinksnode);
    string message("FKS ");
    const char* fksstr = json_dumps(newroot, JSON_COMPACT);
    message += fksstr;
    free((void*) fksstr);
    json_decref(newroot);
    MessagePtr outMessage(MessageBuffer::fromString(message));
    con->send(outMessage);
    json_decref(rootnode);
    //DLOG(INFO) << "Finished search.";
    return FERR_OK;
}
