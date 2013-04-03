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

// This code is HEAVILY based off of the Chromium websocket server source.
// You may view the license for Chromium in 3rdparty/LICENSE.chromium

#include "precompiled_headers.h"

#include "websocket.h"
#include "sha1.h"
#include "md5.h"
#include "base64.h"
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include "startup_config.h"
#include "logging.h"

#include <map>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>

// NOTE NOTE NOTE NOTE
// These functions have side effects!
// The receive function for these classes DOES modify the input buffer!
// The send function DOES NOT modify the input buffer.
// This is an implementation detail. The input buffering is known to be simple.
// The output buffering is far more complex, involving splitting and other things,
// and thus is beyond the scope of these functions. Input buffering is simple to implement here,
// and saves passing around a size variable to then advance the input buffer only if needed.
// NOTE NOTE NOTE NOTE
namespace Websocket
{
	ProtocolVersion Acceptor::accept(std::string& input, std::string& output)
	{
		ProtocolVersion ret = PROTOCOL_INCOMPLETE;
		try
		{
			DLOG(INFO) << "Attempting to parse a websocket header.";
			if(input.find("\r\n\r\n") == std::string::npos)
				return ret;

			std::map<std::string, std::string> headers;
			std::string line;
			std::string remainder = input;
			std::string::size_type end = remainder.find("\r\n");
			while(end != std::string::npos)
			{
				line = remainder.substr(0, end);
				remainder = remainder.substr(end+2);

				std::string::size_type pos = line.find(": ");
				if(pos != std::string::npos)
				{
					headers[line.substr(0, pos)] = line.substr(pos+2);
				}

				end = remainder.find("\r\n");
			}

			std::string outbuffer;

			if(headers.count("Sec-WebSocket-Version") != 0 && headers.count("Sec-WebSocket-Key") != 0)
			{
				DLOG(INFO) << "Found a Hybi websocket header.";
				if(Hybi::accept(headers["Sec-WebSocket-Key"], headers["Sec-WebSocket-Origin"], outbuffer) != WS_RESULT_OK)
				{
					ret = PROTOCOL_BAD;
				}
				else
				{
					ret = PROTOCOL_HYBI;
				}
			}
			else if(headers.count("Sec-WebSocket-Key1") != 0 && headers.count("Sec-WebSocket-Key2") != 0)
			{
				DLOG(INFO) << "Found a Hixie websocket header.";
				std::string::size_type pos = input.find("\r\n\r\n");
				if((pos - input.length()) > 8)
				{
					string key3 = input.substr(pos+4, 8);
					if(Hixie::accept(headers["Sec-WebSocket-Key1"], headers["Sec-WebSocket-Key2"], key3, headers["Host"], headers["Origin"], outbuffer) != WS_RESULT_OK)
					{
						ret = PROTOCOL_BAD;
					}
					else
					{
						ret = PROTOCOL_HIXIE;
					}
				}
				else
				{
					ret = PROTOCOL_INCOMPLETE;
				}
			}
			else
			{
				DLOG(INFO) << "Found a Draft-78 websocket header.";
				char buf[4096];
				bzero(&buf[0], sizeof(buf));
				int len = snprintf(&buf[0], sizeof(buf),
						"HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
						"Upgrade: WebSocket\r\n"
						"Connection: Upgrade\r\n"
						"WebSocket-Origin: %s\r\n"
						"WebSocket-Location: ws://%s:%d/\r\n\r\n",
						StartupConfig::getString("websocketorigin").c_str(), StartupConfig::getString("websockethost").c_str(), static_cast<int>(StartupConfig::getDouble("port"))
					);
				string temp(&buf[0], len);
				outbuffer.swap(temp);
				ret = PROTOCOL_HIXIE;
			}
			output = outbuffer;
		}
		catch (std::exception e)
		{
			LOG(WARNING) << "Exception occurred in websocket accept. e.what: " << e.what();
			ret = PROTOCOL_BAD;
		}
		catch (...)
		{
			LOG(WARNING) << "Unexpected exception occured in websocket accept.";
			ret = PROTOCOL_BAD;
		}
		return ret;

	}

