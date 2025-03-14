#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/sqlite3.h"
#include <time.h>
#include <microhttpd.h>
#include "include/cJSON.h"

#define DB_FILE "/root/urlshortener.db"
#define URL_MAX_LENGTH 2048
#define SHORT_URL_LENGTH 7
#define PORT 8080

// Base62 character set (0-9, a-z, A-Z)
const char base62_chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Function to encode an integer to base62 string
void encode_base62(long long n, char *output, int length) {
    // Fill the output buffer with padding character
    for (int i = 0; i < length; i++) {
        output[i] = base62_chars[0];
    }
    output[length] = '\0';

    // Convert the number to base62
    int pos = length - 1;
    while (n > 0 && pos >= 0) {
        output[pos--] = base62_chars[n % 62];
        n /= 62;
    }
}

// Initialize the database
int init_database() {
    sqlite3 *db;
    int rc;
    char *err_msg = NULL;
    
    rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    const char *sql = "CREATE TABLE IF NOT EXISTS urls ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "original_url TEXT NOT NULL,"
                      "short_code TEXT NOT NULL UNIQUE,"
                      "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
    
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    
    sqlite3_close(db);
    return 0;
}

// Store a URL and return the generated short code
char* shorten_url(const char *url) {
    sqlite3 *db;
    int rc;
    sqlite3_stmt *stmt;
    char *short_code = malloc(SHORT_URL_LENGTH + 1);
    if (!short_code) {
        return NULL;
    }
    
    rc = sqlite3_open(DB_FILE, &db);
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

// Get original URL from short code
char* get_original_url(const char *short_code) {
    sqlite3 *db;
    int rc;
    sqlite3_stmt *stmt;
    char *original_url = NULL;
    
    rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }
    
    const char *sql = "SELECT original_url FROM urls WHERE short_code = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, short_code, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *url = (const char*)sqlite3_column_text(stmt, 0);
        original_url = malloc(strlen(url) + 1);
        if (original_url) {
            strcpy(original_url, url);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);


    cJSON* json = cJSON_Parse(original_url);
    if (!json) {
        printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }
    
    // Extract the URL
    cJSON* url_item = cJSON_GetObjectItemCaseSensitive(json, "url");
    if (!url_item || !cJSON_IsString(url_item) || !url_item->valuestring) {
        cJSON_Delete(json);
        return NULL;
    }
    
    // Create a copy of the URL string
    char* url = strdup(url_item->valuestring);
    
    // Clean up
    cJSON_Delete(json);
    
    return url;
}

// Print usage instructions
void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s shorten <url>      - Create a short URL\n", program_name);
    printf("  %s lookup <code>      - Get original URL from short code\n", program_name);
}


#define MAX_BODY_SIZE 1024  // Adjust this as needed

struct ConnectionInfo {
    char *body;
    size_t size;
};

static enum MHD_Result request_handler(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    struct MHD_Response *response;
    int ret;
    char buffer[256];

    // Step 1: Allocate memory for the request body (only on first call)
    if (*con_cls == NULL) {
        struct ConnectionInfo *conn_info = malloc(sizeof(struct ConnectionInfo));
        if (!conn_info) return MHD_NO;
        conn_info->body = malloc(MAX_BODY_SIZE);
        conn_info->size = 0;
        *con_cls = conn_info;
        return MHD_YES;
    }

    struct ConnectionInfo *conn_info = *con_cls;

    // Step 2: Handle POST request
    if (strcmp(method, "POST") == 0 && strcmp(url, "/shorten") == 0) {
        if (*upload_data_size > 0) {
            if (conn_info->size + *upload_data_size < MAX_BODY_SIZE) {
                memcpy(conn_info->body + conn_info->size, upload_data, *upload_data_size);
                conn_info->size += *upload_data_size;
                conn_info->body[conn_info->size] = '\0';
            }
            *upload_data_size = 0;
            return MHD_YES;
        }

        if (conn_info->size > 0) {
            char *short_code = shorten_url(conn_info->body);
            snprintf(buffer, sizeof(buffer), "Shortened URL: http://165.232.174.202/%s", short_code);
            free(short_code);
        } else {
            snprintf(buffer, sizeof(buffer), "No URL found in body");
        }

        response = MHD_create_response_from_buffer(strlen(buffer), buffer, MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);

        free(conn_info->body);
        free(conn_info);
        return ret;
    }

    // Step 3: Handle GET request
    else if (strncmp(url, "/", 1) == 0 && strcmp(method, "GET") == 0) {
        char *original_url = get_original_url(url + 1);

        if (original_url) {
            response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
            MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
            MHD_add_response_header(response, "Location", original_url);
            ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
            MHD_destroy_response(response);
            free(original_url);
            free(conn_info->body);
            free(conn_info);
            return ret;
        } else {
            snprintf(buffer, sizeof(buffer), "URL not found");
            response = MHD_create_response_from_buffer(strlen(buffer), buffer, MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
            MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
            ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);
            free(conn_info->body);
            free(conn_info);
            return ret;
        }
    }

    // Step 4: Handle OPTIONS request
    else if (strcmp(method, "OPTIONS") == 0) {
        response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        MHD_add_response_header(response, "Access-Control-Max-Age", "86400"); // Cache for 1 day
        ret = MHD_queue_response(connection, MHD_HTTP_NO_CONTENT, response);
        MHD_destroy_response(response);

        free(conn_info->body);
        free(conn_info);
        return ret;
    }

    // Step 5: Handle Unknown Routes
    else {
        response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);

        free(conn_info->body);
        free(conn_info);
        return ret;
    }

}

int main(int argc, char *argv[]) {
    // Seed random number generator
    srand((unsigned int)time(NULL));
    
    // Initialize database
    if (init_database() != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    struct MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, PORT, NULL, NULL, &request_handler, NULL, MHD_OPTION_END);

    if (!daemon) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    printf("Server running on port %d...\nPress Enter to stop.\n", PORT);
    pause();

    MHD_stop_daemon(daemon);
    return 0;
}
