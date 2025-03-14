#include <stdio.h>
#include "include/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define URL_MAX_LENGTH 2048
#define SHORT_URL_LENGTH 7

const char base62_chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// encoding to base62
// take large URL, and convert random number to base62, store taht as short code
// youtube.com/fjaoeifeawjeifjefiojewoijf
// website.com/nfasda
void encode_base62(long long n, char* output, int length){
    // build string buffer
    for(int i = 0; i < length; i++){
        output[i] = base62_chars[0];
    }

    output[length] = '\0';

    // conversion 
    int end = length - 1;

    while(n > 0 && end >= 0){
        output[end--] = base62_chars[n%62];
        n = n / 62;
    }
}

char* get_original_url(const char* short_code){
    sqlite3* db;
    int return_code;
    sqlite3_stmt *stmt;
    char* original_url = NULL;

    return_code = sqlite3_open("urlshortener.db", &db);

    if(return_code != SQLITE_OK){
        fprintf(stderr, "could not open database%s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    const char* sql = "SELECT original_url FROM urls WHERE short_code=?;";
    return_code = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if(return_code != SQLITE_OK){
        fprintf(stderr, "sql prepare error%s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, short_code, -1, SQLITE_STATIC);
    return_code = sqlite3_step(stmt);
    if(return_code == SQLITE_ROW){
        const char* url = (const char*)sqlite3_column_text(stmt, 0);
        original_url = malloc(strlen(url)+1);
        if(original_url) strcpy(original_url, url);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return original_url;
}

char* shorten_url(const char *url){
    sqlite3 *db;
    int rc;
    sqlite3_stmt *stmt;
    char *short_code = malloc(SHORT_URL_LENGTH + 1);
    if (!short_code) {
        return NULL;
    }
    
    rc = sqlite3_open("urlshortener.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(short_code);
        return NULL;
    }
    
    // Check if the URL already exists
    const char *check_sql = "SELECT short_code FROM urls WHERE original_url = ?;";
    rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(short_code);
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, url, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // URL already exists, return existing short code
        strncpy(short_code, (const char*)sqlite3_column_text(stmt, 0), SHORT_URL_LENGTH);
        short_code[SHORT_URL_LENGTH] = '\0';
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return short_code;
    }
    sqlite3_finalize(stmt);
    
    // Generate a unique short code using timestamp + random
    long long code_number = time(NULL) * 100000 + rand() % 100000;
    encode_base62(code_number, short_code, SHORT_URL_LENGTH);
    
    // Insert the new URL
    const char *insert_sql = "INSERT INTO urls (original_url, short_code) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(short_code);
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, url, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, short_code, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL execution error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        free(short_code);
        return NULL;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return short_code;
}

// connecting the database -> sqlite3.db
int init_database(){
    sqlite3* db;
    int return_code; // it will tell success or failure
    char *err_msg = NULL;

    return_code = sqlite3_open("urlshortener.db", &db);

    if(return_code != SQLITE_OK){
        fprintf(stderr, "cannot open %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS urls ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "original_url TEXT NOT NULL,"
    "short_code TEXT NOT NULL UNIQUE,"
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";

    return_code = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if(return_code != SQLITE_OK){
        fprintf(stderr, "sql error %s\n", err_msg);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}

// starting point
int main(int argc, char* argv[]){

    int db = init_database();

    if(db != 0){
        fprintf(stderr, "could not start database\n");
        return 1;
    }

    char* answer;
    srand((unsigned int)time(NULL));
// swiJLCl
    // answer = shorten_url("https://open.spotify.com/user/31p3tj7cw4mwanhj4h6yvzhqfzbq");

    answer = get_original_url("swiJLCl");

    printf("%s\n", answer);

    return 0;
}