	unsigned int Hixie::handshakeKey(std::string& input)
	{
		const char* p = input.c_str();
		int len = input.length();
		std::string number;
		int spaces = 0;
		for(int i = 0;i<len;++i)
		{
			char c = p[i];
			if(c <= '9' && c >= '0')
			{
				number += c;
			}
			else if (c == ' ')
			{
				++spaces;
			}
		}
		if(spaces == 0)
			return 0;
		long long ret = atoll(number.c_str());
		return htonl(static_cast<unsigned int>(ret/spaces));
	}

	WebSocketResult Hixie::accept(string& key1, string& key2, string& key3, string& host, string& origin, string& output)
	{
		if(key1.empty() || key2.empty() || key3.empty())
			return WS_RESULT_ERROR;

		bool useport=true;
		string wshost = host;
		if(wshost.empty())
			wshost = StartupConfig::getString("websockethost");

		if(wshost.find(':') != string::npos)
			useport=false;
//		if(origin.find("f-list.net") == string::npos)
//		{
//			return WS_RESULT_ERROR;
//		}

		unsigned int ikey1 = handshakeKey(key1);
		unsigned int ikey2 = handshakeKey(key2);

		char hash[16];
		memcpy(hash, &ikey1, 4);
		memcpy(hash+4, &ikey2, 4);
		memcpy(hash+8, key3.c_str(), 8);

		thirdparty::MD5Digest md5;
		thirdparty::MD5Sum(hash, 16, &md5);
		char buf[4096];
		bzero(&buf[0], sizeof(buf));

		int len = -1;
		if(useport)
		{
			len = snprintf(&buf[0], sizeof(buf),
				"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
				"Upgrade: WebSocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Origin: %s\r\n"
				"Sec-WebSocket-Location: ws://%s:%d/\r\n\r\n",
					origin.c_str(),
					wshost.c_str(),
					static_cast<int>(StartupConfig::getDouble("port"))
				);
		}
		else
		{
			len = snprintf(&buf[0], sizeof(buf),
				"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
				"Upgrade: WebSocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Origin: %s\r\n"
				"Sec-WebSocket-Location: ws://%s/\r\n\r\n",
					origin.c_str(),
					wshost.c_str()
				);
		}
		string tmp(&buf[0], len);
		tmp.append(reinterpret_cast<char*>(md5.a), 16);
		output.swap(tmp);
		return WS_RESULT_OK;
	}


	WebSocketResult Hixie::receive(std::string& input, std::string& output)
	{
		if(input[0] != 0)
			return WS_RESULT_ERROR;

		std::string::size_type pos = input.find(static_cast<char>(-1));
		if(pos == std::string::npos)
			return WS_RESULT_INCOMPLETE;
		output = input.substr(1, pos-1);
		input = input.substr(pos+1);
		return WS_RESULT_OK;
	}

	void Hixie::send(std::string& input, std::string& output)
	{
		output = static_cast<char>(0) + input + static_cast<char>(-1);
	}

	static const unsigned int wsHeaderSize = 2;
	static const unsigned int wsMaskingKeySize = 4;
	static const unsigned int wsOpcodeMask = 0x0F;
	static const unsigned int wsMaskedMask = 0x80;
	static const unsigned int wsLengthMask = 0x7F;
	static const unsigned int wsSingleByteLength = 125;
	static const unsigned int wsTwoByteLength = 126;
	static const unsigned int wsEightByteLength = 127;
	static const unsigned int wsOpcodeText = 0x01;
	static const unsigned int wsOpcodeClose = 0x08;
	static const unsigned int wsMaximumClientFrameSize = 0x80000; //512kB should be sufficient for any client->server message.
	static const char* const wsMagicalGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	WebSocketResult Hybi::accept(string& key, string& origin, string& output)
	{
		if(key.empty())
			return WS_RESULT_ERROR;

//		if(origin.find("f-list.net") == string::npos)
//		{
//			return WS_RESULT_ERROR;
//		}

		char buf[4096];
		bzero(&buf[0], sizeof(buf));
		int len = snprintf(&buf[0], sizeof(buf), "%s%s", key.c_str(), wsMagicalGUID);
		string keystr(&buf[0], len);
		bzero(&buf[0], sizeof(buf));
		string encodedkey;
		string hashkey = thirdparty::SHA1HashString(keystr);
		thirdparty::Base64Encode(hashkey, encodedkey);
		len = snprintf(&buf[0], sizeof(buf),
					   "HTTP/1.1 101 Switching Protocols\r\n"
					   "Upgrade: websocket\r\n"
					   "Connection: Upgrade\r\n"
					   "Sec-WebSocket-Accept: %s\r\n\r\n", encodedkey.c_str());
		string tmp(&buf[0], len);
		output.swap(tmp);
		return WS_RESULT_OK;
	}

