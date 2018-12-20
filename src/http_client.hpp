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

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <queue>
#include <string>
#include <atomic>
#include <tr1/unordered_map>
#include "fthread.hpp"
#include "logging.hpp"
#include "connection.hpp"
#include <ev.h>
#include <curl/curl.h>

using std::queue;
using std::string;
using std::tr1::unordered_map;

class HTTPReply;

typedef void (* httpCodeCallback)(HTTPReply*);

class HTTPReply {
public:
    HTTPReply() : rawError(0), _status(499), _success(false) {}

    void append(const char* input, size_t length) {
        _body.append(input, length);
    }

    string &body() {
        return _body;
    }

    void success(bool has_error) {
        _success = has_error;
    }

    bool success() {
        return _success;
    }

    void status(int status) {
        _status = status;
    }

    int status() {
        return _status;
    }

    unordered_map<string, string> &extras() {
        return _extras;
    };

    void extras(unordered_map<string, string> &extras) {
        _extras = extras;
    }

    void connection(ConnectionPtr connection) {
        _connection = connection;
    }

    const ConnectionPtr& connection() const {
        return _connection;
    }

    void callback(string &callback) {
        _callbackName = callback;
    }

    string &callback() {
        return _callbackName;
    }

    int rawError;
private:
    ConnectionPtr _connection;
    string _body;
    int _status;
    bool _success;
    string _callbackName;
    unordered_map<string, string> _extras;
};

class HTTPRequest {
public:
    HTTPRequest() : _customHeaders(nullptr),
                    _reply(nullptr),
                    loop(nullptr),
                    ioEvent(nullptr),
                    _curlHandle(nullptr),
                    _codeCallback(nullptr) {}

    ~HTTPRequest() {
        if (_customHeaders)
            curl_slist_free_all(_customHeaders);
        stopIO();
    }

    void setIO(struct ev_loop* _loop, ev_io* _io) {
        loop = _loop;
        ioEvent = _io;
    }

    void stopIO() {
        DLOG(INFO) << "Stopping io on request.";
        if (ioEvent) {
            ev_io_stop(loop, ioEvent);
            delete ioEvent;
            ioEvent = 0;
        }
    }

    void setIOWant(int socket, int what) {
        ev_io_stop(loop, ioEvent);
        ev_io_set(ioEvent, socket, what);
        ev_io_start(loop, ioEvent);
    }

    string &method() {
        return _method;
    }

    void method(string method) {
        _method = method;
    }

    string &url() {
        return _url;
    }

    void url(string &url) {
        _url = url;
    }

    void headerAdd(const char* header) {
        _customHeaders = curl_slist_append(_customHeaders, header);
    }

    unordered_map<string, string> &extras() {
        return _extras;
    };

    void extras(unordered_map<string, string> &extras) {
        _extras = extras;
    }

    void connection(intrusive_ptr <ConnectionInstance> connection) {
        _connection = connection;
    }

    intrusive_ptr <ConnectionInstance> connection() {
        return _connection;
    }

    CURL* curlHandle() {
        return _curlHandle;
    }

    void curlHandle(CURL* handle) {
        _curlHandle = handle;
    }

    HTTPReply* reply() {
        if (_reply == nullptr) {
            _reply = new HTTPReply();
            _reply->status(499);
            _reply->extras(_extras);
            _reply->callback(_callbackName);
            if (_connection) {
                _reply->connection(_connection);
            }
        }
        return _reply;
    }

    void postField(string name, string value);

    void postField(string &name, string &value);

    void reply(HTTPReply* reply) {
        _reply = reply;
    }

    string &postString();

    string &callbackName() {
        return _callbackName;
    }

    void callbackName(string &callback) {
        _callbackName = callback;
    }

    httpCodeCallback codeCallback() {
        return _codeCallback;
    }

    void codeCallback(httpCodeCallback callback) {
        _codeCallback = callback;
    }

private:
    intrusive_ptr <ConnectionInstance> _connection;
    string _method;
    string _url;
    unordered_map<string, string> _postValues;
    struct curl_slist* _customHeaders;
    unordered_map<string, string> _extras;

    string _callbackName;

    string materializedPost;

    HTTPReply* _reply;

    struct ev_loop* loop;
    ev_io* ioEvent;
    CURL* _curlHandle;
    httpCodeCallback _codeCallback;
};

class HTTPClient {
public:
    static void* runThread(void* param);

    static bool addRequest(HTTPRequest* request);

    static HTTPReply* getReply();

    static void stopThread() {
        doRun = false;
        ev_async_send(loop, async);
        LOG(INFO) << "Stopping HTTP thread.";
    }

    static void sendWakeup();

    static bool escapeSegment(string &input) {
        const char* result = curl_easy_escape(escapeHandle, input.c_str(), input.length());
        if (result) {
            input.assign(result);
            curl_free((void*) result);
            return true;
        }
        return false;
    }

private:
    static void addReply(HTTPReply* reply);

    static void processRequest(HTTPRequest* request);

    static void processResponseDone(HTTPRequest* request, CURLcode result);

    static void checkForFinishedRequests();

    static HTTPRequest* getRequest();

    static void init();

    static void processQueue(struct ev_loop* loop, ev_async* w, int events);

    static void timeoutCallback(struct ev_loop* loop, ev_timer* w, int events);

    static void readwriteCallback(struct ev_loop* loop, ev_io* w2, int events);

    static int
    curlSocketCallback(CURL* easy, curl_socket_t socket, int what, HTTPRequest* socketRequest, HTTPRequest* request);

    static int curlTimerCallback(CURLM* multi, long timeoutMS, void* userp);

    static size_t curlWriteCallback(void* data, size_t size, size_t count, HTTPRequest* request);

    static queue<HTTPRequest*> requestQueue;
    static queue<HTTPReply*> replyQueue;
    static pthread_mutex_t requestMutex;
    static pthread_mutex_t replyMutex;

    static CURLM* multiHandle;
    static CURL* escapeHandle;
    static struct ev_loop* loop;
    static ev_timer* timer;
    static ev_async* async;
    static std::atomic<bool> doRun;

};

#endif //HTTP_REQUEST_H