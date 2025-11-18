// Global state
let currentNoteId = null;
let currentNoteEncrypted = false;

// API base URL
const API_BASE = '/api';

// Initialize app
document.addEventListener('DOMContentLoaded', () => {
    syncTime();
    loadNotes();
    loadStats();
    setupEventListeners();
});

// Sync time with server
async function syncTime() {
    try {
        const timestamp = Math.floor(Date.now() / 1000);
        await fetch(`${API_BASE}/time`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ timestamp })
        });
        console.log('Time synchronized');
    } catch (error) {
        console.error('Failed to sync time:', error);
    }
}

function setupEventListeners() {
    document.getElementById('createBtn').addEventListener('click', openCreateModal);
    document.getElementById('createForm').addEventListener('submit', handleCreateNote);
    document.getElementById('unlockForm').addEventListener('submit', handleUnlockNote);
}

// Load notes list
async function loadNotes() {
    try {
        const response = await fetch(`${API_BASE}/notes`);
        const data = await response.json();
        
        const notesList = document.getElementById('notesList');
        
        if (data.notes && data.notes.length > 0) {
            notesList.innerHTML = data.notes.map(note => `
                <div class="note-card" onclick="openNote('${note.id}', '${escapeHtml(note.title)}', ${note.encrypted})">
                    <h3>${escapeHtml(note.title)}</h3>
                    <p class="timestamp">${formatTimestamp(note.timestamp)}</p>
                    ${note.encrypted ? '<span class="encrypted-badge">🔒 Encrypted</span>' : '<span class="plain-badge">📝 Plain</span>'}
                </div>
            `).join('');
        } else {
            notesList.innerHTML = '<p class="loading">No messages yet. Create one to get started!</p>';
        }
    } catch (error) {
        console.error('Failed to load notes:', error);
        document.getElementById('notesList').innerHTML = '<p class="error">Failed to load messages</p>';
    }
}

// Load storage stats
async function loadStats() {
    try {
        const response = await fetch(`${API_BASE}/stats`);
        const data = await response.json();
        
        document.getElementById('noteCount').textContent = `${data.count || 0} messages`;
        
        const usedKB = Math.round((data.used || 0) / 1024);
        const totalKB = Math.round((data.total || 0) / 1024);
        document.getElementById('storageInfo').textContent = `${usedKB} KB / ${totalKB} KB used`;
    } catch (error) {
        console.error('Failed to load stats:', error);
    }
}

// Create note modal
function openCreateModal() {
    document.getElementById('createModal').classList.add('active');
    document.getElementById('createForm').reset();
}

function closeCreateModal() {
    document.getElementById('createModal').classList.remove('active');
}

async function handleCreateNote(e) {
    e.preventDefault();
    
    const title = document.getElementById('titleInput').value;
    const message = document.getElementById('messageInput').value;
    const password = document.getElementById('passwordInput').value;
    
    try {
        let finalMessage = message;
        let encrypted = false;
        
        // Encrypt if password provided
        if (password && password.trim().length > 0) {
            finalMessage = await encryptMessage(message, password);
            encrypted = true;
        }
        
        const response = await fetch(`${API_BASE}/notes`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ title, message: finalMessage, encrypted })
        });
        
        const data = await response.json();
        
        if (response.ok) {
            closeCreateModal();
            loadNotes();
            loadStats();
        } else {
            alert(data.error || 'Failed to create message');
        }
    } catch (error) {
        console.error('Failed to create note:', error);
        alert('Failed to create message: ' + error.message);
    }
}

// Open note (fetch and check if encrypted)
async function openNote(noteId, title, encrypted) {
    currentNoteId = noteId;
    currentNoteEncrypted = encrypted;
    
    if (encrypted) {
        // Show password prompt for encrypted notes
        openUnlockModal(noteId, title);
    } else {
        // Fetch and display plain text note
        try {
            const response = await fetch(`${API_BASE}/notes/${noteId}`);
            const data = await response.json();
            
            if (response.ok) {
                showViewModal(data.title, data.message, data.timestamp, noteId);
            } else {
                alert(data.error || 'Failed to load message');
            }
        } catch (error) {
            console.error('Failed to load note:', error);
            alert('Failed to load message');
        }
    }
}

// Unlock note modal
function openUnlockModal(noteId, title) {
    currentNoteId = noteId;
    document.getElementById('unlockTitle').textContent = title;
    document.getElementById('unlockModal').classList.add('active');
    document.getElementById('unlockForm').reset();
    document.getElementById('unlockError').textContent = '';
}

function closeUnlockModal() {
    document.getElementById('unlockModal').classList.remove('active');
    // Don't clear currentNoteId here - we need it for viewing/deleting
}

async function handleUnlockNote(e) {
    e.preventDefault();
    
    const password = document.getElementById('unlockPasswordInput').value;
    const errorEl = document.getElementById('unlockError');
    
    try {
        // Fetch encrypted note
        const response = await fetch(`${API_BASE}/notes/${currentNoteId}`);
        const data = await response.json();
        
        if (!response.ok) {
            errorEl.textContent = data.error || 'Failed to load message';
            return;
        }
        
        // Decrypt client-side
        try {
            const decrypted = await decryptMessage(data.message, password);
            closeUnlockModal();
            showViewModal(data.title, decrypted, data.timestamp, data.id || currentNoteId);
        } catch (decryptError) {
            errorEl.textContent = 'Wrong password!';
        }
    } catch (error) {
        console.error('Failed to unlock note:', error);
        errorEl.textContent = 'Failed to unlock message';
    }
}

// View note modal
function showViewModal(title, message, timestamp, noteId) {
    currentNoteId = noteId; // Set the note ID for delete
    document.getElementById('viewTitle').textContent = title;
    document.getElementById('viewTimestamp').textContent = formatTimestamp(timestamp);
    document.getElementById('viewMessage').textContent = message;
    document.getElementById('viewModal').classList.add('active');
}

function closeViewModal() {
    document.getElementById('viewModal').classList.remove('active');
    currentNoteId = null;
}

async function deleteNote() {
    if (!confirm('Are you sure you want to delete this message?')) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/notes/${currentNoteId}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            closeViewModal();
            loadNotes();
            loadStats();
        } else {
            alert('Failed to delete message');
        }
    } catch (error) {
        console.error('Failed to delete note:', error);
        alert('Failed to delete message');
    }
}

// Utility functions
function formatTimestamp(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleString();
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}
