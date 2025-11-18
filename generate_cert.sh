#!/bin/bash

# Generate self-signed certificate for DeadDrop HTTPS server
# Certificate valid for 10 years

CERT_DIR="main/certs"
mkdir -p "$CERT_DIR"

echo "Generating self-signed certificate..."

openssl req -newkey rsa:2048 -nodes \
    -keyout "$CERT_DIR/prvtkey.pem" \
    -x509 -days 3650 \
    -out "$CERT_DIR/cacert.pem" \
    -subj "/CN=DeadDrop/O=DeadDrop/C=US"

if [ $? -eq 0 ]; then
    echo "✓ Certificate generated successfully"
    echo "  Private key: $CERT_DIR/prvtkey.pem"
    echo "  Certificate: $CERT_DIR/cacert.pem"
    echo ""
    echo "Certificate details:"
    openssl x509 -in "$CERT_DIR/cacert.pem" -noout -subject -dates
else
    echo "✗ Failed to generate certificate"
    exit 1
fi
