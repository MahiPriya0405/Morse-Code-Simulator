#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "morse_logic.h"
#include "database.h"
#include "password_hash.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

#define PORT 8080
#define REQUEST_SIZE 16384

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} JsonBuffer;

static int send_all(SOCKET client, const char *data, size_t length)
{
    size_t sent_total = 0;

    while (sent_total < length)
    {
        size_t remaining = length - sent_total;
        int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        int sent = send(client, data + sent_total, chunk, 0);

        if (sent <= 0)
            return 0;

        sent_total += (size_t)sent;
    }

    return 1;
}

static const char *reason_phrase(int status)
{
    switch (status)
    {
    case 201: return "Created";
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
    default: return "Error";
    }
}

static void send_response(SOCKET client, int status, const char *content_type,
                          const char *body)
{
    char header[768];
    size_t body_length = body ? strlen(body) : 0;
    int header_length = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n\r\n",
        status, reason_phrase(status), content_type,
        (unsigned long)body_length);

    if (header_length <= 0 || (size_t)header_length >= sizeof(header))
        return;

    if (!send_all(client, header, (size_t)header_length))
        return;

    if (body_length > 0)
        send_all(client, body, body_length);
}

static void json_init(JsonBuffer *json)
{
    json->capacity = 256;
    json->length = 0;
    json->data = (char *)malloc(json->capacity);
    if (json->data)
        json->data[0] = '\0';
}

