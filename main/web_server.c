#include "web_server.h"
#include "storage.h"
#include "constants.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "cJSON.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Embedded certificate and private key
extern const uint8_t cacert_pem_start[] asm("_binary_cacert_pem_start");
extern const uint8_t cacert_pem_end[]   asm("_binary_cacert_pem_end");
extern const uint8_t prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");


// Serve static files from SPIFFS
static esp_err_t static_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Static handler: %s %s", http_method_str(req->method), req->uri);
    
    char filepath[600];
    
    // Default to index.html
    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", SPIFFS_BASE_PATH);
    } else {
        // Truncate URI if needed to prevent overflow
        size_t uri_len = strlen(req->uri);
        if (uri_len > 512) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        snprintf(filepath, sizeof(filepath), "%s%s", SPIFFS_BASE_PATH, req->uri);
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set content type
    if (strstr(filepath, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(filepath, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filepath, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    }

    // Send file in chunks
    char chunk[512];
    size_t read_bytes;
    do {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (read_bytes > 0) {
            httpd_resp_send_chunk(req, chunk, read_bytes);
        }
    } while (read_bytes > 0);

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// GET /api/notes - List all notes
static esp_err_t api_list_notes_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Listing notes request received");
    
    // Allocate notes array dynamically to avoid stack overflow
    note_metadata_t *notes = malloc(10 * sizeof(note_metadata_t));
    if (!notes) {
        ESP_LOGE(TAG, "Failed to allocate memory for notes list");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Out of memory\"}");
        return ESP_FAIL;
    }
    
    size_t count = 0;
    esp_err_t err = storage_list_notes(notes, 10, &count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to list notes: %s", esp_err_to_name(err));
        free(notes);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to list notes\"}");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Found %d notes", count);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        free(notes);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Out of memory\"}");
        return ESP_FAIL;
    }
    
    cJSON *notes_array = cJSON_CreateArray();
    if (!notes_array) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        cJSON_Delete(root);
        free(notes);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Out of memory\"}");
        return ESP_FAIL;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON *note = cJSON_CreateObject();
        if (!note) {
            ESP_LOGE(TAG, "Failed to create note object for index %d", i);
            continue;
        }
        cJSON_AddStringToObject(note, "id", notes[i].id);
        cJSON_AddStringToObject(note, "title", notes[i].title);
        cJSON_AddNumberToObject(note, "timestamp", (double)notes[i].timestamp);
        cJSON_AddBoolToObject(note, "encrypted", notes[i].encrypted);
        cJSON_AddItemToArray(notes_array, note);
    }

    cJSON_AddItemToObject(root, "notes", notes_array);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(notes);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize JSON response");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to serialize response\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

// POST /api/notes - Create new note
static esp_err_t api_create_note_handler(httpd_req_t *req)
{
    char content[MAX_NOTE_SIZE_BYTES + 512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid JSON\"}");
        return ESP_FAIL;
    }

    cJSON *title_item = cJSON_GetObjectItem(json, "title");
    cJSON *message_item = cJSON_GetObjectItem(json, "message");
    cJSON *encrypted_item = cJSON_GetObjectItem(json, "encrypted");

    if (!title_item || !message_item) {
        cJSON_Delete(json);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Missing required fields\"}");
        return ESP_FAIL;
    }

    bool encrypted = encrypted_item ? cJSON_IsTrue(encrypted_item) : false;

    char note_id[16];
    esp_err_t err = storage_create_note(title_item->valuestring,
                                        message_item->valuestring,
                                        encrypted,
                                        note_id);
    cJSON_Delete(json);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create note: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to create note\"}");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Note created successfully: %s", note_id);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "id", note_id);
    cJSON_AddStringToObject(response, "status", "created");

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize JSON response");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to serialize response\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

// GET /api/notes/{id} - Read note (returns encrypted message if encrypted)
static esp_err_t api_read_note_handler(httpd_req_t *req)
{
    // Extract note ID from URI
    char note_id[16];
    const char *uri = req->uri;
    sscanf(uri, "/api/notes/%s", note_id);

    // Read note
    char message[MAX_NOTE_SIZE_BYTES];
    size_t message_len = sizeof(message);
    note_metadata_t metadata;

    esp_err_t err = storage_read_note(note_id, message, &message_len, &metadata);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Note not found\"}");
        return ESP_FAIL;
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "id", metadata.id);
    cJSON_AddStringToObject(response, "title", metadata.title);
    cJSON_AddNumberToObject(response, "timestamp", (double)metadata.timestamp);
    cJSON_AddBoolToObject(response, "encrypted", metadata.encrypted);
    cJSON_AddStringToObject(response, "message", message);

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize read response");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to serialize response\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    ESP_LOGI(TAG, "Read note %s (encrypted=%d)", note_id, metadata.encrypted);
    return ESP_OK;
}

