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

class MessageBuffer {
public:

    MessageBuffer()
    :
    length_(0),
    buffer_(0),
    refCount(0) { }

    ~MessageBuffer() {
        if (buffer_) {
            delete[] buffer_;
        }
    }

    inline void set(const char* source, size_t inLength) {
        if (buffer_) {
            delete[] buffer_;
            buffer_ = 0;
        }
        buffer_ = new uint8_t[inLength];
        memcpy(buffer_, source, inLength);
        length_ = inLength;
    }

    static MessageBuffer* fromString(string& message);
    
    const size_t length() const {
        return length_;
    }
    const uint8_t* buffer() const {
        return buffer_;
    }
private:
    size_t length_;
    uint8_t* buffer_;

    int refCount;

    friend inline void intrusive_ptr_release(MessageBuffer* p) {
        if (__sync_sub_and_fetch(&p->refCount, 1) <= 0) {
            delete p;
        }
    }

    friend inline void intrusive_ptr_add_ref(MessageBuffer* p) {
        __sync_fetch_and_add(&p->refCount, 1);
    }
};

typedef intrusive_ptr<MessageBuffer> MessagePtr;

#endif	//MESSAGEBUFFER_HPP

