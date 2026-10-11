#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "password_hash.h"

static int execute_sql(sqlite3 *db, const char *sql)
{
    char *error_message = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &error_message);

    if (result != SQLITE_OK)
    {
        fprintf(stderr, "SQLite error: %s\n",
                error_message ? error_message : sqlite3_errmsg(db));
        sqlite3_free(error_message);
    }

    return result == SQLITE_OK;
}

static int has_column(sqlite3 *db, const char *table, const char *column)
{
    char sql[128];
    sqlite3_stmt *statement = NULL;
    int found = 0;

    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        if (name && sqlite3_stricmp((const char *)name, column) == 0)
        {
            found = 1;
            break;
        }
    }

    sqlite3_finalize(statement);
    return found;
}

static char *copy_column_text(sqlite3_stmt *statement, int column)
{
    const unsigned char *value = sqlite3_column_text(statement, column);
    const char *source = value ? (const char *)value : "";
    size_t length = strlen(source);
    char *copy = (char *)malloc(length + 1);

    if (copy)
        memcpy(copy, source, length + 1);

    return copy;
}

static void copy_text_to_buffer(char *destination, size_t capacity,
                                sqlite3_stmt *statement, int column)
{
    const unsigned char *value = sqlite3_column_text(statement, column);
    const char *source = value ? (const char *)value : "";

    if (capacity == 0)
        return;

    snprintf(destination, capacity, "%s", source);
}

int open_database(sqlite3 **db)
{
    int result = sqlite3_open("morse.db", db);

    if (result != SQLITE_OK)
    {
        fprintf(stderr, "Database connection failed: %s\n",
                *db ? sqlite3_errmsg(*db) : "unable to allocate database handle");
        if (*db)
            sqlite3_close(*db);
        *db = NULL;
        return 0;
    }

    sqlite3_busy_timeout(*db, 3000);
    printf("Database connected successfully.\n");
    return 1;
}

int create_tables(sqlite3 *db)
{
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS Users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL,"
        "email TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS History ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "input_text TEXT,"
        "morse_code TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS Quiz ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "score INTEGER,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (!execute_sql(db, create_sql))
        return 0;

    if (!has_column(db, "Users", "name") &&
        !execute_sql(db, "ALTER TABLE Users ADD COLUMN name TEXT NOT NULL DEFAULT ''"))
        return 0;

    if (!has_column(db, "History", "translation_type") &&
        !execute_sql(db, "ALTER TABLE History ADD COLUMN translation_type TEXT NOT NULL DEFAULT ''"))
        return 0;

    if (!execute_sql(db,
                     "UPDATE Users SET name = username "
                     "WHERE name IS NULL OR TRIM(name) = ''"))
        return 0;

    if (!execute_sql(db,
                     "UPDATE History SET translation_type = CASE "
                     "WHEN TRIM(COALESCE(input_text, '')) <> '' "
                     "AND TRIM(input_text) NOT GLOB '*[^./ -]*' "
                     "AND (INSTR(input_text, '.') > 0 OR INSTR(input_text, '-') > 0) "
                     "THEN CASE WHEN TRIM(COALESCE(morse_code, '')) <> '' "
                     "AND TRIM(morse_code) NOT GLOB '*[^0-9]*' "
                     "THEN 'morse_to_number' ELSE 'morse_to_text' END "
                     "WHEN TRIM(COALESCE(input_text, '')) <> '' "
                     "AND TRIM(input_text) NOT GLOB '*[^0-9]*' "
                     "THEN 'number_to_morse' ELSE 'text_to_morse' END "
                     "WHERE translation_type IS NULL OR translation_type = ''"))
        return 0;

    if (!execute_sql(db,
                     "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username_nocase "
                     "ON Users(username COLLATE NOCASE)"))
        return 0;

    if (!execute_sql(db,
                     "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email_nocase "
                     "ON Users(email COLLATE NOCASE)"))
        return 0;

    printf("Database tables are ready; existing rows were preserved.\n");
    return 1;
}

void close_database(sqlite3 *db)
{
    if (db)
        sqlite3_close(db);
}

