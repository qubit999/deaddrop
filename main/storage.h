#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"
#include "constants.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Note metadata structure
typedef struct {
    char id[16];
    char title[MAX_TITLE_LENGTH];
    uint64_t timestamp;
    bool encrypted;  // Flag to indicate if message is encrypted
} note_metadata_t;

// Storage statistics
typedef struct {
    uint32_t count;
    size_t total;
    size_t used;
} storage_stats_t;

/**
 * Initialize storage system (mount SPIFFS, init NVS)
 */
esp_err_t storage_init(void);

/**
 * Create a new note (message is already encrypted client-side if password was used)
 * 
 * @param title Public title
 * @param message Message content (plain or encrypted)
 * @param encrypted Flag indicating if message is encrypted
 * @param note_id Output buffer for generated note ID (min 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t storage_create_note(const char *title, const char *message, 
                               bool encrypted, char *note_id);

/**
 * Get list of all notes (metadata only)
 * 
 * @param notes Output array of note metadata
 * @param max_notes Maximum number of notes to return
 * @param count Output: actual number of notes returned
 * @return ESP_OK on success
 */
esp_err_t storage_list_notes(note_metadata_t *notes, size_t max_notes, size_t *count);

/**
 * Read a note (returns encrypted message if encrypted, plain if not)
 * 
 * @param note_id Note ID
 * @param message Output buffer for message content
 * @param message_len Size of output buffer / actual message length
 * @param metadata Output: note metadata
 * @return ESP_OK on success
 */
esp_err_t storage_read_note(const char *note_id, char *message, size_t *message_len,
                             note_metadata_t *metadata);

/**
 * Delete a note
 * 
 * @param note_id Note ID
 * @return ESP_OK on success
 */
esp_err_t storage_delete_note(const char *note_id);

/**
 * Get storage statistics
 * 
 * @param stats Output: storage statistics
 * @return ESP_OK on success
 */
esp_err_t storage_get_stats(storage_stats_t *stats);

#endif // STORAGE_H
