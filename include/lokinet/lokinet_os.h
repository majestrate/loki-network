#pragma once

/// OS specific types

#ifdef _WIN32
#include <handleapi.h>
#include <unistd.h>
#else
#include <poll.h>
#include <sys/uio.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WIN32
  typedef HANDLE OS_FD_t;
  struct iovec
  {
    void* iov_base;
    size_t iov_len;
  };

  typedef unsigned nfds_t;

#else
typedef int OS_FD_t;
#endif

#ifdef __cplusplus
}
#endif
