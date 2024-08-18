#if defined(__GNUC__)

#define RTC_FORCE_INLINE __attribute__((__always_inline__))
#define RTC_NO_INLINE __attribute__((__noinline__))

#else

#define RTC_FORCE_INLINE
#define RTC_NO_INLINE

#endif