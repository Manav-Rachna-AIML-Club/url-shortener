#include <stdio.h>
#include "include/sqlite3.h"

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

    printf("database started");

    return 0;
}

