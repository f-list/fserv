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

#include "redis.hpp"

#include "logging.hpp"
#include "startup_config.hpp"
#include <time.h>


pthread_mutex_t Redis::requestMutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t Redis::replyMutex = PTHREAD_MUTEX_INITIALIZER;
queue<RedisRequest*> Redis::requestQueue;
bool Redis::doRun = true;
struct ev_loop* Redis::redis_loop = 0;
ev_timer* Redis::redis_timer = 0;
redisContext* Redis::context = 0;
const string Redis::onlineUsersKey("users.online");
const string Redis::lastCheckinKey("chat.lastCheckin");

const __useconds_t Redis::REDIS_FAILURE_WAIT = 5000000; //5 seconds;

void Redis::connectToRedis() {
    DLOG(INFO) << "Connecting to redis.";
    if (context)
        redisFree(context);
    context = 0;

    static struct timeval contimeout;
    contimeout.tv_sec = 10;
    contimeout.tv_usec = 0;
    context = redisConnectWithTimeout(StartupConfig::getString("redishost").c_str(), static_cast<int> (StartupConfig::getDouble("redisport")), contimeout);
    if (context->err) {
        DLOG(INFO) << "Failed to connect to redis with error: " << context->errstr;
        redisFree(context);
        context = 0;
        return;
    }
    redisSetTimeout(context, contimeout);
    redisReply* reply = (redisReply*) redisCommand(context, "AUTH %s", StartupConfig::getString("redispass").c_str());
    if (reply == 0 || (reply->type != REDIS_REPLY_STATUS && reply->len != 2)) {
        DLOG(INFO) << "Failed to authenticate with the redis server.";
        if (reply)
            freeReplyObject(reply);
        reply = 0;
        redisFree(context);
        context = 0;
        return;
    }
    freeReplyObject(reply);
}

void Redis::processRequest(RedisRequest* req) {
    while (doRun && !context) {
        usleep(REDIS_FAILURE_WAIT);
        connectToRedis();
    }

    if (doRun && context) {
        const RedisMethod method = req->method;
        switch (method) {
            case REDIS_DEL:
            {
                redisReply* reply = (redisReply*) redisCommand(context, "DEL %s", req->key.c_str());
                if (reply)
                    freeReplyObject(reply);
                else {
                    DLOG(WARNING) << "A redis command failed. Restarting the redis system.";
                    redisFree(context);
                    context = 0;
                    delete req;
                    return;
                }
                break;
            }
            case REDIS_SADD:
            case REDIS_SREM:
            case REDIS_LPUSH:
            case REDIS_LREM:
            case REDIS_SET:
            {
                const char* key = req->key.c_str();
                int size = req->values.size();
                for (int i = 0; i < size; ++i) {
                    switch (method) {
                        case REDIS_SADD:
                            redisAppendCommand(context, "SADD %s %s", key, req->values.front().c_str());
                            break;
                        case REDIS_SREM:
                            redisAppendCommand(context, "SREM %s %s", key, req->values.front().c_str());
                            break;
                        case REDIS_LPUSH:
                            redisAppendCommand(context, "LPUSH %s %s", key, req->values.front().c_str());
                            break;
                        case REDIS_LREM:
                            redisAppendCommand(context, "LREM %s %s", key, req->values.front().c_str());
                            break;
                        case REDIS_SET:
                            redisAppendCommand(context, "SET %s %s", key, req->values.front().c_str());
                            break;
                        default:
                            delete req;
                            return;
                    }
                    req->values.pop();
                }
                for (int i = 0; i < size; ++i) {
                    redisReply* reply = 0;
                    int ret = redisGetReply(context, (void**) &reply);
                    if (ret != REDIS_OK) {
                        DLOG(WARNING) << "A redis command failed. Restarting the redis system.";
                        if (reply)
                            freeReplyObject(reply);
                        redisFree(context);
                        context = 0;
                        delete req;
                        return;
                    }
                    if (reply)
                        freeReplyObject(reply);
                }
                break;
            }
            default:
                break;
        }
    }
    delete req;
}

void* Redis::runThread(void* param) {
    DLOG(INFO) << "Starting Redis thread.";
    redis_loop = ev_loop_new(EVFLAG_AUTO);
    redis_timer = new ev_timer;
    ev_timer_init(redis_timer, Redis::timeoutCallback, 0, 5.);
    ev_timer_start(redis_loop, redis_timer);

    ev_loop(redis_loop, 0);

    //Cleanup
    ev_timer_stop(redis_loop, redis_timer);
    delete redis_timer;
    redis_timer = 0;
    ev_loop_destroy(redis_loop);
    redis_loop = 0;
    DLOG(INFO) << "Redis thread exiting.";
    pthread_exit(NULL);
}

void Redis::timeoutCallback(struct ev_loop* loop, ev_timer* w, int revents) {
    if (!doRun) {
        ev_unloop(redis_loop, EVUNLOOP_ONE);
        return;
    }

    static char timebuf[32];
    time_t now = time(NULL);
    strftime(&timebuf[0], sizeof (timebuf), "%s", gmtime(&now));
    RedisRequest* checkin = new RedisRequest;
    checkin->key = lastCheckinKey;
    checkin->method = REDIS_SET;
    checkin->updateContext = RCONTEXT_ONLINE;
    checkin->values.push(&timebuf[0]);
    if (!addRequest(checkin))
        delete checkin;

    RedisRequest* req = getRequest();
    while (doRun && req) {
        processRequest(req);
        req = getRequest();
    }

    ev_timer_again(redis_loop, w);
}

RedisRequest* Redis::getRequest() {
    RedisRequest* ret = 0;
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        ret = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    return ret;
}

bool Redis::addRequest(RedisRequest* newRequest) {
    if (!doRun)
        return false;
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_nsec += REDIS_MUTEX_TIMEOUT;
    if (MUT_TIMEDLOCK(requestMutex, abs_time))
        return false;
    requestQueue.push(newRequest);
    MUT_UNLOCK(requestMutex);
    return true;
}
