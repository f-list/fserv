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

#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>

//Local project headers
#include "logging.hpp"
#include "fthread.hpp"
#include "login_evhttp.hpp"
#include "http_client.hpp"
#include "server.hpp"
#include "startup_config.hpp"
#include "lua_constants.hpp"

#define SHUTDOWN_WAIT 2000000

void setup_signals() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
}

int main(int argc, char* argv[]) {
    bool in_docker = getenv("DOCKER") != nullptr;
    //Logging
    if(in_docker) {
        FLAGS_logbuflevel = 0;
        FLAGS_logbufsecs = 1;
        FLAGS_logtostderr = true;
    } else {
        FLAGS_logbuflevel = 0;
        FLAGS_logbufsecs = 0;
        FLAGS_logtostderr = false;
        FLAGS_log_dir = "./logs/";
    }
    FLAGS_log_prefix = argv[0];
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "F-Chat Server starting";
    unsigned const int desiredlimit = 10000;
    //Common startup
    setup_signals();
    struct rlimit fdlimit;
    getrlimit(RLIMIT_NOFILE, &fdlimit);
    if (fdlimit.rlim_max < desiredlimit) {
        LOG(WARNING) << "File limit was below the desired value of " << desiredlimit << ". Current: "
                     << fdlimit.rlim_cur << " Max: " << fdlimit.rlim_max;
        fdlimit.rlim_cur = desiredlimit;
        if (setrlimit(RLIMIT_NOFILE, &fdlimit) != 0) {
            getrlimit(RLIMIT_NOFILE, &fdlimit);
            fdlimit.rlim_cur = fdlimit.rlim_max;
            if (setrlimit(RLIMIT_NOFILE, &fdlimit) != 0) {
                LOG(WARNING) << "Call to raise the limit for open files failed.";
                printf("Call to raise the limit for open files failed. Continue with caution\n");
            } else {
                printf("Could not raise the limit for open files to %u. Current: %u Max: %u\n",
                       static_cast<unsigned int> (desiredlimit), static_cast<unsigned int> (fdlimit.rlim_cur),
                       static_cast<unsigned int> (fdlimit.rlim_max));
            }
        }
    }

    LuaConstants::initClass();
    StartupConfig::init();

    if(argc > 1 && strcmp("test", argv[1]) == 0) {
        Server::runTesting();
        LOG(INFO) << "Server shutdown completed.";
        google::ShutdownGoogleLogging();
        return 0;
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
        LOG(ERROR) << "Curl global startup failed.";
    LoginEvHTTPClient::setMaxLoginSlots(static_cast<unsigned int> (StartupConfig::getDouble("loginslots")));

    pthread_t loginThread;
    pthread_attr_t loginAttr;
    pthread_attr_init(&loginAttr);
    pthread_attr_setdetachstate(&loginAttr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&loginThread, &loginAttr, &LoginEvHTTPClient::runThread, 0);

    pthread_t httpThread;
    pthread_attr_t httpAttr;
    pthread_attr_init(&httpAttr);
    pthread_attr_setdetachstate(&httpAttr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&httpThread, &httpAttr, &HTTPClient::runThread, 0);

    pthread_t redisThread;
    if (StartupConfig::getBool("enableredis")) {
        pthread_attr_t redisAttr;
        pthread_attr_init(&redisAttr);
        pthread_attr_setdetachstate(&redisAttr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&redisThread, &redisAttr, &Redis::runThread, 0);
    } else {
        Redis::stopThread(); //Need to prevent redis from accepting requests.
    }

    //	Redis::stopThread();
    Server::run();

    usleep(SHUTDOWN_WAIT);
    DLOG(INFO) << "Starting shutdown.";

    HTTPClient::stopThread();
    pthread_join(httpThread, nullptr);

    //Shutdown
    LoginEvHTTPClient::stopThread();
    pthread_join(loginThread, 0);

    if (Redis::isRunning() && StartupConfig::getBool("enableredis")) {
        Redis::stopThread();
        pthread_join(redisThread, 0);
    }

    curl_global_cleanup();
    LOG(INFO) << "Server shutdown completed.";
    google::ShutdownGoogleLogging();
    return 0;
}
