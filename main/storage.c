#include "storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>

static const char *TAG = "storage";
static nvs_handle_t g_nvs_handle;

esp_err_t storage_init(void)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Open NVS namespace
    err = nvs_open("storage", NVS_READWRITE, &g_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    // Configure SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = SPIFFS_MAX_FILES,
        .format_if_mount_failed = true
    };

    // Mount SPIFFS
    err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(err));
        return err;
    }

    // Check SPIFFS info
    size_t total = 0, used = 0;
    err = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    } else {
        ESP_LOGE(TAG, "Failed to get SPIFFS info: %s", esp_err_to_name(err));
    }

    // Verify SPIFFS is writable by testing directory access
    DIR *dir = opendir(SPIFFS_BASE_PATH);
    if (dir) {
        closedir(dir);
        ESP_LOGI(TAG, "SPIFFS directory accessible");
    } else {
        ESP_LOGE(TAG, "Cannot access SPIFFS directory: %s", SPIFFS_BASE_PATH);
    }

    ESP_LOGI(TAG, "Storage initialized");
    return ESP_OK;
}

static uint32_t get_next_note_id(void)
{
    uint32_t counter = 0;
    nvs_get_u32(g_nvs_handle, "note_counter", &counter);
    counter++;
    nvs_set_u32(g_nvs_handle, "note_counter", counter);
    nvs_commit(g_nvs_handle);
    return counter;
}

esp_err_t storage_create_note(const char *title, const char *message,
                               bool encrypted, char *note_id)
{
    if (!title || !message || !note_id) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check max note count (if limit is set)
#if MAX_NOTE_COUNT > 0
    storage_stats_t stats;
    storage_get_stats(&stats);
    if (stats.count >= MAX_NOTE_COUNT) {
        ESP_LOGE(TAG, "Maximum note count reached");
        return ESP_ERR_NO_MEM;
    }
#endif

    // Check message size
    size_t message_len = strlen(message);
    if (message_len > MAX_NOTE_SIZE_BYTES) {
        ESP_LOGE(TAG, "Message too large");
        return ESP_ERR_INVALID_SIZE;
    }

    // Generate note ID
    uint32_t id_num = get_next_note_id();
    snprintf(note_id, 16, "%08" PRIx32, id_num);

    // Create metadata
    note_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.id, note_id, sizeof(meta.id) - 1);
    strncpy(meta.title, title, sizeof(meta.title) - 1);
    meta.timestamp = (uint64_t)time(NULL);
    meta.encrypted = encrypted;

    // Save metadata file
    char meta_path[64];
    snprintf(meta_path, sizeof(meta_path), "%s/note_%s.meta", SPIFFS_BASE_PATH, note_id);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", meta.id);
    cJSON_AddStringToObject(json, "title", meta.title);
    cJSON_AddNumberToObject(json, "timestamp", (double)meta.timestamp);
    cJSON_AddBoolToObject(json, "encrypted", meta.encrypted);

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    FILE *f = fopen(meta_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create metadata file: %s (errno=%d)", meta_path, errno);
        free(json_str);
        return ESP_FAIL;
    }
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);

    // Save message content file (plain or encrypted, as received from client)
    char msg_path[64];
    snprintf(msg_path, sizeof(msg_path), "%s/note_%s.txt", SPIFFS_BASE_PATH, note_id);

    f = fopen(msg_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create message file: %s (errno=%d)", msg_path, errno);
        return ESP_FAIL;
    }
    fprintf(f, "%s", message);
    fclose(f);

    ESP_LOGI(TAG, "Created note %s: meta=%s, msg=%s, encrypted=%d", 
             note_id, meta_path, msg_path, encrypted);
    return ESP_OK;
}

