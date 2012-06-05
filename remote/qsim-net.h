#ifndef __QSIM_NET_H
#define __QSIM_NET_H

#include <map>
#include <vector>

#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#define QSIM_NET_BUF_SIZE 32*(1<<20)

#ifdef DEBUG
#include <iostream>
#include <stdio.h>

static void dump(const char *s, size_t n) {
  while (n > 0) {
    unsigned i, dif = (n > 16)?16:n;
    for (i = 0; i < dif; i++) std::cout << std::hex << std::setw(2) 
                                        << std::setfill('0') 
                                        << (s[i]&0xff) << ' ';
    for (i = dif; i < 16; i++) std::cout << "   ";
    for (i = 0; i < dif; i++) std::cout << (isprint(s[i])?s[i]:'~');
    std::cout << '\n';
    n -= dif;
    s += dif;
  }
}

#endif

namespace QsimNet {
  struct SockHandle {
    SockHandle(int fd): fd(fd), buf_end(buf) {}
    SockHandle(): buf_end(buf) {}
    int fd;
    char buf[QSIM_NET_BUF_SIZE], *buf_end;
  };

  static bool raw_senddata(int fd, const char *message, size_t n) {
#ifdef DEBUG
    std::cout << "Send to " << fd << ":\n";
    dump(message, n);
#endif
     while (n > 0) {
       ssize_t rval = send(fd, message, n, 0);
       if (rval == -1 || rval == 0) {
         return false;
       } else {
         message += rval;
         n -= rval;
       }
     }
     return true;
  }

  static bool senddata(SockHandle &sock, const char *message, size_t n) {
    if (sock.buf_end - sock.buf + n > QSIM_NET_BUF_SIZE) {
      if (!raw_senddata(sock.fd, sock.buf, sock.buf_end - sock.buf))
        return false;
      sock.buf_end = sock.buf;
    }

    memcpy(sock.buf_end, message, n);
    sock.buf_end += n;

    return true;
  }

  static bool recvdata(SockHandle &sock, char *buf, size_t n) {
    #ifdef DEBUG
    size_t n0(n);
    char *buf0(buf);
    #endif

    // We're turning the link around, so flush the outgoing buffer to prevent
    // deadlock.
    if (sock.buf_end > sock.buf) {
      if (!raw_senddata(sock.fd, sock.buf, sock.buf_end - sock.buf))
        return false;
      sock.buf_end = sock.buf;
    }

    while (n > 0) {
      ssize_t rval = recv(sock.fd, buf, n, 0);

      if (rval == -1 || rval == 0) {
        return false;
      } else {
        buf += rval;
        n -= rval;
      }
    }
#ifdef DEBUG
    std::cout << "Recv on " << sock.fd << ":\n";
    dump(buf0, n0);
#endif
    return true;
  }

  struct SockBinStream {
    SockHandle &sock;
    SockBinStream(SockHandle *s) : sock(*s) {}
  };

  struct SockBinStreamError {};

  template <typename T> bool sockBinGet(SockHandle &sock, T& d) {
    return recvdata(sock, (char *)&d, sizeof(d));
  }

  template <typename T> bool sockBinPut(SockHandle &sock, const T& d) {
    return senddata(sock, (char *)&d, sizeof(d));
  }

  template <typename T> SockBinStream &operator>>(SockBinStream &g, T& d) {
    if (!sockBinGet(g.sock, d)) throw SockBinStreamError();
    return g;
  }

  template <typename T> SockBinStream &operator<<(SockBinStream &g, const T& d) 
  {
    if (!sockBinPut(g.sock, d)) throw SockBinStreamError();
    return g;
  }
}

static inline int create_listen_socket(const char *port, unsigned backlog) {
  struct addrinfo hints, *r;
  int listenfd;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, port, &hints, &r);
  listenfd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
  if (bind(listenfd, r->ai_addr, r->ai_addrlen) ||
      listen(listenfd, backlog))
  {
    std::cout << "Could not listen on port " << port 
              << ". Probably still in TIME_WAIT state.\n";
    exit(1);
  }

  return listenfd;
}

static inline int client_socket(const char *servaddr, const char *port) {
  struct addrinfo hints, *serv;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int rval = getaddrinfo(servaddr, port, &hints, &serv);
  if (rval) {
    std::cout << "Could not connect to server.\n";
    exit(1);
  }

  int socket_fd;
  for (struct addrinfo *p = serv; p; p = p->ai_next) { 
    socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (socket_fd == -1) continue;

    rval = connect(socket_fd, p->ai_addr, p->ai_addrlen);
    if (rval) { close(socket_fd); socket_fd = -1; continue; }

    int v(1);
    rval = setsockopt(socket_fd,IPPROTO_TCP,TCP_NODELAY,(char*)&v,sizeof(v));
    if (rval) { close(socket_fd); socket_fd = -1; continue; }
  }

  if (socket_fd == -1) {
    std::cout << "Could not connect to server.\n";
    exit(1);
  }

  return socket_fd;
}

static inline int next_connection(int listenfd) {
  struct sockaddr_storage remote_ad;
  socklen_t addr_len = sizeof(remote_ad);
  int fd = accept(listenfd, (struct sockaddr *)&remote_ad, &addr_len);

  int v(1), rval;
  rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
  if (rval < 0) {
    std::cout << "Could not set TCP_NODELAY.\n";
    exit(1);
  }

  return fd;
}

#endif
