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

#include "login_curl.hpp"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "logging.hpp"
#include "startup_config.hpp"
#include "server.hpp"
#include "connection.hpp"

pthread_mutex_t Login::requestMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Login::replyMutex = PTHREAD_MUTEX_INITIALIZER;
queue<LoginRequest*> Login::requestQueue;
queue<LoginReply*> Login::replyQueue;
unsigned int Login::maxLoginSlots = 30;
bool Login::doRun = true;
struct ev_loop* Login::login_loop = 0;
ev_async* Login::login_async = 0;
ev_timer* Login::login_timer = 0;

const __useconds_t Login::CURL_FAILURE_WAIT = 5000000; //5 seconds;

CURL* curl_handle = 0;

bool check_curl_code(CURLcode code)
{
	if(code != CURLE_OK)
	{
		LOG(WARNING) << "Curl reported: " << curl_easy_strerror(code) << " and may not be set up properly.";
		return false;
	}
	return true;
}

bool curl_escape_string(string& to_escape)
{
	bool res = false;
	char* output = curl_easy_escape(curl_handle, to_escape.c_str(), to_escape.length());
	if(output)
	{
		to_escape = output;
		curl_free(output);
		res = true;
	}
	return res;
}

bool Login::setupCurlHandle()
{
	bool res = false;

	for(int i = 0; i < 5; ++i)
	{
		if(curl_handle)
		{
			curl_easy_cleanup(curl_handle);
			curl_handle = 0;
		}

		curl_handle = curl_easy_init();
		if(!curl_handle)
			continue;

		CURLcode code = CURLE_OK;
		code = curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 3600);
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, static_cast<long>(StartupConfig::getDouble("logintimeout")));
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, StartupConfig::getString("version").c_str());
		if (!check_curl_code(code))
			continue;
		code = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, Login::curlWriteFunction);
		if (!check_curl_code(code))
			continue;

		res = true;
		break;
	}

	return res;
}

size_t Login::curlWriteFunction(void* contents, size_t size, size_t nmemb, void* user)
{
	LoginReply* reply = static_cast<LoginReply*>(user);
	size_t realsize = size * nmemb;
	reply->message.append(static_cast<char*>(contents), realsize);
	return realsize;
}

LoginReply* Login::processLogin(LoginRequest* request)
{
	bool res = false;
	LoginReply* reply = new LoginReply;
	reply->connection = request->connection;
	reply->success = false;

	string url = StartupConfig::getString("loginurl");
	switch(request->method)
	{
		case LOGIN_METHOD_TICKET:
			url += "?method=ticket&account=";
			if(!curl_escape_string(request->account))
				break;
			url += request->account + "&ticket=";
			if(!curl_escape_string(request->ticket))
				break;
			url += request->ticket + "&char=";
			if(!curl_escape_string(request->characterName))
				break;
			url += request->characterName;
			res = true;
			break;
		default:
			LOG(WARNING) << "Login request with unhandled method used. Method: " << (int)request->method;
			delete request;
			return reply;
			break;
	}

	CURLcode code = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, reply);
	if(!check_curl_code(code))
		res = false;

	if(res)
	{
		code = curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
		if(check_curl_code(code))
		{
			code = curl_easy_perform(curl_handle);
			if(!check_curl_code(code))
			{
				LOG(WARNING) << "Curl perform failed with: " << curl_easy_strerror(code);
			}
			else
			{
				reply->success = true;
			}
		}
		else
		{
			LOG(WARNING) << "Failed to set url for curl to perform. Error: " << curl_easy_strerror(code);
		}
	}

	delete request;
	return reply;
}

void Login::processQueue(struct ev_loop* loop, ev_async* w, int revents)
{
    if (!doRun)
    {
        ev_unloop(login_loop, EVUNLOOP_ONE);
        return;
    }

	LoginRequest* req = getRequest();
	while(req)
	{
		LoginReply* reply = processLogin(req);
		addReply(reply);
		Server::sendWakeup();
		req = getRequest();
	}
}

void Login::timeoutCallback(struct ev_loop* loop, ev_timer* w, int revents)
{
	if(!doRun)
	{
		ev_unloop(login_loop, EVUNLOOP_ONE);
		return;
	}

	ev_timer_again(login_loop, w);
}

void* Login::runThread(void* param)
{
    DLOG(INFO) << "Starting login thread.";
	while(!setupCurlHandle())
	{
		LOG(DFATAL) << "Could not set up a curl handle for login. Sleeping.";
		usleep(CURL_FAILURE_WAIT);
	}

    login_loop = ev_loop_new(EVFLAG_AUTO);
	login_timer = new ev_timer;
	ev_timer_init(login_timer, Login::timeoutCallback, 0, 5.);
	ev_timer_start(login_loop, login_timer);
    login_async = new ev_async;
    ev_async_init(login_async, Login::processQueue);
    ev_async_start(login_loop, login_async);

    ev_loop(login_loop, 0);

    //Cleanup
    ev_async_stop(login_loop, login_async);
	delete login_async;
    login_async = 0;
	ev_timer_stop(login_loop, login_timer);
	delete login_timer;
	login_timer = 0;
    ev_loop_destroy(login_loop);
    login_loop = 0;
    DLOG(INFO) << "Login thread exiting.";
    pthread_exit(NULL);
}

void Login::sendWakeup()
{
    if (login_loop && login_async)
    {
		//DLOG(INFO) << "Sending a wakeup to the login thread.";
        ev_async_send(login_loop, login_async);
    }
}

LoginRequest* Login::getRequest()
{
	LoginRequest* ret = 0;
	//DLOG(INFO) << "Getting login request from queue.";
	MUT_LOCK(requestMutex);
	if(requestQueue.size() > 0)
	{
		ret = requestQueue.front();
		requestQueue.pop();
	}
	MUT_UNLOCK(requestMutex);
	//DLOG(INFO) << "Finished getting login request from queue.";
	return ret;
}

LoginReply* Login::getReply()
{
	LoginReply* ret = 0;
	//DLOG(INFO) << "Getting login reply from queue.";
	MUT_LOCK(replyMutex);
	if(replyQueue.size() > 0)
	{
		ret = replyQueue.front();
		replyQueue.pop();
	}
	MUT_UNLOCK(replyMutex);
	//DLOG(INFO) << "Finished getting login reply from queue.";
	return ret;
}


bool Login::addReply ( LoginReply* newReply )
{
    //DLOG(INFO) << "Adding login reply to queue.";
    MUT_LOCK(replyMutex);
    replyQueue.push(newReply);
    MUT_UNLOCK(replyMutex);
    //DLOG(INFO) << "Finished adding login reply to queue.";
    return true;
}

bool Login::addRequest ( LoginRequest* newRequest )
{
    //DLOG(INFO) << "Adding login request to queue.";
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_nsec += LOGIN_MUTEX_TIMEOUT;
    if ( MUT_TIMEDLOCK(requestMutex, abs_time) )
        return false;
    if ( requestQueue.size() >= maxLoginSlots )
    {
        MUT_UNLOCK(requestMutex);
        return false;
    }
    requestQueue.push(newRequest);
    MUT_UNLOCK(requestMutex);
    //DLOG(INFO) << "Finished adding login request to queue.";
    return true;
}

