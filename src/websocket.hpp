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

// This code is HEAVILY based off of the Chromium websocket server source. Which is covered by the BSD license.
// You may view the license for Chromium in 3rdparty/LICENSE.chromium

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>

enum ProtocolVersion {
    PROTOCOL_HYBI, //Complex framing!
    PROTOCOL_BAD, //Fatal handshake failure.
    PROTOCOL_INCOMPLETE, //Incomplete handshake.
    PROTOCOL_UNKNOWN //Something bad happened.
};

enum WebSocketResult {
    WS_RESULT_OK,
    WS_RESULT_ERROR,
    WS_RESULT_INCOMPLETE,
    WS_RESULT_CLOSE
};

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

    class Acceptor {
    public:
        static ProtocolVersion accept(std::string& input, std::string& output,
                                      std::string& ip);
    private:

        Acceptor() { }

        ~Acceptor() { }
    };

    class Hybi {
    public:
        static WebSocketResult accept(std::string& key, std::string& origin,
                                      std::string& output);
        static WebSocketResult receive(std::string& input, std::string& output);
        static void send(std::string& input, std::string& output);
    private:

        Hybi() { }

        ~Hybi() { }
    };
}
#endif //WEBSOCKET_H