// DELETE /api/notes/{id} - Delete note
static esp_err_t api_delete_note_handler(httpd_req_t *req)
{
    char note_id[16];
    const char *uri = req->uri;
    sscanf(uri, "/api/notes/%15s", note_id);
    
    // Remove any query parameters or trailing slashes
    char *query = strchr(note_id, '?');
    if (query) *query = '\0';
    char *slash = strchr(note_id, '/');
    if (slash) *slash = '\0';
    
    ESP_LOGI(TAG, "Deleting note: %s", note_id);

    esp_err_t err = storage_delete_note(note_id);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to delete note\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"deleted\"}");
    return ESP_OK;
}

// POST /api/time - Sync time from client
static esp_err_t api_time_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid JSON\"}");
        return ESP_FAIL;
    }

    cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
    if (!timestamp || !cJSON_IsNumber(timestamp)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Missing timestamp\"}");
        return ESP_FAIL;
    }

    // Set system time
    struct timeval tv = {
        .tv_sec = (time_t)timestamp->valueint,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);

    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Time synchronized to %ld", (long)tv.tv_sec);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// GET /api/stats - Get storage statistics
static esp_err_t api_stats_handler(httpd_req_t *req)
{
    storage_stats_t stats;
    esp_err_t err = storage_get_stats(&stats);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to get stats\"}");
        return ESP_FAIL;
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "count", stats.count);
    cJSON_AddNumberToObject(response, "total", stats.total);
    cJSON_AddNumberToObject(response, "used", stats.used);

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize stats response");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to serialize response\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }

    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.stack_size = 8192;
    config.httpd.max_uri_handlers = 10;
    config.httpd.uri_match_fn = httpd_uri_match_wildcard;
    
    // Set certificate and private key
    config.servercert = cacert_pem_start;
    config.servercert_len = cacert_pem_end - cacert_pem_start;
    config.prvtkey_pem = prvtkey_pem_start;
    config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    
    ESP_LOGI(TAG, "Certificate length: %zu bytes", config.servercert_len);
    ESP_LOGI(TAG, "Private key length: %zu bytes", config.prvtkey_len);

    if (httpd_ssl_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTPS server");
        return ESP_FAIL;
    }

    // Register API handlers FIRST (more specific routes)
    httpd_uri_t api_time = {
        .uri = "/api/time",
        .method = HTTP_POST,
        .handler = api_time_handler
    };
    ESP_LOGI(TAG, "Registering handler: POST /api/time");
    httpd_register_uri_handler(server, &api_time);

    httpd_uri_t api_stats = {
        .uri = "/api/stats",
        .method = HTTP_GET,
        .handler = api_stats_handler
    };
    ESP_LOGI(TAG, "Registering handler: GET /api/stats");
    httpd_register_uri_handler(server, &api_stats);

    httpd_uri_t api_list_notes = {
        .uri = "/api/notes",
        .method = HTTP_GET,
        .handler = api_list_notes_handler
    };
    ESP_LOGI(TAG, "Registering handler: GET /api/notes");
    httpd_register_uri_handler(server, &api_list_notes);

    httpd_uri_t api_create_note = {
        .uri = "/api/notes",
        .method = HTTP_POST,
        .handler = api_create_note_handler
    };
    ESP_LOGI(TAG, "Registering handler: POST /api/notes");
    httpd_register_uri_handler(server, &api_create_note);

    httpd_uri_t api_read_note = {
        .uri = "/api/notes/*",
        .method = HTTP_GET,
        .handler = api_read_note_handler
    };
    ESP_LOGI(TAG, "Registering handler: GET /api/notes/*");
    httpd_register_uri_handler(server, &api_read_note);

    httpd_uri_t api_delete_note = {
        .uri = "/api/notes/*",
        .method = HTTP_DELETE,
        .handler = api_delete_note_handler
    };
    ESP_LOGI(TAG, "Registering handler: DELETE /api/notes/*");
    httpd_register_uri_handler(server, &api_delete_note);

    // Register static file handler LAST (wildcard, catches remaining)
    httpd_uri_t static_files = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_handler
    };
    ESP_LOGI(TAG, "Registering handler: GET /*");
    httpd_register_uri_handler(server, &static_files);

    ESP_LOGI(TAG, "HTTPS server started on port 443");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (server == NULL) {
        ESP_LOGW(TAG, "Web server not running");
        return ESP_OK;
    }

    httpd_ssl_stop(server);
    server = NULL;

    ESP_LOGI(TAG, "Web server stopped");
    return ESP_OK;
}