int user_exists(sqlite3 *db, int user_id)
{
    const char *sql = "SELECT 1 FROM Users WHERE id = ? LIMIT 1";
    sqlite3_stmt *statement = NULL;
    int result;

    if (user_id <= 0 ||
        sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(statement, 1, user_id);
    result = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return result;
}

int check_login(sqlite3 *db, const char *identifier, const char *password)
{
    const char *sql =
        "SELECT id, password FROM Users "
        "WHERE email = ? COLLATE NOCASE OR username = ? COLLATE NOCASE "
        "LIMIT 1";
    sqlite3_stmt *statement = NULL;
    int user_id = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(statement, 1, identifier, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, identifier, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *stored_password = sqlite3_column_text(statement, 1);
        if (stored_password &&
            verify_password(password, (const char *)stored_password))
        {
        user_id = sqlite3_column_int(statement, 0);
        }
    }

    sqlite3_finalize(statement);
    return user_id;
}

static int user_field_exists(sqlite3 *db, const char *sql,
                             const char *value, int *exists)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);

    *exists = 0;
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_text(statement, 1, value, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
    {
        *exists = 1;
        result = SQLITE_OK;
    }
    else if (result == SQLITE_DONE)
    {
        result = SQLITE_OK;
    }

    sqlite3_finalize(statement);
    return result;
}

