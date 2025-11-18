// Client-side encryption using Web Crypto API (AES-256-GCM)

// Generate random salt
function generateSalt() {
    return crypto.getRandomValues(new Uint8Array(16));
}

// Generate random IV
function generateIV() {
    return crypto.getRandomValues(new Uint8Array(12)); // GCM uses 12 bytes
}

// Derive key from password using PBKDF2
async function deriveKey(password, salt) {
    const encoder = new TextEncoder();
    const passwordKey = await crypto.subtle.importKey(
        'raw',
        encoder.encode(password),
        'PBKDF2',
        false,
        ['deriveKey']
    );
    
    return await crypto.subtle.deriveKey(
        {
            name: 'PBKDF2',
            salt: salt,
            iterations: 10000,
            hash: 'SHA-256'
        },
        passwordKey,
        { name: 'AES-GCM', length: 256 },
        false,
        ['encrypt', 'decrypt']
    );
}

// Encrypt message with password
async function encryptMessage(message, password) {
    const encoder = new TextEncoder();
    const salt = generateSalt();
    const iv = generateIV();
    
    const key = await deriveKey(password, salt);
    
    const ciphertext = await crypto.subtle.encrypt(
        { name: 'AES-GCM', iv: iv },
        key,
        encoder.encode(message)
    );
    
    // Combine salt + iv + ciphertext and convert to base64
    const combined = new Uint8Array(salt.length + iv.length + ciphertext.byteLength);
    combined.set(salt, 0);
    combined.set(iv, salt.length);
    combined.set(new Uint8Array(ciphertext), salt.length + iv.length);
    
    return btoa(String.fromCharCode(...combined));
}

// Decrypt message with password
async function decryptMessage(encryptedData, password) {
    try {
        // Decode from base64
        const combined = Uint8Array.from(atob(encryptedData), c => c.charCodeAt(0));
        
        // Extract salt, IV, and ciphertext
        const salt = combined.slice(0, 16);
        const iv = combined.slice(16, 28);
        const ciphertext = combined.slice(28);
        
        const key = await deriveKey(password, salt);
        
        const decrypted = await crypto.subtle.decrypt(
            { name: 'AES-GCM', iv: iv },
            key,
            ciphertext
        );
        
        const decoder = new TextDecoder();
        return decoder.decode(decrypted);
    } catch (error) {
        throw new Error('Decryption failed - wrong password or corrupted data');
    }
}
