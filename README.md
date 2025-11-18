# DeadDrop

A secure, BLE-activated encrypted messaging device built on ESP32-S3. DeadDrop allows you to leave encrypted messages that can only be accessed by connecting to the device via Bluetooth and WiFi.

## Features

- **BLE-Triggered WiFi Access**: Device starts in Bluetooth-only mode. Connecting via BLE activates the WiFi access point (15 seconds grace period after disconnect).
- **Client-Side Encryption**: Messages are encrypted in the browser using AES-256-GCM with PBKDF2 key derivation. The server never sees your plaintext or password.
- **Optional Password Protection**: Messages can be stored as plain text or encrypted with a password.
- **HTTPS Web Interface**: Self-signed certificate provides secure context for Web Crypto API.
- **Automatic Time Sync**: Device time is automatically synchronized from the browser on page load.
- **SPIFFS Storage**: Notes stored on 14MB flash partition.

## Hardware Requirements

- ESP32-S3 with at least 16MB flash and 8MB PSRAM
- USB cable for programming

## Software Requirements

- ESP-IDF v5.5.1 or later
- Python 3.x

## Setup

1. **Install ESP-IDF**

   Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) to install ESP-IDF v5.5.1.

2. **Clone/Download Project**

   ```bash
   cd /path/to/deaddrop
   ```

3. **Generate SSL Certificate**

   Run the certificate generation script:

   ```bash
   ./generate_cert.sh
   ```

   This creates a self-signed certificate valid for 10 years in `main/certs/`.

4. **Set ESP-IDF Environment**

   ```bash
   . $HOME/esp/v5.5.1/esp-idf/export.sh
   ```

## Building and Flashing

1. **Build the project**

   ```bash
   idf.py build
   ```

2. **Flash to device**

   Connect your ESP32-S3 via USB and flash:

   ```bash
   idf.py flash
   ```

3. **Monitor serial output** (optional)

   ```bash
   idf.py monitor
   ```

   Press `Ctrl+]` to exit monitor.

## Usage

### Initial Setup

1. Power on the device. It will start in BLE-only mode.
2. Look for a Bluetooth device named "DeadDrop" on your phone/computer.
3. Connect to the BLE device. This triggers the WiFi access point.

### Accessing the Web Interface

1. After BLE connection, the WiFi AP "DeadDrop" becomes available.
2. Connect to the WiFi network (open, no password).
3. Open a browser and navigate to: `https://192.168.4.1`
4. Accept the self-signed certificate warning.

### Creating Messages

1. Click "+ New Message"
2. Enter a title (publicly visible in the message list)
3. Enter your message content
4. **Optional**: Enter a password to encrypt the message
   - Leave password blank for plain text storage
   - Encrypted messages show a 🔒 icon
5. Click "Create"

### Reading Messages

- **Plain text messages**: Click to view immediately
- **Encrypted messages**: Click, then enter the password to decrypt

### Deleting Messages

1. Open a message
2. Click "Delete" button
3. Confirm deletion

### Power Saving

The device automatically disables WiFi 15 seconds after BLE disconnection. To re-enable:
1. Disconnect from BLE
2. Reconnect to BLE device
3. WiFi will stay active as long as BLE remains connected

## Storage Capacity

- Maximum 4096 bytes per message
- ~14MB total storage (exact capacity depends on metadata overhead)
- Storage stats shown at bottom of web interface

## Security Notes

- All encryption happens in the browser using Web Crypto API
- The ESP32 server never sees your password or plaintext
- Encrypted messages are stored as AES-256-GCM ciphertext with salt and IV
- PBKDF2 with 10,000 iterations derives encryption keys from passwords
- HTTPS prevents man-in-the-middle attacks (though certificate is self-signed)

## Configuration

Edit `main/constants.h` to customize:

- `DEVICE_NAME_BLE`: Bluetooth device name
- `WIFI_AP_SSID`: WiFi access point name
- `WIFI_AP_PASSWORD`: WiFi password (empty = open network)
- `WIFI_AP_IP`: Server IP address
- `MAX_NOTE_SIZE_BYTES`: Maximum message size
- Grace period and other timeouts

## Project Structure

```
deaddrop/
├── main/
│   ├── main.c              # Main application logic
│   ├── ble.c               # Bluetooth Low Energy handling
│   ├── wifi_ap.c           # WiFi access point
│   ├── web_server.c        # HTTPS server
│   ├── storage.c           # SPIFFS storage management
│   ├── constants.h         # Configuration
│   └── certs/              # SSL certificates
├── data/
│   ├── index.html          # Web interface
│   ├── app.js              # Frontend logic
│   ├── crypto.js           # Client-side encryption
│   └── style.css           # Styling
├── partitions.csv          # Flash partition table
└── generate_cert.sh        # Certificate generation script
```

## Troubleshooting

**WiFi doesn't start after BLE connection:**
- Check serial monitor for errors
- Ensure BLE connection is successful
- Try power cycling the device

**Can't access web interface:**
- Verify you're connected to "DeadDrop" WiFi
- Try `http://192.168.4.1` if HTTPS fails initially
- Accept the certificate warning in your browser

**Encryption errors in browser:**
- Web Crypto API requires HTTPS (secure context)
- Ensure you accepted the certificate warning
- Try a different browser (Chrome, Firefox, Safari)

**Build errors:**
- Ensure ESP-IDF v5.5.1+ is properly installed
- Run `idf.py fullclean` and rebuild
- Check that certificates exist in `main/certs/`

## License

© by Q

---