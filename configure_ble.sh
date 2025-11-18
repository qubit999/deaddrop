#!/bin/bash

# Enable NimBLE and configure Bluetooth settings for DeadDrop

echo "Configuring NimBLE for DeadDrop..."

cd "$(dirname "$0")"

# Enable Bluetooth and NimBLE
idf.py menuconfig <<EOF
# Navigate to Component config -> Bluetooth
# Enable Bluetooth
# Select NimBLE as host
# Navigate back and save
EOF

echo "NimBLE configuration complete"
echo "Run 'idf.py build' to rebuild with BLE support"
