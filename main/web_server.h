#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

/**
 * Initialize and start web server
 */
esp_err_t web_server_start(void);

/**
 * Stop web server
 */
esp_err_t web_server_stop(void);

#endif // WEB_SERVER_H
