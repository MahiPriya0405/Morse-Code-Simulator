#include "password_hash.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

#define PBKDF2_ITERATIONS 600000UL
#define PBKDF2_SALT_BYTES 16
#define PBKDF2_KEY_BYTES 32

typedef void *BCRYPT_ALG_HANDLE;
typedef LONG (WINAPI *bcrypt_open_algorithm_provider_fn)(
    BCRYPT_ALG_HANDLE *, LPCWSTR, LPCWSTR, ULONG);
typedef LONG (WINAPI *bcrypt_gen_random_fn)(
    BCRYPT_ALG_HANDLE, unsigned char *, ULONG, ULONG);
typedef LONG (WINAPI *bcrypt_derive_key_pbkdf2_fn)(
    BCRYPT_ALG_HANDLE, unsigned char *, ULONG, unsigned char *, ULONG,
    unsigned long long, unsigned char *, ULONG, ULONG);
typedef LONG (WINAPI *bcrypt_close_algorithm_provider_fn)(
    BCRYPT_ALG_HANDLE, ULONG);

#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008UL
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002UL
static int derive_key(const char *password, size_t password_length,
                      unsigned char salt[PBKDF2_SALT_BYTES],
                      unsigned char key[PBKDF2_KEY_BYTES], int create_salt,
                      unsigned long iterations)
{
    HMODULE bcrypt = NULL;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    bcrypt_open_algorithm_provider_fn open_provider;
    bcrypt_gen_random_fn gen_random;
    bcrypt_derive_key_pbkdf2_fn derive_pbkdf2;
    bcrypt_close_algorithm_provider_fn close_provider;
    int success = 0;
    LONG status;

    if (password_length > (size_t)ULONG_MAX)
        return 0;

    bcrypt = LoadLibraryA("bcrypt.dll");
    if (!bcrypt)
        return 0;

    open_provider = (bcrypt_open_algorithm_provider_fn)
        GetProcAddress(bcrypt, "BCryptOpenAlgorithmProvider");
    gen_random = (bcrypt_gen_random_fn)
        GetProcAddress(bcrypt, "BCryptGenRandom");
    derive_pbkdf2 = (bcrypt_derive_key_pbkdf2_fn)
        GetProcAddress(bcrypt, "BCryptDeriveKeyPBKDF2");
    close_provider = (bcrypt_close_algorithm_provider_fn)
        GetProcAddress(bcrypt, "BCryptCloseAlgorithmProvider");

    if (!open_provider || !gen_random || !derive_pbkdf2 || !close_provider)
        goto cleanup;

    status = open_provider(&algorithm, L"SHA256", NULL,
                           BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status < 0)
        goto cleanup;

    if (create_salt)
    {
        status = gen_random(NULL, salt, PBKDF2_SALT_BYTES,
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0)
            goto cleanup;
    }

    status = derive_pbkdf2(algorithm, (unsigned char *)password,
                           (ULONG)password_length, salt, PBKDF2_SALT_BYTES,
                           iterations, key, PBKDF2_KEY_BYTES, 0);
    success = status >= 0;

cleanup:
    if (algorithm && close_provider)
        close_provider(algorithm, 0);
    if (bcrypt)
        FreeLibrary(bcrypt);
    return success;
}
#endif

static void secure_zero(void *memory, size_t length)
{
    volatile unsigned char *bytes = (volatile unsigned char *)memory;
    while (length--)
        *bytes++ = 0;
}

