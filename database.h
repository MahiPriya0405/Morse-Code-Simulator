#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>
#include "sqlite3.h"

#define DB_VALUE_SIZE 512

typedef struct {
    char *input;
    char *output;
    char *type;
    char *created_at;
} HistoryEntry;

typedef struct {
    char date[16];
    int count;
} ActivityDay;

typedef struct {
    int total_translations;
    int text_to_morse;
    int number_to_morse;
    int morse_to_text;
    int morse_to_number;
    int quiz_attempts;
    int best_quiz_score;
    int latest_quiz_score;
} DashboardStats;

typedef struct {
    int id;
    char name[DB_VALUE_SIZE];
    char username[DB_VALUE_SIZE];
    char email[DB_VALUE_SIZE];
    DashboardStats stats;
} UserProfile;

enum {
    USER_REGISTRATION_DUPLICATE_USERNAME = -1001,
    USER_REGISTRATION_DUPLICATE_EMAIL = -1002
};

int open_database(sqlite3 **db);
int create_tables(sqlite3 *db);
void close_database(sqlite3 *db);

int user_exists(sqlite3 *db, int user_id);
int check_login(sqlite3 *db, const char *identifier, const char *password);
int register_user(sqlite3 *db, const char *name, const char *username,
                  const char *email, const char *password_hash, int *user_id);
int save_history(sqlite3 *db, int user_id, const char *input_text,
                 const char *output_text, const char *translation_type);
int load_history(sqlite3 *db, int user_id, HistoryEntry **entries,
                 size_t *entry_count);
void free_history(HistoryEntry *entries, size_t entry_count);

int get_dashboard_stats(sqlite3 *db, int user_id, DashboardStats *stats);
int get_daily_activity(sqlite3 *db, int user_id, ActivityDay days[7],
                       size_t *day_count);
int get_user_profile(sqlite3 *db, int user_id, UserProfile *profile);

int update_user_profile(sqlite3 *db, int user_id, const char *name,
                        const char *username, const char *email);
int update_user_account(sqlite3 *db, int user_id, const char *username,
                        const char *email);
int change_user_password(sqlite3 *db, int user_id, const char *password);
int clear_user_history(sqlite3 *db, int user_id, int *deleted_count);
int save_quiz_score(sqlite3 *db, int user_id, int score_percent);

#endif