static int json_reserve(JsonBuffer *json, size_t extra)
{
    size_t needed;
    size_t capacity;
    char *grown;

    if (!json->data || extra > (size_t)-1 - json->length - 1)
        return 0;

    needed = json->length + extra + 1;
    if (needed <= json->capacity)
        return 1;

    capacity = json->capacity;
    while (capacity < needed)
    {
        if (capacity > (size_t)-1 / 2)
        {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    grown = (char *)realloc(json->data, capacity);
    if (!grown)
        return 0;

    json->data = grown;
    json->capacity = capacity;
    return 1;
}

static int json_append(JsonBuffer *json, const char *text)
{
    size_t length = strlen(text);
    if (!json_reserve(json, length))
        return 0;

    memcpy(json->data + json->length, text, length + 1);
    json->length += length;
    return 1;
}

static int json_appendf(JsonBuffer *json, const char *format, ...)
{
    va_list args;
    va_list copy;
    int needed;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (needed < 0 || !json_reserve(json, (size_t)needed))
    {
        va_end(args);
        return 0;
    }

    vsnprintf(json->data + json->length,
              json->capacity - json->length, format, args);
    json->length += (size_t)needed;
    va_end(args);
    return 1;
}

static int json_append_string(JsonBuffer *json, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");

    if (!json_append(json, "\""))
        return 0;

    while (*cursor)
    {
        char escaped[8];

        switch (*cursor)
        {
        case '"': if (!json_append(json, "\\\"")) return 0; break;
        case '\\': if (!json_append(json, "\\\\")) return 0; break;
        case '\b': if (!json_append(json, "\\b")) return 0; break;
        case '\f': if (!json_append(json, "\\f")) return 0; break;
        case '\n': if (!json_append(json, "\\n")) return 0; break;
        case '\r': if (!json_append(json, "\\r")) return 0; break;
        case '\t': if (!json_append(json, "\\t")) return 0; break;
        default:
            if (*cursor < 0x20)
            {
                snprintf(escaped, sizeof(escaped), "\\u%04x", *cursor);
                if (!json_append(json, escaped)) return 0;
            }
            else
            {
                char character[2] = {(char)*cursor, '\0'};
                if (!json_append(json, character)) return 0;
            }
        }

        cursor++;
    }

    return json_append(json, "\"");
}

static void send_json(SOCKET client, int status, JsonBuffer *json)
{
    if (!json->data)
    {
        send_response(client, 500, "application/json; charset=utf-8",
                      "{\"error\":\"Out of memory\"}");
        return;
    }

    send_response(client, status, "application/json; charset=utf-8", json->data);
    free(json->data);
    json->data = NULL;
}

static void send_error(SOCKET client, int status, const char *message)
{
    JsonBuffer json;
    json_init(&json);

    if (!json_append(&json, "{\"error\":") ||
        !json_append_string(&json, message) ||
        !json_append(&json, "}"))
    {
        free(json.data);
        send_response(client, 500, "application/json; charset=utf-8",
                      "{\"error\":\"Internal server error\"}");
        return;
    }

    send_json(client, status, &json);
}

static int decode_component(const char *source, size_t length,
                            char *destination, size_t capacity)
{
    size_t read_index = 0;
    size_t write_index = 0;

    if (!capacity)
        return 0;

    while (read_index < length)
    {
        unsigned char value = (unsigned char)source[read_index++];

        if (value == '+')
        {
            value = ' ';
        }
        else if (value == '%')
        {
            int high;
            int low;

            if (read_index + 1 >= length)
                return 0;

            high = isxdigit((unsigned char)source[read_index])
                       ? (isdigit((unsigned char)source[read_index])
                              ? source[read_index] - '0'
                              : tolower((unsigned char)source[read_index]) - 'a' + 10)
                       : -1;
            low = isxdigit((unsigned char)source[read_index + 1])
                      ? (isdigit((unsigned char)source[read_index + 1])
                             ? source[read_index + 1] - '0'
                             : tolower((unsigned char)source[read_index + 1]) - 'a' + 10)
                      : -1;

            if (high < 0 || low < 0)
                return 0;

            value = (unsigned char)((high << 4) | low);
            read_index += 2;
        }

        if (value == '\0' || write_index + 1 >= capacity)
            return 0;

        destination[write_index++] = (char)value;
    }

    destination[write_index] = '\0';
    return 1;
}

static int extract_param(const char *target, const char *key,
                         char *output, size_t output_size)
{
    const char *query = strchr(target, '?');
    size_t key_length = strlen(key);

    if (!output_size)
        return 0;
    output[0] = '\0';

    if (!query)
        return 0;

    query++;
    while (*query)
    {
        const char *pair_end = strchr(query, '&');
        const char *end = pair_end ? pair_end : query + strlen(query);
        const char *equals = (const char *)memchr(query, '=', (size_t)(end - query));

        if (equals && (size_t)(equals - query) == key_length &&
            memcmp(query, key, key_length) == 0)
        {
            return decode_component(equals + 1,
                                    (size_t)(end - equals - 1),
                                    output, output_size);
        }

        if (!pair_end)
            break;
        query = pair_end + 1;
    }

    return 0;
}

static int parse_integer_param(const char *target, const char *key,
                               int minimum, int maximum, int *value)
{
    char text[32];
    char *end = NULL;
    long parsed;

    if (!extract_param(target, key, text, sizeof(text)) || !text[0])
        return 0;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || !end || *end != '\0' || parsed < minimum || parsed > maximum)
        return 0;

    *value = (int)parsed;
    return 1;
}

static int route_is(const char *target, const char *route)
{
    const char *query = strchr(target, '?');
    size_t path_length = query ? (size_t)(query - target) : strlen(target);
    size_t route_length = strlen(route);

    return path_length == route_length && memcmp(target, route, route_length) == 0;
}

static int require_user(SOCKET client, sqlite3 *db, const char *target,
                        int *user_id)
{
    if (!parse_integer_param(target, "user_id", 1, INT_MAX, user_id))
    {
        send_error(client, 400, "A valid user_id is required.");
        return 0;
    }

    if (!user_exists(db, *user_id))
    {
        send_error(client, 401, "The requested user does not exist.");
        return 0;
    }

    return 1;
}

static void append_history_object(JsonBuffer *json, const HistoryEntry *entry)
{
    json_append(json, "{\"input\":");
    json_append_string(json, entry->input);
    json_append(json, ",\"output\":");
    json_append_string(json, entry->output);
    json_append(json, ",\"type\":");
    json_append_string(json, entry->type);
    json_append(json, ",\"created_at\":");
    json_append_string(json, entry->created_at);
    json_append(json, "}");
}

static void handle_login(SOCKET client, sqlite3 *db, const char *target)
{
    char identifier[512];
    char password[512];
    int user_id;

    if (!extract_param(target, "identifier", identifier, sizeof(identifier)) ||
        !extract_param(target, "password", password, sizeof(password)) ||
        !identifier[0] || !password[0])
    {
        send_response(client, 400, "text/plain; charset=utf-8", "LOGIN_FAILED");
        return;
    }

    user_id = check_login(db, identifier, password);
    if (user_id > 0)
    {
        char response[64];
        snprintf(response, sizeof(response), "LOGIN_SUCCESS:%d", user_id);
        send_response(client, 200, "text/plain; charset=utf-8", response);
    }
    else
    {
        send_response(client, 401, "text/plain; charset=utf-8", "LOGIN_FAILED");
    }
}

static void trim_text(char *text)
{
    char *first = text;
    size_t length;

    while (*first && isspace((unsigned char)*first))
        first++;
    if (first != text)
        memmove(text, first, strlen(first) + 1);

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static int valid_display_name(const char *name)
{
    const unsigned char *cursor = (const unsigned char *)name;
    size_t length = strlen(name);

    if (!length || length > 255)
        return 0;
    while (*cursor)
    {
        if (*cursor < 32 || *cursor == 127)
            return 0;
        cursor++;
    }
    return 1;
}

static int valid_username(const char *username)
{
    const unsigned char *cursor = (const unsigned char *)username;
    size_t length = strlen(username);

    if (!length || length > 50)
        return 0;
    while (*cursor)
    {
        if (isspace(*cursor) || *cursor < 32 || *cursor == 127)
            return 0;
        cursor++;
    }
    return 1;
}

static int valid_email_address(const char *email)
{
    const char *at = strchr(email, '@');
    const char *cursor;
    const char *label_start;
    size_t length = strlen(email);
    size_t local_length;
    int has_domain_dot = 0;

    if (length < 3 || length > 254 || !at || at == email ||
        strchr(at + 1, '@') || !at[1])
        return 0;

    local_length = (size_t)(at - email);
    if (local_length > 64 || email[0] == '.' || email[local_length - 1] == '.')
        return 0;

    for (cursor = email; cursor < at; cursor++)
    {
        unsigned char value = (unsigned char)*cursor;
        if (value >= 128 || isspace(value) || iscntrl(value) ||
            !(isalnum(value) || strchr(".!#$%&'*+-/=?^_`{|}~", value)))
            return 0;
        if (*cursor == '.' && cursor + 1 < at && cursor[1] == '.')
            return 0;
    }

    label_start = at + 1;
    for (cursor = label_start; ; cursor++)
    {
        if (*cursor == '.' || *cursor == '\0')
        {
            size_t label_length = (size_t)(cursor - label_start);
            if (!label_length || label_length > 63 ||
                label_start[0] == '-' || cursor[-1] == '-')
                return 0;
            if (*cursor == '.')
                has_domain_dot = 1;
            else
                break;
            label_start = cursor + 1;
        }
        else if (!(isalnum((unsigned char)*cursor) || *cursor == '-'))
        {
            return 0;
        }
    }

    return has_domain_dot;
}

static void handle_signup(SOCKET client, sqlite3 *db, const char *body,
                          size_t body_length)
{
    char form_target[REQUEST_SIZE];
    char name[DB_VALUE_SIZE];
    char username[DB_VALUE_SIZE];
    char email[DB_VALUE_SIZE];
    char password[256];
    char password_hash[PASSWORD_HASH_BUFFER_SIZE];
    int target_length;
    int user_id = 0;
    int result;

    if (!body_length || body_length >= sizeof(form_target))
    {
        send_error(client, 400, "Registration details are required.");
        return;
    }

    target_length = snprintf(form_target, sizeof(form_target), "/signup?%.*s",
                             (int)body_length, body);
    if (target_length < 0 || (size_t)target_length >= sizeof(form_target))
    {
        send_error(client, 400, "Registration details are too long.");
        return;
    }

    if (!extract_param(form_target, "name", name, sizeof(name)) ||
        !extract_param(form_target, "username", username, sizeof(username)) ||
        !extract_param(form_target, "email", email, sizeof(email)) ||
        !extract_param(form_target, "password", password, sizeof(password)))
    {
        send_error(client, 400, "All registration fields are required.");
        return;
    }

    trim_text(name);
    trim_text(username);
    trim_text(email);

    if (!valid_display_name(name))
    {
        send_error(client, 400, "Enter a valid name (up to 255 characters).");
        return;
    }
    if (!valid_username(username))
    {
        send_error(client, 400,
                   "Username is required, must be at most 50 characters, and cannot contain spaces.");
        return;
    }
    if (!valid_email_address(email))
    {
        send_error(client, 400, "Enter a valid email address.");
        return;
    }
    if (!password[0] || strlen(password) > 255)
    {
        send_error(client, 400, "Password is required and must be at most 255 characters.");
        return;
    }

    if (!hash_password(password, password_hash, sizeof(password_hash)))
    {
        memset(password, 0, sizeof(password));
        send_error(client, 500, "Secure password hashing is unavailable.");
        return;
    }
    memset(password, 0, sizeof(password));

    result = register_user(db, name, username, email, password_hash, &user_id);
    memset(password_hash, 0, sizeof(password_hash));

    if (result == USER_REGISTRATION_DUPLICATE_USERNAME)
    {
        send_error(client, 409, "Username already exists.");
        return;
    }
    if (result == USER_REGISTRATION_DUPLICATE_EMAIL)
    {
        send_error(client, 409, "Email already exists.");
        return;
    }
    if (result != SQLITE_OK)
    {
        fprintf(stderr, "User registration failed: SQLite result %d\n", result);
        send_error(client, 500, "The account could not be created.");
        return;
    }

    {
        char response[96];
        snprintf(response, sizeof(response),
                 "{\"ok\":true,\"user_id\":%d}", user_id);
        send_response(client, 201, "application/json; charset=utf-8", response);
    }
}

static void handle_history(SOCKET client, sqlite3 *db, int user_id)
{
    HistoryEntry *entries = NULL;
    size_t entry_count = 0;
    size_t i;
    JsonBuffer json;

    if (load_history(db, user_id, &entries, &entry_count) != SQLITE_OK)
    {
        send_error(client, 500, "Could not load translation history.");
        return;
    }

    json_init(&json);
    json_append(&json, "[");
    for (i = 0; i < entry_count; i++)
    {
        if (i)
            json_append(&json, ",");
        append_history_object(&json, &entries[i]);
    }
    json_append(&json, "]");

    free_history(entries, entry_count);
    send_json(client, 200, &json);
}

static void handle_dashboard(SOCKET client, sqlite3 *db, int user_id)
{
    DashboardStats stats;
    ActivityDay days[7];
    size_t day_count = 0;
    HistoryEntry *entries = NULL;
    size_t entry_count = 0;
    size_t i;
    size_t recent_count;
    JsonBuffer json;

    if (get_dashboard_stats(db, user_id, &stats) != SQLITE_OK ||
        get_daily_activity(db, user_id, days, &day_count) != SQLITE_OK ||
        load_history(db, user_id, &entries, &entry_count) != SQLITE_OK)
    {
        free_history(entries, entry_count);
        send_error(client, 500, "Could not load dashboard data.");
        return;
    }

    json_init(&json);
    json_appendf(&json,
                 "{\"totalTranslations\":%d,\"textToMorse\":%d,"
                 "\"numberToMorse\":%d,\"morseToText\":%d,"
                 "\"morseToNumber\":%d,\"quizAttempts\":%d,"
                 "\"bestQuizScore\":%d,\"latestQuizScore\":%d,"
                 "\"dailyActivity\":[",
                 stats.total_translations, stats.text_to_morse,
                 stats.number_to_morse, stats.morse_to_text,
                 stats.morse_to_number, stats.quiz_attempts,
                 stats.best_quiz_score, stats.latest_quiz_score);

    for (i = 0; i < day_count; i++)
    {
        if (i)
            json_append(&json, ",");
        json_append(&json, "{\"date\":");
        json_append_string(&json, days[i].date);
        json_appendf(&json, ",\"count\":%d}", days[i].count);
    }

    json_append(&json, "],\"recentActivity\":[");
    recent_count = entry_count < 5 ? entry_count : 5;
    for (i = 0; i < recent_count; i++)
    {
        if (i)
            json_append(&json, ",");
        append_history_object(&json, &entries[i]);
    }
    json_append(&json, "]}");

    free_history(entries, entry_count);
    send_json(client, 200, &json);
}

static void handle_profile(SOCKET client, sqlite3 *db, int user_id)
{
    UserProfile profile;
    JsonBuffer json;

    if (get_user_profile(db, user_id, &profile) != SQLITE_OK)
    {
        send_error(client, 500, "Could not load the user profile.");
        return;
    }

    json_init(&json);
    json_append(&json, "{\"id\":");
    json_appendf(&json, "%d,\"name\":", profile.id);
    json_append_string(&json, profile.name);
    json_append(&json, ",\"username\":");
    json_append_string(&json, profile.username);
    json_append(&json, ",\"email\":");
    json_append_string(&json, profile.email);
    json_appendf(&json,
                 ",\"totalTranslations\":%d,\"quizAttempts\":%d,"
                 "\"bestQuizScore\":%d,\"latestQuizScore\":%d}",
                 profile.stats.total_translations,
                 profile.stats.quiz_attempts,
                 profile.stats.best_quiz_score,
                 profile.stats.latest_quiz_score);
    send_json(client, 200, &json);
}

static int get_required_text(SOCKET client, const char *target,
                             const char *key, char *value, size_t capacity)
{
    if (!extract_param(target, key, value, capacity) || !value[0])
    {
        send_error(client, 400, "A required field is missing or too long.");
        return 0;
    }
    return 1;
}

static void send_database_result(SOCKET client, int result,
                                 const char *success_message)
{
    if (result == SQLITE_OK)
    {
        send_response(client, 200, "application/json; charset=utf-8",
                      "{\"ok\":true}");
    }
    else if ((result & 0xff) == SQLITE_CONSTRAINT)
    {
        send_error(client, 409, "Username or email is already in use.");
    }
    else if (result == SQLITE_NOTFOUND)
    {
        send_error(client, 404, "The requested user was not found.");
    }
    else
    {
        fprintf(stderr, "%s: SQLite result %d\n", success_message, result);
        send_error(client, 500, "The database operation failed.");
    }
}

static int header_name_equals(const char *header, size_t header_length,
                              const char *name)
{
    size_t i;
    if (strlen(name) != header_length)
        return 0;

    for (i = 0; i < header_length; i++)
    {
        if (tolower((unsigned char)header[i]) !=
            tolower((unsigned char)name[i]))
            return 0;
    }
    return 1;
}

static int read_content_length(const char *request, size_t header_length,
                               size_t *content_length)
{
    const char *headers_end = request + header_length - 4;
    const char *line = strstr(request, "\r\n");
    int found = 0;

    *content_length = 0;
    if (!line || line >= headers_end)
        return 1;
    line += 2;

    while (line < headers_end)
    {
        const char *line_end = strstr(line, "\r\n");
        const char *colon;
        const char *value;
        const char *value_end;
        size_t parsed = 0;

        if (!line_end || line_end > headers_end || line_end == line)
            break;
        colon = (const char *)memchr(line, ':', (size_t)(line_end - line));
        if (!colon)
            return 0;

        if (header_name_equals(line, (size_t)(colon - line), "Content-Length"))
        {
            if (found)
                return 0;
            found = 1;
            value = colon + 1;
            while (value < line_end && (*value == ' ' || *value == '\t'))
                value++;
            value_end = line_end;
            while (value_end > value &&
                   (value_end[-1] == ' ' || value_end[-1] == '\t'))
                value_end--;
            if (value == value_end)
                return 0;

            while (value < value_end)
            {
                unsigned char digit = (unsigned char)*value++;
                if (!isdigit(digit) || parsed > ((size_t)-1 - 9) / 10)
                    return 0;
                parsed = parsed * 10 + (size_t)(digit - '0');
            }
            *content_length = parsed;
        }
        line = line_end + 2;
    }

    return 1;
}

static int request_has_form_content_type(const char *request,
                                         size_t header_length)
{
    const char *headers_end = request + header_length - 4;
    const char *line = strstr(request, "\r\n");
    static const char expected[] = "application/x-www-form-urlencoded";

    if (!line || line >= headers_end)
        return 0;
    line += 2;

    while (line < headers_end)
    {
        const char *line_end = strstr(line, "\r\n");
        const char *colon;
        const char *value;
        size_t i;

        if (!line_end || line_end > headers_end || line_end == line)
            break;
        colon = (const char *)memchr(line, ':', (size_t)(line_end - line));
        if (colon && header_name_equals(line, (size_t)(colon - line),
                                        "Content-Type"))
        {
            value = colon + 1;
            while (value < line_end && (*value == ' ' || *value == '\t'))
                value++;
            for (i = 0; i < sizeof(expected) - 1; i++)
            {
                if (value + i >= line_end ||
                    tolower((unsigned char)value[i]) != expected[i])
                    return 0;
            }
            value += sizeof(expected) - 1;
            return value == line_end || *value == ';' ||
                   *value == ' ' || *value == '\t';
        }
        line = line_end + 2;
    }

    return 0;
}

static int read_request(SOCKET client, char request[REQUEST_SIZE],
                        size_t *header_length, size_t *body_length)
{
    size_t used = 0;
    size_t expected_length = 0;
    int headers_read = 0;

    *header_length = 0;
    *body_length = 0;

    while (used < REQUEST_SIZE - 1)
    {
        int received = recv(client, request + used,
                            (int)(REQUEST_SIZE - used - 1), 0);
        if (received <= 0)
            return 0;

        used += (size_t)received;
        request[used] = '\0';

        if (!headers_read)
        {
            const char *separator = strstr(request, "\r\n\r\n");
            if (separator)
            {
                *header_length = (size_t)(separator - request) + 4;
                if (!read_content_length(request, *header_length, body_length) ||
                    *body_length > REQUEST_SIZE - *header_length - 1)
                    return 0;
                expected_length = *header_length + *body_length;
                headers_read = 1;
            }
        }

        if (headers_read && used >= expected_length)
        {
            request[expected_length] = '\0';
            return 1;
        }
    }

    return 0;
}

static void handle_request(SOCKET client, const char *request,
                           size_t header_length, size_t body_length,
                           sqlite3 *db)
{
    char method[8];
    char target[REQUEST_SIZE];
    int user_id;

    if (sscanf(request, "%7s %16383s", method, target) != 2)
    {
        send_error(client, 400, "Malformed HTTP request.");
        return;
    }

    if (strcmp(method, "OPTIONS") == 0)
    {
        send_response(client, 204, "text/plain; charset=utf-8", "");
        return;
    }

    if (strcmp(method, "POST") == 0)
    {
        if (route_is(target, "/signup"))
        {
            if (!request_has_form_content_type(request, header_length))
            {
                send_error(client, 400,
                           "Registration must use form URL-encoded data.");
                return;
            }

            handle_signup(client, db, request + header_length, body_length);
            return;
        }

        send_error(client, 404, "Unknown endpoint.");
        return;
    }

    if (strcmp(method, "GET") != 0)
    {
        send_error(client, 405, "This HTTP method is not supported.");
        return;
    }

    if (route_is(target, "/login"))
    {
        handle_login(client, db, target);
        return;
    }

    if (route_is(target, "/text-to-morse") ||
        route_is(target, "/number-to-morse") ||
        route_is(target, "/morse-to-text") ||
        route_is(target, "/morse-to-number"))
    {
        char input[8192];
        char *result = NULL;
        const char *translation_type;

        if (!require_user(client, db, target, &user_id))
            return;
        if (!extract_param(target, "input", input, sizeof(input)))
        {
            send_error(client, 400, "A valid input value is required.");
            return;
        }

        if (route_is(target, "/text-to-morse"))
        {
            result = text_to_morse(input);
            translation_type = "text_to_morse";
        }
        else if (route_is(target, "/number-to-morse"))
        {
            result = number_to_morse(input);
            translation_type = "number_to_morse";
        }
        else if (route_is(target, "/morse-to-text"))
        {
            result = morse_to_text(input);
            translation_type = "morse_to_text";
        }
        else
        {
            result = morse_to_number(input);
            translation_type = "morse_to_number";
        }

        if (!result)
        {
            send_error(client, 500, "The translation could not be generated.");
            return;
        }

        if (!save_history(db, user_id, input, result, translation_type))
        {
            free(result);
            send_error(client, 500, "The translation was not saved to history.");
            return;
        }

        send_response(client, 200, "text/plain; charset=utf-8", result);
        free(result);
        return;
    }

    if (route_is(target, "/history"))
    {
        if (require_user(client, db, target, &user_id))
            handle_history(client, db, user_id);
        return;
    }

    if (route_is(target, "/dashboard"))
    {
        if (require_user(client, db, target, &user_id))
            handle_dashboard(client, db, user_id);
        return;
    }

    if (route_is(target, "/profile"))
    {
        if (require_user(client, db, target, &user_id))
            handle_profile(client, db, user_id);
        return;
    }

    if (route_is(target, "/update-profile"))
    {
        char name[DB_VALUE_SIZE];
        char username[DB_VALUE_SIZE];
        char email[DB_VALUE_SIZE];
        int result;

        if (!require_user(client, db, target, &user_id) ||
            !get_required_text(client, target, "name", name, sizeof(name)) ||
            !get_required_text(client, target, "username", username, sizeof(username)) ||
            !get_required_text(client, target, "email", email, sizeof(email)))
            return;

        result = update_user_profile(db, user_id, name, username, email);
        send_database_result(client, result, "Profile update");
        return;
    }

    if (route_is(target, "/update-account"))
    {
        char username[DB_VALUE_SIZE];
        char email[DB_VALUE_SIZE];
        int result;

        if (!require_user(client, db, target, &user_id) ||
            !get_required_text(client, target, "username", username, sizeof(username)) ||
            !get_required_text(client, target, "email", email, sizeof(email)))
            return;

        result = update_user_account(db, user_id, username, email);
        send_database_result(client, result, "Account update");
        return;
    }

    if (route_is(target, "/change-password"))
    {
        char password[DB_VALUE_SIZE];
        int result;

        if (!require_user(client, db, target, &user_id) ||
            !get_required_text(client, target, "password", password, sizeof(password)))
            return;

        result = change_user_password(db, user_id, password);
        send_database_result(client, result, "Password update");
        return;
    }

    if (route_is(target, "/clear-history"))
    {
        int deleted_count = 0;
        int result;

        if (!require_user(client, db, target, &user_id))
            return;

        result = clear_user_history(db, user_id, &deleted_count);
        if (result != SQLITE_OK)
        {
            send_error(client, 500, "Could not clear translation history.");
            return;
        }

        {
            char body[96];
            snprintf(body, sizeof(body), "{\"ok\":true,\"deleted\":%d}", deleted_count);
            send_response(client, 200, "application/json; charset=utf-8", body);
        }
        return;
    }

    if (route_is(target, "/save-quiz"))
    {
        int score;

        if (!require_user(client, db, target, &user_id))
            return;

        if (!parse_integer_param(target, "score", 0, 100, &score))
        {
            send_error(client, 400, "A quiz score from 0 to 100 is required.");
            return;
        }

        if (save_quiz_score(db, user_id, score) != SQLITE_OK)
        {
            send_error(client, 500, "Could not save the quiz score.");
            return;
        }

        send_response(client, 200, "application/json; charset=utf-8",
                      "{\"ok\":true}");
        return;
    }

    send_error(client, 404, "Unknown endpoint.");
}

int main(void)
{
    sqlite3 *db = NULL;
    SOCKET server_socket;
    struct sockaddr_in address;
    int option = 1;

    if (!open_database(&db) || !create_tables(db))
    {
        close_database(db);
        return 1;
    }

#ifdef _WIN32
    {
        WSADATA winsock_data;
        if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        {
            fprintf(stderr, "WSAStartup failed.\n");
            close_database(db);
            return 1;
        }
    }
#endif

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Socket creation failed.\n");
        close_database(db);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(PORT);

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&option, sizeof(option));

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(server_socket, 10) < 0)
    {
        fprintf(stderr, "Could not start the backend on 127.0.0.1:%d.\n", PORT);
        closesocket(server_socket);
        close_database(db);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("Morse backend running at http://127.0.0.1:%d\n", PORT);
    printf("Using SQLite database: morse.db\n");

    for (;;)
    {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);
        SOCKET client = accept(server_socket,
                               (struct sockaddr *)&client_address,
                               &client_length);

        if (client == INVALID_SOCKET)
            continue;

        {
            char request[REQUEST_SIZE];
            size_t header_length = 0;
            size_t body_length = 0;
            if (read_request(client, request, &header_length, &body_length))
                handle_request(client, request, header_length, body_length, db);
            else
                send_error(client, 400, "Could not read the HTTP request.");
            memset(request, 0, sizeof(request));
        }

        closesocket(client);
    }

    return 0;
}