static void encode_hex(const unsigned char *bytes, size_t length, char *output)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < length; i++)
    {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    output[length * 2] = '\0';
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static int decode_hex(const char *text, size_t text_length,
                      unsigned char *output, size_t output_length)
{
    size_t i;

    if (text_length != output_length * 2)
        return 0;

    for (i = 0; i < output_length; i++)
    {
        int high = hex_value(text[i * 2]);
        int low = hex_value(text[i * 2 + 1]);
        if (high < 0 || low < 0)
            return 0;
        output[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}

static int constant_time_equal(const unsigned char *left,
                               const unsigned char *right, size_t length)
{
    unsigned int difference = 0;
    size_t i;

    for (i = 0; i < length; i++)
        difference |= (unsigned int)(left[i] ^ right[i]);

    return difference == 0;
}

static int constant_time_text_equal(const char *left, const char *right)
{
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t i;
    size_t maximum = left_length > right_length ? left_length : right_length;
    unsigned int difference = (unsigned int)(left_length ^ right_length);

    for (i = 0; i < maximum; i++)
    {
        unsigned char left_byte = i < left_length ? (unsigned char)left[i] : 0;
        unsigned char right_byte = i < right_length ? (unsigned char)right[i] : 0;
        difference |= (unsigned int)(left_byte ^ right_byte);
    }

    return difference == 0;
}

int hash_password(const char *password, char *encoded, size_t encoded_size)
{
    static const char prefix[] = "pbkdf2_sha256$";
    unsigned char salt[16] = {0};
    unsigned char key[32] = {0};
    char salt_hex[33];
    char key_hex[65];
    int written;

    if (!password || !encoded || !encoded_size)
        return 0;

#ifdef _WIN32
    if (!derive_key(password, strlen(password), salt, key, 1,
                    PBKDF2_ITERATIONS))
    {
        secure_zero(salt, sizeof(salt));
        secure_zero(key, sizeof(key));
        return 0;
    }
#else
    (void)salt;
    (void)key;
    return 0;
#endif

    encode_hex(salt, sizeof(salt), salt_hex);
    encode_hex(key, sizeof(key), key_hex);
    written = snprintf(encoded, encoded_size, "%s%u$%s$%s", prefix,
                       (unsigned int)PBKDF2_ITERATIONS, salt_hex, key_hex);
    secure_zero(salt, sizeof(salt));
    secure_zero(key, sizeof(key));
    return written > 0 && (size_t)written < encoded_size;
}

static int verify_hashed_password(const char *password, const char *stored_value)
{
    static const char prefix[] = "pbkdf2_sha256$";
    const char *iterations_text;
    const char *salt_text;
    const char *key_text;
    char *end = NULL;
    unsigned long iterations;
    unsigned char salt[16] = {0};
    unsigned char expected[32] = {0};
    unsigned char actual[32] = {0};
    int matches = 0;

    if (strncmp(stored_value, prefix, sizeof(prefix) - 1) != 0)
        return 0;

    iterations_text = stored_value + sizeof(prefix) - 1;
    errno = 0;
    iterations = strtoul(iterations_text, &end, 10);
    if (errno || end == iterations_text || !end || *end != '$' ||
        iterations < 100000UL || iterations > 10000000UL)
        return 0;

    salt_text = end + 1;
    if (strlen(salt_text) < 32 || salt_text[32] != '$')
        return 0;
    key_text = salt_text + 33;
    if (strlen(key_text) != 64 ||
        !decode_hex(salt_text, 32, salt, sizeof(salt)) ||
        !decode_hex(key_text, 64, expected, sizeof(expected)))
        goto cleanup;

#ifdef _WIN32
    if (derive_key(password, strlen(password), salt, actual, 0, iterations))
        matches = constant_time_equal(actual, expected, sizeof(actual));
#endif

cleanup:
    secure_zero(salt, sizeof(salt));
    secure_zero(expected, sizeof(expected));
    secure_zero(actual, sizeof(actual));
    return matches;
}

int verify_password(const char *password, const char *stored_value)
{
    static const char prefix[] = "pbkdf2_sha256$";

    if (!password || !stored_value)
        return 0;

    if (strncmp(stored_value, prefix, sizeof(prefix) - 1) == 0)
        return verify_hashed_password(password, stored_value);

    /* Existing project accounts contain plaintext passwords; keep them usable. */
    return constant_time_text_equal(password, stored_value);
}
