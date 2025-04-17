#ifndef SJIS_H
#define SJIS_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
  int convert_sjis(const unsigned char *, size_t, unsigned char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
