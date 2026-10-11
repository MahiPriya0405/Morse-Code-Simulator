#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include <stddef.h>

#define PASSWORD_HASH_BUFFER_SIZE 128

int hash_password(const char *password, char *encoded, size_t encoded_size);
int verify_password(const char *password, const char *stored_value);

#endif