	WebSocketResult Hybi::receive(std::string& input, std::string& output)
	{
		unsigned long  rlen = input.length();
		if(rlen < wsHeaderSize)
			return WS_RESULT_INCOMPLETE;

		const char* msg = input.c_str();
		unsigned char b1 = *msg++;
		unsigned char b2 = *msg++;

		unsigned int opcode = b1 & wsOpcodeMask;
		bool masked = b2 & wsMaskedMask;
		unsigned int lengthhint = b2 & wsLengthMask;

		// Standard defines that all client to server communication must be masked.
		if(!masked)
			return WS_RESULT_ERROR;

		switch (opcode)
		{
			// Text
			case wsOpcodeText:
				break;
			// Close
			case wsOpcodeClose:
				return WS_RESULT_CLOSE;
			default:
				return WS_RESULT_ERROR;
		}

		uint64_t payload_length = 0;
		int lengthsize = 0;
		if(lengthhint > wsSingleByteLength)
		{
			lengthsize = 2;
			if( lengthhint == wsEightByteLength )
				lengthsize = 8;

			if(rlen < wsHeaderSize+lengthsize)
				return WS_RESULT_INCOMPLETE;

			for(int i = 0;i<lengthsize;++i)
			{
				payload_length <<= 8;
				payload_length |= static_cast<unsigned char>(*msg++);
			}
		}
		else
		{
			payload_length = lengthhint;
		}

		if(payload_length > wsMaximumClientFrameSize)
			return WS_RESULT_ERROR;

		uint64_t totallen = (masked ? wsMaskingKeySize : 0) + payload_length;
		if(rlen < (wsHeaderSize + lengthsize + totallen))
			return WS_RESULT_INCOMPLETE;

		if(masked)
		{
			output.resize(payload_length);
			const char* mask = msg;
			msg += wsMaskingKeySize;
			for(unsigned int i = 0; i < payload_length; ++i)
			{
				output[i] = msg[i] ^ mask[i % wsMaskingKeySize];
			}
		}
		else
		{
			string tmp(msg, payload_length);
			output.swap(tmp);
		}


		input = input.substr(wsHeaderSize + lengthsize + totallen);

		return WS_RESULT_OK;
	}

	void Hybi::send(string& input, string& output)
	{
		std::vector<char> frame;
		unsigned int length = input.length();

		frame.push_back( 0x80 | wsOpcodeText );

		if(length <= wsSingleByteLength)
		{
			frame.push_back(static_cast<char>(length));
		}
		else if(length <= 0xFFFF)
		{
			frame.push_back(wsTwoByteLength);
			frame.push_back((length & 0xFF00) >> 8);
			frame.push_back(length & 0xFF);
		}
		else
		{
			uint64_t qlength = length;
			frame.push_back(wsEightByteLength);
			for(size_t i=0;i<sizeof(qlength);++i)
			{
				frame.push_back(qlength & 0xFF);
				qlength >>= 8;
			}
		}
		frame.insert(frame.end(), input.c_str(), input.c_str() + length);
		string tmp(frame.begin(), frame.end());
		output.swap(tmp);
	}
}
