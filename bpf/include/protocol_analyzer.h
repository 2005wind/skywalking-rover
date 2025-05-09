// Licensed to Apache Software Foundation (ASF) under one or more contributor
// license agreements. See the NOTICE file distributed with
// this work for additional information regarding copyright
// ownership. Apache Software Foundation (ASF) licenses this file to you under
// the Apache License, Version 2.0 (the "License"); you may
// not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#define CONNECTION_PROTOCOL_UNKNOWN 0
#define CONNECTION_PROTOCOL_HTTP1 1
#define CONNECTION_PROTOCOL_HTTP2 2

#define CONNECTION_MESSAGE_TYPE_UNKNOWN 0
#define CONNECTION_MESSAGE_TYPE_REQUEST 1
#define CONNECTION_MESSAGE_TYPE_RESPONSE 2

// HTTP 1.x
// request frame format: https://www.rfc-editor.org/rfc/rfc2068.html#section-5
// response frame format: https://www.rfc-editor.org/rfc/rfc2068.html#section-6
static __inline __u32 infer_http1_message(const char* buf, size_t count) {
    if (count < 16) {
        return CONNECTION_MESSAGE_TYPE_UNKNOWN;
    }
    // response
    if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
        return CONNECTION_MESSAGE_TYPE_RESPONSE;
    }
    // request
    if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'O' && buf[1] == 'P' && buf[2] == 'T' && buf[3] == 'I' && buf[4] == 'O'
        && buf[5] == 'N' && buf[6] == 'S')
    {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'A' && buf[3] == 'D') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'T') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'E' && buf[4] == 'T' && buf[5] == 'E') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'C' && buf[1] == 'O' && buf[2] == 'N' && buf[3] == 'N' && buf[4] == 'E'
        && buf[5] == 'C' && buf[6] == 'T')
    {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'T' && buf[1] == 'R' && buf[2] == 'A' && buf[3] == 'C' && buf[4] == 'E') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    if (buf[0] == 'P' && buf[1] == 'A' && buf[2] == 'T' && buf[3] == 'C' && buf[4] == 'H') {
        return CONNECTION_MESSAGE_TYPE_REQUEST;
    }
    return CONNECTION_MESSAGE_TYPE_UNKNOWN;
}

static bool is_http2_magic(const char *buf_src, size_t count)
{
	static const char magic[] = "PRI * HTTP/2";
	char buffer[sizeof(magic)] = { 0 };
	bpf_probe_read(buffer, sizeof(buffer) - 1, buf_src);
	for (int idx = 0; idx < sizeof(magic); ++idx) {
		if (magic[idx] == buffer[idx])
			continue;
		return false;
	}
	return true;
}

// HTTP 2.x
// frame format: https://www.rfc-editor.org/rfc/rfc7540.html#section-4.1
static __inline __u32 infer_http2_message(const char* buf, size_t count) {
    static const __u8 kFrameBasicSize = 0x9; // including Length, Type, Flags, Reserved, Stream Identity
    static const __u8 kFrameTypeHeader = 0x1; // the type of the frame: https://www.rfc-editor.org/rfc/rfc7540.html#section-6.2
    static const __u8 kFrameLoopCount = 5;

    static const __u8 kStaticTableMaxSize = 61;// https://www.rfc-editor.org/rfc/rfc7541#appendix-A
    static const __u8 kStaticTableAuth = 1;
    static const __u8 kStaticTableGet = 2;
    static const __u8 kStaticTablePost = 3;
    static const __u8 kStaticTablePath1 = 4;
    static const __u8 kStaticTablePath2 = 5;

    // the buffer size must bigger than basic frame size
    if (count < kFrameBasicSize) {
		return CONNECTION_MESSAGE_TYPE_UNKNOWN;
    }

    // frame info
    __u8 frame[21] = { 0 };
    __u32 frameOffset = 0;
    // header info
    __u8 staticInx, headerBlockFragmentOffset;

    if (is_http2_magic(buf, count)) {
        frameOffset = 24;
    }

    // each all frame
#pragma unroll
    for (__u8 i = 0; i < kFrameLoopCount; i++) {
        if (frameOffset >= count) {
            break;
        }

        // read frame
        bpf_probe_read(frame, sizeof(frame), buf + frameOffset);
        frameOffset += (bpf_ntohl(*(__u32 *) frame) >> 8) + kFrameBasicSize;

        // frametype only accept 0x00 - 0x09
        if (frame[3] > 0x09) {
            return CONNECTION_MESSAGE_TYPE_UNKNOWN;
        }
        // is header frame
        if (frame[3] != kFrameTypeHeader) {
            continue;
        }

        // validate the header(unset): not HTTP2 protocol
        // this frame must is a send request
        if ((frame[4] & 0xd2) || frame[5] & 0x01) {
            return CONNECTION_MESSAGE_TYPE_UNKNOWN;
        }

        // stream ID cannot be 0
        __u32 streamID = ((frame[5] & 0x7F) << 24) | (frame[6] << 16) | (frame[7] << 8) | frame[8];
        if (streamID == 0) {
            return CONNECTION_MESSAGE_TYPE_UNKNOWN;
        }

        // locate the header block fragment offset
        headerBlockFragmentOffset = kFrameBasicSize;
        if (frame[4] & 0x08) {  // PADDED flag is set
            headerBlockFragmentOffset += 1;
        }
        if (frame[4] & 0x20) {  // PRIORITY flag is set
            headerBlockFragmentOffset += 5;
        }

#pragma unroll
        for (__u8 j = 0; j <= kStaticTablePath2; j++) {
            if (headerBlockFragmentOffset > count) {
                return CONNECTION_MESSAGE_TYPE_UNKNOWN;
            }
            staticInx = frame[headerBlockFragmentOffset] & 0x7f;
            if (staticInx <= kStaticTableMaxSize && staticInx > 0) {
                if (staticInx == kStaticTableAuth ||
                    staticInx == kStaticTableGet ||
                    staticInx == kStaticTablePost ||
                    staticInx == kStaticTablePath1 ||
                    staticInx == kStaticTablePath2) {
                    return CONNECTION_MESSAGE_TYPE_REQUEST;
                } else {
                    return CONNECTION_MESSAGE_TYPE_RESPONSE;
                }
            }
            headerBlockFragmentOffset++;
        }
    }

	return CONNECTION_MESSAGE_TYPE_UNKNOWN;
}

static __inline __u32 analyze_protocol(char *buf, __u32 count, __u8 *protocol_ref) {
    __u32 protocol = CONNECTION_PROTOCOL_UNKNOWN, type = CONNECTION_MESSAGE_TYPE_UNKNOWN;

    // support http 1.x and 2.x
    if ((type = infer_http1_message(buf, count)) != CONNECTION_PROTOCOL_UNKNOWN) {
        protocol = CONNECTION_PROTOCOL_HTTP1;
    } else if ((type = infer_http2_message(buf, count)) != CONNECTION_PROTOCOL_UNKNOWN) {
        protocol = CONNECTION_PROTOCOL_HTTP2;
    }

    if (protocol != CONNECTION_PROTOCOL_UNKNOWN) {
        *protocol_ref = protocol;
    }

    return type;
}