int register_user(sqlite3 *db, const char *name, const char *username,
                  const char *email, const char *password_hash, int *user_id)
{
    const char *username_sql =
        "SELECT 1 FROM Users WHERE username = ? COLLATE NOCASE LIMIT 1";
    const char *email_sql =
        "SELECT 1 FROM Users WHERE email = ? COLLATE NOCASE LIMIT 1";
    const char *insert_sql =
        "INSERT INTO Users (name, username, email, password) "
        "VALUES (?, ?, ?, ?)";
    sqlite3_stmt *statement = NULL;
    int exists = 0;
    int result;

    if (user_id)
        *user_id = 0;
    if (!db || !name || !username || !email || !password_hash || !user_id)
        return SQLITE_MISUSE;

    result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        return result;

    result = user_field_exists(db, username_sql, username, &exists);
    if (result != SQLITE_OK)
        goto rollback;
    if (exists)
    {
        result = USER_REGISTRATION_DUPLICATE_USERNAME;
        goto rollback;
    }

    result = user_field_exists(db, email_sql, email, &exists);
    if (result != SQLITE_OK)
        goto rollback;
    if (exists)
    {
        result = USER_REGISTRATION_DUPLICATE_EMAIL;
        goto rollback;
    }

    result = sqlite3_prepare_v2(db, insert_sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        goto rollback;

    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, password_hash, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    statement = NULL;

    if (result != SQLITE_DONE)
    {
        int username_exists = 0;
        int email_exists = 0;
        int username_result = user_field_exists(
            db, username_sql, username, &username_exists);
        int email_result = user_field_exists(db, email_sql, email, &email_exists);

        if (username_result == SQLITE_OK && username_exists)
            result = USER_REGISTRATION_DUPLICATE_USERNAME;
        else if (email_result == SQLITE_OK && email_exists)
            result = USER_REGISTRATION_DUPLICATE_EMAIL;
        goto rollback;
    }

    *user_id = (int)sqlite3_last_insert_rowid(db);
    result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    if (result == SQLITE_OK)
        return SQLITE_OK;

    *user_id = 0;
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return result;

rollback:
    if (statement)
        sqlite3_finalize(statement);
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return result == SQLITE_DONE ? SQLITE_OK : result;
}

int save_history(sqlite3 *db, int user_id, const char *input_text,
                 const char *output_text, const char *translation_type)
{
    const char *sql =
        "INSERT INTO History (user_id, input_text, morse_code, translation_type) "
        "VALUES (?, ?, ?, ?)";
    sqlite3_stmt *statement = NULL;
    int result;

    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(statement, 1, user_id);
    sqlite3_bind_text(statement, 2, input_text, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, output_text, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, translation_type, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return result;
}

int load_history(sqlite3 *db, int user_id, HistoryEntry **entries,
                 size_t *entry_count)
{
    const char *sql =
        "SELECT input_text, morse_code, translation_type, created_at "
        "FROM History WHERE user_id = ? ORDER BY id DESC";
    sqlite3_stmt *statement = NULL;
    HistoryEntry *items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int result;

    *entries = NULL;
    *entry_count = 0;

    result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);

    while ((result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        if (count == capacity)
        {
            size_t new_capacity = capacity ? capacity * 2 : 16;
            HistoryEntry *grown = (HistoryEntry *)realloc(
                items, new_capacity * sizeof(HistoryEntry));

            if (!grown)
            {
                result = SQLITE_NOMEM;
                break;
            }

            memset(grown + capacity, 0,
                   (new_capacity - capacity) * sizeof(HistoryEntry));
            items = grown;
            capacity = new_capacity;
        }

        items[count].input = copy_column_text(statement, 0);
        items[count].output = copy_column_text(statement, 1);
        items[count].type = copy_column_text(statement, 2);
        items[count].created_at = copy_column_text(statement, 3);
        count++;

        if (!items[count - 1].input || !items[count - 1].output ||
            !items[count - 1].type || !items[count - 1].created_at)
        {
            result = SQLITE_NOMEM;
            break;
        }
    }

    if (result == SQLITE_DONE)
        result = SQLITE_OK;

    sqlite3_finalize(statement);

    if (result != SQLITE_OK)
    {
        free_history(items, count);
        return result;
    }

    *entries = items;
    *entry_count = count;
    return SQLITE_OK;
}

void free_history(HistoryEntry *entries, size_t entry_count)
{
    size_t i;

    for (i = 0; i < entry_count; i++)
    {
        free(entries[i].input);
        free(entries[i].output);
        free(entries[i].type);
        free(entries[i].created_at);
    }

    free(entries);
}

int get_dashboard_stats(sqlite3 *db, int user_id, DashboardStats *stats)
{
    const char *history_sql =
        "SELECT COUNT(*), "
        "COALESCE(SUM(translation_type = 'text_to_morse'), 0), "
        "COALESCE(SUM(translation_type = 'number_to_morse'), 0), "
        "COALESCE(SUM(translation_type = 'morse_to_text'), 0), "
        "COALESCE(SUM(translation_type = 'morse_to_number'), 0) "
        "FROM History WHERE user_id = ?";
    const char *quiz_sql =
        "SELECT COUNT(*), COALESCE(MAX(score), 0), "
        "COALESCE((SELECT score FROM Quiz WHERE user_id = ? ORDER BY id DESC LIMIT 1), 0) "
        "FROM Quiz WHERE user_id = ?";
    sqlite3_stmt *statement = NULL;
    int result;

    memset(stats, 0, sizeof(*stats));

    result = sqlite3_prepare_v2(db, history_sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
    {
        stats->total_translations = sqlite3_column_int(statement, 0);
        stats->text_to_morse = sqlite3_column_int(statement, 1);
        stats->number_to_morse = sqlite3_column_int(statement, 2);
        stats->morse_to_text = sqlite3_column_int(statement, 3);
        stats->morse_to_number = sqlite3_column_int(statement, 4);
        result = SQLITE_OK;
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_OK)
        return result;

    result = sqlite3_prepare_v2(db, quiz_sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    sqlite3_bind_int(statement, 2, user_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
    {
        stats->quiz_attempts = sqlite3_column_int(statement, 0);
        stats->best_quiz_score = sqlite3_column_int(statement, 1);
        stats->latest_quiz_score = sqlite3_column_int(statement, 2);
        result = SQLITE_OK;
    }
    sqlite3_finalize(statement);
    return result;
}

int get_daily_activity(sqlite3 *db, int user_id, ActivityDay days[7],
                       size_t *day_count)
{
    const char *sql =
        "SELECT date(created_at, 'localtime'), COUNT(*) "
        "FROM History WHERE user_id = ? "
        "AND date(created_at, 'localtime') >= date('now', 'localtime', '-6 days') "
        "GROUP BY date(created_at, 'localtime') ORDER BY date(created_at, 'localtime')";
    sqlite3_stmt *statement = NULL;
    int result;

    *day_count = 0;
    result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    while ((result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        if (*day_count >= 7)
            break;
        copy_text_to_buffer(days[*day_count].date,
                            sizeof(days[*day_count].date), statement, 0);
        days[*day_count].count = sqlite3_column_int(statement, 1);
        (*day_count)++;
    }

    if (result == SQLITE_DONE || result == SQLITE_ROW)
        result = SQLITE_OK;

    sqlite3_finalize(statement);
    return result;
}

int get_user_profile(sqlite3 *db, int user_id, UserProfile *profile)
{
    const char *sql =
        "SELECT id, name, username, email FROM Users WHERE id = ?";
    sqlite3_stmt *statement = NULL;
    int result;

    memset(profile, 0, sizeof(*profile));
    result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
    {
        profile->id = sqlite3_column_int(statement, 0);
        copy_text_to_buffer(profile->name, sizeof(profile->name), statement, 1);
        copy_text_to_buffer(profile->username, sizeof(profile->username), statement, 2);
        copy_text_to_buffer(profile->email, sizeof(profile->email), statement, 3);
        result = SQLITE_OK;
    }
    else if (result == SQLITE_DONE)
    {
        result = SQLITE_NOTFOUND;
    }
    sqlite3_finalize(statement);

    if (result != SQLITE_OK)
        return result;

    return get_dashboard_stats(db, user_id, &profile->stats);
}

static int run_user_update(sqlite3 *db, const char *sql,
                           const char *first, const char *second,
                           const char *third, int user_id, int field_count)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
    int parameter = 1;

    if (result != SQLITE_OK)
        return result;

    if (field_count >= 1)
        sqlite3_bind_text(statement, parameter++, first, -1, SQLITE_TRANSIENT);
    if (field_count >= 2)
        sqlite3_bind_text(statement, parameter++, second, -1, SQLITE_TRANSIENT);
    if (field_count >= 3)
        sqlite3_bind_text(statement, parameter++, third, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, parameter, user_id);

    result = sqlite3_step(statement);
    if (result == SQLITE_DONE)
        result = sqlite3_changes(db) > 0 ? SQLITE_OK : SQLITE_NOTFOUND;

    sqlite3_finalize(statement);
    return result;
}

int update_user_profile(sqlite3 *db, int user_id, const char *name,
                        const char *username, const char *email)
{
    return run_user_update(db,
                           "UPDATE Users SET name = ?, username = ?, email = ? WHERE id = ?",
                           name, username, email, user_id, 3);
}

int update_user_account(sqlite3 *db, int user_id, const char *username,
                        const char *email)
{
    return run_user_update(db,
                           "UPDATE Users SET username = ?, email = ? WHERE id = ?",
                           username, email, NULL, user_id, 2);
}

int change_user_password(sqlite3 *db, int user_id, const char *password)
{
    char password_hash[PASSWORD_HASH_BUFFER_SIZE];
    int result;

    if (!hash_password(password, password_hash, sizeof(password_hash)))
        return SQLITE_ERROR;

    result = run_user_update(db,
                             "UPDATE Users SET password = ? WHERE id = ?",
                             password_hash, NULL, NULL, user_id, 1);
    memset(password_hash, 0, sizeof(password_hash));
    return result;
}

int clear_user_history(sqlite3 *db, int user_id, int *deleted_count)
{
    const char *sql = "DELETE FROM History WHERE user_id = ?";
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);

    *deleted_count = 0;
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE)
    {
        *deleted_count = sqlite3_changes(db);
        result = SQLITE_OK;
    }

    sqlite3_finalize(statement);
    return result;
}

int save_quiz_score(sqlite3 *db, int user_id, int score_percent)
{
    const char *sql = "INSERT INTO Quiz (user_id, score) VALUES (?, ?)";
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);

    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int(statement, 1, user_id);
    sqlite3_bind_int(statement, 2, score_percent);
    result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
    sqlite3_finalize(statement);
    return result;
}
