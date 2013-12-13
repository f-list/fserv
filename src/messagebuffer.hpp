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

#ifndef MESSAGEBUFFER_HPP
#define	MESSAGEBUFFER_HPP

#include <stddef.h>
#include <boost/intrusive_ptr.hpp>
#include <string>

using std::string;
using boost::intrusive_ptr;

class StreamedList;

class MessageBuffer {
public:

    MessageBuffer()
    :
    length(0),
    buffer(0),
    refCount(0) { }

    ~MessageBuffer() {
        if (buffer) {
            delete[] buffer;
        }
    }

    inline void Set(const char* source, size_t inLength) {
        if (buffer) {
            delete[] buffer;
            buffer = 0;
        }
        buffer = new uint8_t[inLength];
        memcpy(buffer, source, inLength);
        length = inLength;
    }

    static MessageBuffer* FromString(string& message);
    
    const size_t Length() const {
        return length;
    }
    const uint8_t* Buffer() const {
        return buffer;
    }
private:
    size_t length;
    uint8_t* buffer;
    //StreamedList* list;

    volatile size_t refCount;

    friend inline void intrusive_ptr_release(MessageBuffer* p) {
        if ((--p->refCount) <= 0) {
            delete p;
        }
    }

    friend inline void intrusive_ptr_add_ref(MessageBuffer* p) {
        ++p->refCount;
    }
};

typedef intrusive_ptr<MessageBuffer> MessagePtr;

#endif	//MESSAGEBUFFER_HPP

