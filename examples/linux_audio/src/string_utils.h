#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

int startswith(const char *str, const char *pre);

#define streq(a,b) (!strcmp((a),(b)))

#if defined(__cplusplus)
}
#endif