static esp_err_t load_metadata(const char *note_id, note_metadata_t *meta)
{
    char meta_path[64];
    snprintf(meta_path, sizeof(meta_path), "%s/note_%s.meta", SPIFFS_BASE_PATH, note_id);

    FILE *f = fopen(meta_path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Metadata file not found: %s", meta_path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_str = malloc(fsize + 1);
    if (!json_str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(json_str, 1, fsize, f);
    fclose(f);
    json_str[fsize] = 0;

    cJSON *json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse metadata JSON for note %s", note_id);
        return ESP_FAIL;
    }

    memset(meta, 0, sizeof(note_metadata_t));
    
    cJSON *item = cJSON_GetObjectItem(json, "id");
    if (item) strncpy(meta->id, item->valuestring, sizeof(meta->id) - 1);
    
    item = cJSON_GetObjectItem(json, "title");
    if (item) strncpy(meta->title, item->valuestring, sizeof(meta->title) - 1);
    
    item = cJSON_GetObjectItem(json, "timestamp");
    if (item) meta->timestamp = (uint64_t)item->valuedouble;

    item = cJSON_GetObjectItem(json, "encrypted");
    if (item) meta->encrypted = cJSON_IsTrue(item);

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t storage_list_notes(note_metadata_t *notes, size_t max_notes, size_t *count)
{
    if (!notes || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;
    DIR *dir = opendir(SPIFFS_BASE_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SPIFFS directory: %s (errno=%d)", SPIFFS_BASE_PATH, errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Scanning for notes in %s", SPIFFS_BASE_PATH);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_notes) {
        // Look for .meta files
        if (strstr(entry->d_name, ".meta")) {
            ESP_LOGI(TAG, "Found metadata file: %s", entry->d_name);
            // Extract note ID
            char note_id[16];
            sscanf(entry->d_name, "note_%[^.].meta", note_id);
            
            if (load_metadata(note_id, &notes[*count]) == ESP_OK) {
                ESP_LOGI(TAG, "Loaded note: id=%s, title=%s", notes[*count].id, notes[*count].title);
                (*count)++;
            } else {
                ESP_LOGW(TAG, "Failed to load metadata for %s", note_id);
            }
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Found %d notes", *count);
    return ESP_OK;
}

esp_err_t storage_read_note(const char *note_id, char *message, size_t *message_len,
                            note_metadata_t *metadata)
{
    if (!note_id || !message || !message_len || !metadata) {
        return ESP_ERR_INVALID_ARG;
    }

    // Load metadata
    esp_err_t err = load_metadata(note_id, metadata);
    if (err != ESP_OK) {
        return err;
    }

    // Load message content (plain or encrypted)
    char msg_path[64];
    snprintf(msg_path, sizeof(msg_path), "%s/note_%s.txt", SPIFFS_BASE_PATH, note_id);

    FILE *f = fopen(msg_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open message file: %s", msg_path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > (long)*message_len) {
        fclose(f);
        ESP_LOGE(TAG, "Message buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }

    *message_len = fread(message, 1, fsize, f);
    fclose(f);

    message[*message_len] = '\0';  // Null-terminate
    ESP_LOGI(TAG, "Read note %s (encrypted=%d)", note_id, metadata->encrypted);
    return ESP_OK;
}

esp_err_t storage_delete_note(const char *note_id)
{
    if (!note_id) {
        return ESP_ERR_INVALID_ARG;
    }

    char meta_path[64];
    char msg_path[64];
    snprintf(meta_path, sizeof(meta_path), "%s/note_%s.meta", SPIFFS_BASE_PATH, note_id);
    snprintf(msg_path, sizeof(msg_path), "%s/note_%s.txt", SPIFFS_BASE_PATH, note_id);

    unlink(meta_path);
    unlink(msg_path);

    ESP_LOGI(TAG, "Deleted note %s", note_id);
    return ESP_OK;
}

esp_err_t storage_get_stats(storage_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(stats, 0, sizeof(storage_stats_t));

    // Get SPIFFS info
    esp_spiffs_info(SPIFFS_PARTITION_LABEL, &stats->total, &stats->used);

    // Count notes
    DIR *dir = opendir(SPIFFS_BASE_PATH);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".meta")) {
                stats->count++;
            }
        }
        closedir(dir);
    }

    return ESP_OK;
}
