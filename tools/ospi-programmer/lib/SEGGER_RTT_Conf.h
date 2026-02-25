/*
 * SEGGER RTT configuration for bare-metal OSPI programmer on M55_HP.
 * Custom config — no Zephyr Kconfig, no OS dependencies.
 *
 * Down buffer (host->target): 16KB for command+data (4KB write chunks + header)
 * Up buffer (target->host): 4KB for responses
 */

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS    1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS  1

/* Buffer sizes — allocated in DTCM via static arrays in main.c */
#define BUFFER_SIZE_UP                   (4 * 1024)
#define BUFFER_SIZE_DOWN                 (16 * 1024)

/* No OS locking — single-threaded bare-metal */
#define SEGGER_RTT_LOCK()
#define SEGGER_RTT_UNLOCK()

/* No malloc — all buffers statically allocated */
#define SEGGER_RTT_MALLOC(size)          ((void*)0)
#define SEGGER_RTT_FREE(ptr)

/* No memcpy/memset from libc — provide inline versions */
#define SEGGER_RTT_MEMCPY_USE_BYTELOOP  1
#define SEGGER_RTT_MEMCPY(dst,src,cnt)   _rtt_memcpy(dst,src,cnt)
#define SEGGER_RTT_MEMSET(dst,val,cnt)   _rtt_memset(dst,val,cnt)

static inline void *_rtt_memcpy(void *dst, const void *src, unsigned cnt) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (cnt--) *d++ = *s++;
    return dst;
}

static inline void *_rtt_memset(void *dst, int val, unsigned cnt) {
    unsigned char *d = (unsigned char *)dst;
    while (cnt--) *d++ = (unsigned char)val;
    return dst;
}

/* No printf support needed — use raw RTT_Write */
#define SEGGER_RTT_PRINTF_BUFFER_SIZE    0

/* No terminal support */
#define SEGGER_RTT_MAX_NUM_TERMINAL_OUT  0

#endif /* SEGGER_RTT_CONF_H */
