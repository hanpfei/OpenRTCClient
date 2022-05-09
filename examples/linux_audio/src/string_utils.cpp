
#include "string_utils.h"

#include <string.h>

int startswith(const char *str, const char *pre) {
  size_t lenstr = strlen(str);
  size_t lenpre = strlen(pre);
  return lenstr < lenpre ? -1 : memcmp(pre, str, lenpre);
}
