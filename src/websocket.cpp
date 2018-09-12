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

#include "precompiled_headers.hpp"

#include "websocket.hpp"
#include "sha1.hpp"
#include "base64.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include "startup_config.hpp"
#include "logging.hpp"
#include "connection.hpp"

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
namespace Websocket {

    /*
     * Due to some "issues" with the TLS proxy server mangling headers, we
     * have to lowercase headers to normalize the capitalization.
     */
    ProtocolVersion Acceptor::accept(std::string& input, std::string& output,
            std::string& ip) {
        ProtocolVersion ret = PROTOCOL_INCOMPLETE;
        try {
            DLOG(INFO) << "Attempting to parse a websocket header.";
            if (input.find("\r\n\r\n") == std::string::npos)
                return ret;

            std::map<std::string, std::string> headers;
            std::string line;
            std::string remainder = input;
            std::string::size_type end = remainder.find("\r\n");
            while (end != std::string::npos) {
                line = remainder.substr(0, end);
                remainder = remainder.substr(end + 2);

                std::string::size_type pos = line.find(": ");
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    for (size_t i = 0; i < key.length(); ++i) {
                        key[i] = tolower(key[i]);
                    }
                    headers[key] = line.substr(pos + 2);
                }

                end = remainder.find("\r\n");
            }

            std::string outbuffer;

            // Copy the forwarded for ip into our output if it exists.
            // Caller checks for proper remote address before trusting this.
            if (headers.count("x-real-ip") != 0) {
                ip = headers["x-real-ip"];
            }
            if (headers.count("sec-websocket-version") != 0 && headers.count("sec-websocket-key") != 0) {
                DLOG(INFO) << "Found a Hybi websocket header.";
                if (Hybi::accept(headers["sec-websocket-key"], headers["sec-websocket-origin"], outbuffer) != WS_RESULT_OK) {
                    ret = PROTOCOL_BAD;
                } else {
                    ret = PROTOCOL_HYBI;
                }
            } else {
                ret = PROTOCOL_BAD;
            }
            output = outbuffer;
        } catch (std::exception e) {
            LOG(WARNING) << "Exception occurred in websocket accept. e.what: " << e.what();
            ret = PROTOCOL_BAD;
        } catch (...) {
            LOG(WARNING) << "Unexpected exception occured in websocket accept.";
            ret = PROTOCOL_BAD;
        }
        return ret;
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
    //static const unsigned int wsOpcodeBinary = 0x02;
    static const unsigned int wsOpcodeClose = 0x08;
    static const unsigned int wsOpcodePing = 0x09;
    static const unsigned int wsOpcodePong = 0x0A;
    static const unsigned int wsMaximumClientFrameSize = 0x80000; //512kB should be sufficient for any client->server message.
    static const char* const wsMagicalGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    WebSocketResult Hybi::accept(string& key, string& origin, string& output) {
        if (key.empty())
            return WS_RESULT_ERROR;

        char buf[4096];
        bzero(&buf[0], sizeof (buf));
        int len = snprintf(&buf[0], sizeof (buf), "%s%s", key.c_str(), wsMagicalGUID);
        string keystr(&buf[0], len);
        bzero(&buf[0], sizeof (buf));
        string encodedkey;
        string hashkey = thirdparty::SHA1HashString(keystr);
        thirdparty::Base64Encode(hashkey, encodedkey);
        len = snprintf(&buf[0], sizeof (buf),
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: %s\r\n\r\n", encodedkey.c_str());
        string tmp(&buf[0], len);
        output.swap(tmp);
        return WS_RESULT_OK;
    }

    WebSocketResult Hybi::receive(ConnectionInstance* con, std::string& input,
                                  std::string& output) {
        unsigned long rlen = input.length();
        if (rlen < wsHeaderSize)
            return WS_RESULT_INCOMPLETE;

        const char* msg = input.c_str();
        unsigned char b1 = *msg++;
        unsigned char b2 = *msg++;

        unsigned int opcode = b1 & wsOpcodeMask;
        bool masked = b2 & wsMaskedMask;
        unsigned int lengthhint = b2 & wsLengthMask;

        // Standard defines that all client to server communication must be masked.
        if (!masked)
            return WS_RESULT_ERROR;

        switch (opcode) {
                // Text
            case wsOpcodeText:
            case wsOpcodePing:
            case wsOpcodePong:
                break;
                // Close
            case wsOpcodeClose:
                return WS_RESULT_CLOSE;
            default:
                return WS_RESULT_ERROR;
        }

        uint64_t payload_length = 0;
        int lengthsize = 0;
        if (lengthhint > wsSingleByteLength) {
            lengthsize = 2;
            if (lengthhint == wsEightByteLength)
                lengthsize = 8;

            if (rlen < wsHeaderSize + lengthsize)
                return WS_RESULT_INCOMPLETE;

            for (int i = 0; i < lengthsize; ++i) {
                payload_length <<= 8;
                payload_length |= static_cast<unsigned char> (*msg++);
            }
        } else {
            payload_length = lengthhint;
        }

        if (payload_length > wsMaximumClientFrameSize)
            return WS_RESULT_ERROR;

        uint64_t totallen = (masked ? wsMaskingKeySize : 0) + payload_length;
        if (rlen < (wsHeaderSize + lengthsize + totallen))
            return WS_RESULT_INCOMPLETE;

        // Immediately dump pongs.
        if (opcode == wsOpcodePong) {
            input = input.substr(wsHeaderSize + lengthsize + totallen);
            return WS_RESULT_PING_PONG;
        } else if (opcode == wsOpcodePing) {
            string pingData;
            pingData.resize(payload_length);
            const char* mask = msg;
            msg += wsMaskingKeySize;
            for (unsigned int i = 0; i < payload_length; ++i) {
                pingData[i] = msg[i] ^ mask[i % wsMaskingKeySize];
            }
            string pongData;
            Hybi::sendPong(pingData, pongData);
            con->sendRaw(pongData);
            input = input.substr(wsHeaderSize + lengthsize + totallen);
            return WS_RESULT_PING_PONG;
        }

        output.resize(payload_length);
        const char* mask = msg;
        msg += wsMaskingKeySize;
        for (unsigned int i = 0; i < payload_length; ++i) {
            output[i] = msg[i] ^ mask[i % wsMaskingKeySize];
        }

        input = input.substr(wsHeaderSize + lengthsize + totallen);

        return WS_RESULT_OK;
    }

    void Hybi::sendMessage(unsigned int opcode, const string& input, string& output) {
        std::vector<unsigned char> frame;
        unsigned int length = input.length();

        frame.push_back(0x80 | opcode);
        if (length <= wsSingleByteLength) {
            frame.push_back(static_cast<unsigned char> (length));
        } else if (length <= 0xFFFF) {
            frame.push_back(wsTwoByteLength);
            frame.push_back((length & 0xFF00) >> 8);
            frame.push_back(length & 0xFF);
        } else {
            uint64_t qlength = length;
            frame.push_back(wsEightByteLength);
            for (size_t i = 0; i < 8; ++i) {
                unsigned char length_piece = (qlength >> 8 * (7 - i)) & 0xFF;
                frame.push_back(length_piece);
            }
        }
        frame.insert(frame.end(), input.data(), input.data() + length);
        string tmp(frame.begin(), frame.end());
        output.swap(tmp);
    }

    void Hybi::sendText(const string& input, string& output) {
        Hybi::sendMessage(wsOpcodeText, input, output);
    }

    void Hybi::sendPong(const string& input, string& output) {
        Hybi::sendMessage(wsOpcodePong, input, output);
    }
}
