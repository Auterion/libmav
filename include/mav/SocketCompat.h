/****************************************************************************
 *
 * Copyright (c) 2023, libmav development team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name libmav nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef MAV_SOCKET_COMPAT_H
#define MAV_SOCKET_COMPAT_H

// Cross-platform socket abstraction. The BSD-socket interfaces (UDPServer,
// UDPClient, TCPServer, TCPClient) include this instead of the raw POSIX
// headers so they compile against Winsock2 on Windows while remaining
// byte-for-byte identical on POSIX platforms.

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

namespace mav {

    using socket_handle_t = SOCKET;
    static const socket_handle_t INVALID_SOCKET_HANDLE = INVALID_SOCKET;

    // Winsock requires explicit initialization. The OS refcounts WSAStartup,
    // so coexisting with other libraries that also initialize Winsock (e.g.
    // the bundled httplib) is safe. We never call WSACleanup: usage lasts for
    // the process lifetime.
    inline void initSocketLibrary() {
        static std::once_flag flag;
        std::call_once(flag, []() {
            WSADATA wsa_data;
            WSAStartup(MAKEWORD(2, 2), &wsa_data);
        });
    }

    inline int closeSocket(socket_handle_t s) { return ::closesocket(s); }

    inline int lastSocketError() { return WSAGetLastError(); }

    // POSIX inet_aton replacement. Returns 1 on success, 0 on failure.
    inline int inet_aton_compat(const char* cp, struct in_addr* inp) {
        return InetPtonA(AF_INET, cp, inp) == 1 ? 1 : 0;
    }
}

#define MAV_SHUT_RDWR SD_BOTH
#define MAV_ECONNREFUSED WSAECONNREFUSED

#else // POSIX

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

namespace mav {

    using socket_handle_t = int;
    static const socket_handle_t INVALID_SOCKET_HANDLE = -1;

    inline void initSocketLibrary() {}

    inline int closeSocket(socket_handle_t s) { return ::close(s); }

    inline int lastSocketError() { return errno; }

    inline int inet_aton_compat(const char* cp, struct in_addr* inp) {
        return ::inet_aton(cp, inp);
    }
}

#define MAV_SHUT_RDWR SHUT_RDWR
#define MAV_ECONNREFUSED ECONNREFUSED

#endif

#endif // MAV_SOCKET_COMPAT_H
