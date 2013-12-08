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

#ifndef REDIS_H
#define REDIS_H

#include <queue>
#include <string>
#include "fthread.hpp"
#include <hiredis/hiredis.h>

using std::string;
using std::queue;

#define REDIS_MUTEX_TIMEOUT 250000000

enum RedisMethod
{
	REDIS_DEL,
	REDIS_LPUSH,
	REDIS_LREM,
	REDIS_SADD,
	REDIS_SREM,
	REDIS_SET,
	REDIX_MAX
};

enum RedisUpdateContext
{
	RCONTEXT_IGNORE,
	RCONTEXT_ONLINE,
	RCONTEXT_MAX
};

class RedisRequest
{
public:
	string key;
	queue<string> values;
	RedisMethod method;
	RedisUpdateContext updateContext;
};

class Redis
{
public:
	static void* runThread(void* param);
	static bool addRequest(RedisRequest* newRequest);

	static void stopThread() { doRun = false; }
	static bool isRunning() { return doRun; }

	static pthread_mutex_t requestMutex;
	static const string onlineUsersKey;
	static const string lastCheckinKey;
private:
	Redis() {}
	~Redis() {}

	static void connectToRedis();
	static void sendInitialUserList();

	static RedisRequest* getRequest();

	static void processRequest(RedisRequest* req);

	static void timeoutCallback(struct ev_loop* loop, ev_timer* w, int revents);

	static redisContext* context;

	static queue<RedisRequest*> requestQueue;
	static bool doRun;
	static struct ev_loop* redis_loop;
	static ev_timer* redis_timer;
	static const __useconds_t REDIS_FAILURE_WAIT;
};

#endif //REDIS_H
