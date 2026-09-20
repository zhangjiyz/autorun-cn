#ifndef PROFILE_TEST_SWITCH_H
#define PROFILE_TEST_SWITCH_H
#include <openssl/sha.h>
#define SHA256_HASH_SIZE SHA256_DIGEST_LENGTH
typedef SHA256_CTX Sha256Context;
static inline void sha256ContextCreate( Sha256Context *c ) { SHA256_Init( c ); }
static inline void sha256ContextUpdate( Sha256Context *c, const void *p, size_t n ) { SHA256_Update( c, p, n ); }
static inline void sha256ContextGetHash( Sha256Context *c, void *out ) { SHA256_Final( out, c ); }
#endif
