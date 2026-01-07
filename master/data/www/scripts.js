// Global variables
let currentBank = 0;
let registerStart = 0;
let systemStatus = {};
let slaveConnected = false;

// Register database (partial from datasheet)
const REGISTER_DB = {
    "0": { // DSP Bank
        "0x12": { name: "COM7", desc: "Common Control 7 - Reset/Resolution" },
        "0xC0": { name: "CTRL0", desc: "Module Enable Control" },
        "0xC1": { name: "CTRL1", desc: "Module Enable Control" },
        "0xDA": { name: "IMAGE_MODE", desc: "Output Format Select" },
        "0xD3": { name: "COM10", desc: "Clock/Polarity Control" },
        "0x17": { name: "HSTART", desc: "Horizontal Start High" },
        "0x18": { name: "HSTART", desc: "Horizontal Start Low" },
        "0x19": { name: "VSTART", desc: "Vertical Start High" },
        "0x1A": { name: "VSTART", desc: "Vertical Start Low" },
        "0x1B": { name: "HSIZE", desc: "Horizontal Size High" },
        "0x1C": { name: "HSIZE", desc: "Horizontal Size Low" },
        "0x1D": { name: "VSIZE", desc: "Vertical Size High" },
        "0x1E": { name: "VSIZE", desc: "Vertical Size Low" }
    },
    "1": { // Sensor Bank
        "0x00": { name: "GAIN", desc: "AGC Gain Control LSBs" },
        "0x03": { name: "COM1", desc: "Common Control 1 - Window Control" },
        "0x10": { name: "AEC", desc: "Automatic Exposure Control LSB" },
        "0x11": { name: "AEC", desc: "Automatic Exposure Control MSB" },
        "0x12": { name: "COM7", desc: "Resolution Selection" },
        "0x13": { name: "COM8", desc: "AGC/AEC Control" },
        "0x14": { name: "COM9", desc: "AGC/AEC Ceiling" },
        "0x15": { name: "COM10", desc: "Clock/Polarity Control" },
        "0x16": { name: "HREF", desc: "HREF Control" },
        "0x17": { name: "COM11", desc: "Night Mode/Edge Enhancement" },
        "0x1E": { name: "MVFP", desc: "Mirror/VFlip" },
        "0x24": { name: "AEW", desc: "AGC/AEC Stable Operation" },
        "0x25": { name: "AEB", desc: "AGC/AEC Bright Region" },
        "0x26": { name: "VPT", desc: "AGC/AEC Low Region" }
    }
};

// Initialize application
document.addEventListener('DOMContentLoaded', function() {
    // Initialize components
    initEventListeners();
    updateSystemStatus();
    loadRegisterTable();
    
    // Start periodic updates
    setInterval(updateSystemStatus, 5000);
    setInterval(updateUptime, 1000);
    
    // Check slave connection
    checkSlaveConnection();
});

// Initialize event listeners
function initEventListeners() {
    // Capture buttons
    document.getElementById('capture-single').addEventListener('click', () => captureImage('single'));
    document.getElementById('capture-burst').addEventListener('click', () => captureImage('burst'));
    document.getElementById('capture-video').addEventListener('click', () => captureImage('video'));
    
    // Bank selection
    document.querySelectorAll('.bank-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.bank-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            currentBank = parseInt(this.dataset.bank);
            document.querySelector('.current-bank').textContent = 
                `Current: ${currentBank === 0 ? 'DSP (0x00)' : 'Sensor (0x01)'}`;
            loadRegisterTable();
        });
    });
    
    // Register control
    document.getElementById('reg-read').addEventListener('click', readRegister);
    document.getElementById('reg-write').addEventListener('click', writeRegister);
    document.getElementById('load-more-registers').addEventListener('click', loadMoreRegisters);
    
    // Settings
    document.getElementById('apply-settings').addEventListener('click', applySettings);
    document.getElementById('frame-rate').addEventListener('input', function() {
        document.getElementById('fps-value').textContent = this.value;
    });
    
    // Presets
    document.getElementById('load-preset').addEventListener('click', loadPreset);
    document.getElementById('save-preset').addEventListener('click', savePreset);
    document.getElementById('delete-preset').addEventListener('click', deletePreset);
    
    // Advanced controls
    document.getElementById('dump-registers').addEventListener('click', dumpRegisters);
    document.getElementById('reset-camera').addEventListener('click', resetCamera);
    document.getElementById('factory-reset').addEventListener('click', factoryReset);
    
    // System controls
    document.getElementById('refresh-status').addEventListener('click', updateSystemStatus);
    document.getElementById('refresh-files').addEventListener('click', loadRecentFiles);
    document.getElementById('system-reboot').addEventListener('click', rebootSystem);
}

// Update system status
async function updateSystemStatus() {
    try {
        const response = await fetch('/api/system/status');
        const data = await response.json();
        systemStatus = data;
        
        // Update status indicators
        updateStatusIndicators(data);
        
        // Update information displays
        document.getElementById('master-ip').textContent = wifiGetIp() || '192.168.4.1';
        document.getElementById('slave-ip').textContent = data.slave.ip || 'Not connected';
        document.getElementById('sd-status').textContent = data.storage.mounted ? 'Mounted' : 'Not mounted';
        document.getElementById('free-space').textContent = 
            `${Math.round(data.storage.free_kb / 1024)} MB`;
        
        // Update slave connection status
        slaveConnected = data.slave.connected;
        updateSlaveStatus(slaveConnected);
        
    } catch (error) {
        console.error('Failed to update system status:', error);
        document.getElementById('system-status').className = 'status status-error';
        document.getElementById('system-status').innerHTML = '<i class="fas fa-circle"></i> Connection Error';
    }
}

// Update status indicators
function updateStatusIndicators(data) {
    const systemStatusEl = document.getElementById('system-status');
    const cameraStatusEl = document.getElementById('camera-status');
    const slaveStatusEl = document.getElementById('slave-status');
    
    // System status
    if (data.storage.mounted && data.camera.initialized) {
        systemStatusEl.className = 'status status-ok';
        systemStatusEl.innerHTML = '<i class="fas fa-circle"></i> System Ready';
    } else {
        systemStatusEl.className = 'status status-warning';
        systemStatusEl.innerHTML = '<i class="fas fa-circle"></i> System Warning';
    }
    
    // Camera status
    if (data.camera.initialized) {
        cameraStatusEl.className = 'status status-ok';
        cameraStatusEl.innerHTML = `<i class="fas fa-circle"></i> Camera Ready (${getResolutionName(data.camera.resolution)})`;
    } else {
        cameraStatusEl.className = 'status status-error';
        cameraStatusEl.innerHTML = '<i class="fas fa-circle"></i> Camera Error';
    }
    
    // Slave status
    if (data.slave.connected) {
        slaveStatusEl.className = 'status status-ok';
        slaveStatusEl.innerHTML = `<i class="fas fa-circle"></i> Slave Connected`;
    } else {
        slaveStatusEl.className = 'status status-error';
        slaveStatusEl.innerHTML = '<i class="fas fa-circle"></i> Slave Disconnected';
    }
}

// Get resolution name
function getResolutionName(resCode) {
    const resolutions = {
        0: 'UXGA',
        1: 'SVGA',
        2: 'CIF'
    };
    return resolutions[resCode] || 'Unknown';
}

// Update uptime counter
function updateUptime() {
    const uptimeEl = document.getElementById('uptime');
    const current = uptimeEl.textContent;
    
    if (current === '00:00:00') {
        // Start from zero
        uptimeEl.textContent = '00:00:01';
    } else {
        const parts = current.split(':').map(Number);
        let hours = parts[0];
        let minutes = parts[1];
        let seconds = parts[2];
        
        seconds++;
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
            if (minutes >= 60) {
                minutes = 0;
                hours++;
            }
        }
        
        uptimeEl.textContent = 
            `${hours.toString().padStart(2, '0')}:` +
            `${minutes.toString().padStart(2, '0')}:` +
            `${seconds.toString().padStart(2, '0')}`;
    }
}

// Load register table
async function loadRegisterTable(start = 0) {
    try {
        const response = await fetch(`/api/registers/range?bank=${currentBank}&start=${start}&end=${start + 31}`);
        const data = await response.json();
        
        const tableBody = document.getElementById('reg-table-body');
        tableBody.innerHTML = '';
        
        for (let addr = start; addr <= start + 31; addr++) {
            const hexAddr = '0x' + addr.toString(16).padStart(2, '0').toUpperCase();
            const value = data[hexAddr] || 0;
            const regInfo = REGISTER_DB[currentBank][hexAddr] || { name: '-', desc: 'Unknown register' };
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td><code>${hexAddr}</code></td>
                <td>${regInfo.name}</td>
                <td><code>0x${value.toString(16).padStart(2, '0').toUpperCase()}</code></td>
                <td><code>${value.toString(2).padStart(8, '0')}</code></td>
                <td>
                    <button class="btn btn-secondary btn-sm" onclick="editRegister('${hexAddr}')">
                        <i class="fas fa-edit"></i>
                    </button>
                </td>
            `;
            tableBody.appendChild(row);
        }
        
        registerStart = start;
        
    } catch (error) {
        console.error('Failed to load register table:', error);
        showError('Failed to load registers');
    }
}

// Load more registers
function loadMoreRegisters() {
    loadRegisterTable(registerStart + 32);
}

// Read single register
async function readRegister() {
    const addrInput = document.getElementById('reg-addr');
    const addr = addrInput.value.trim();
    
    if (!addr.match(/^0x[0-9A-Fa-f]{2}$/)) {
        showError('Invalid address format. Use 0xXX');
        return;
    }
    
    try {
        const response = await fetch(`/api/registers/single?bank=${currentBank}&addr=${addr}`);
        const data = await response.json();
        
        document.getElementById('reg-value').value = '0x' + data.value.toString(16).padStart(2, '0');
        updateRegisterDetails(addr, data.value);
        
    } catch (error) {
        console.error('Failed to read register:', error);
        showError('Failed to read register');
    }
}

// Write single register
async function writeRegister() {
    const addrInput = document.getElementById('reg-addr');
    const valueInput = document.getElementById('reg-value');
    
    const addr = addrInput.value.trim();
    const value = valueInput.value.trim();
    
    if (!addr.match(/^0x[0-9A-Fa-f]{2}$/)) {
        showError('Invalid address format. Use 0xXX');
        return;
    }
    
    if (!value.match(/^0x[0-9A-Fa-f]{2}$/)) {
        showError('Invalid value format. Use 0xXX');
        return;
    }
    
    const valueNum = parseInt(value, 16);
    
    try {
        const response = await fetch('/api/registers/single', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                bank: currentBank,
                addr: parseInt(addr, 16),
                value: valueNum
            })
        });
        
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess('Register written successfully');
            updateRegisterDetails(addr, valueNum);
            
            // Reload table if this register is in view
            const addrNum = parseInt(addr, 16);
            if (addrNum >= registerStart && addrNum <= registerStart + 31) {
                loadRegisterTable(registerStart);
            }
            
            // Auto-sync to slave if enabled
            if (document.getElementById('auto-sync').checked && slaveConnected) {
                syncRegisterToSlave(addr, valueNum);
            }
            
        } else {
            showError('Failed to write register: ' + (data.message || 'Unknown error'));
        }
        
    } catch (error) {
        console.error('Failed to write register:', error);
        showError('Failed to write register');
    }
}

// Update register details display
function updateRegisterDetails(addr, value) {
    const hexValue = '0x' + value.toString(16).padStart(2, '0').toUpperCase();
    const binaryValue = value.toString(2).padStart(8, '0');
    const regInfo = REGISTER_DB[currentBank][addr] || { name: '-', desc: 'Unknown register' };
    
    document.getElementById('reg-name').textContent = regInfo.name;
    document.getElementById('reg-desc').textContent = regInfo.desc;
    document.getElementById('reg-current').textContent = hexValue;
    document.getElementById('reg-binary').textContent = binaryValue;
}

// Edit register (load into editor)
function editRegister(addr) {
    document.getElementById('reg-addr').value = addr;
    document.getElementById('reg-value').value = '';
    
    // Read the current value
    fetch(`/api/registers/single?bank=${currentBank}&addr=${addr}`)
        .then(response => response.json())
        .then(data => {
            document.getElementById('reg-value').value = '0x' + data.value.toString(16).padStart(2, '0');
            updateRegisterDetails(addr, data.value);
        })
        .catch(error => {
            console.error('Failed to read register for editing:', error);
        });
}

// Capture image
async function captureImage(type) {
    const mode = document.getElementById('capture-mode').value;
    const payload = { type };
    
    if (type === 'burst') {
        payload.count = 10;
    } else if (type === 'video') {
        payload.duration = 10000; // 10 seconds
    }
    
    try {
        const response = await fetch('/api/capture', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess(data.message);
            
            // Update last capture time
            const now = new Date();
            document.getElementById('last-capture').textContent = 
                now.toLocaleTimeString();
                
            // Load recent files after a delay
            setTimeout(loadRecentFiles, 2000);
            
        } else {
            showError('Capture failed: ' + (data.message || 'Unknown error'));
        }
        
    } catch (error) {
        console.error('Capture failed:', error);
        showError('Capture failed: ' + error.message);
    }
}

// Apply settings to both cameras
async function applySettings() {
    const resolution = document.getElementById('resolution').value;
    const fps = parseInt(document.getElementById('frame-rate').value);
    const exposure = parseInt(document.getElementById('exposure').value);
    const gain = parseInt(document.getElementById('gain').value);
    
    try {
        // Apply to master
        const response = await fetch('/api/camera/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                resolution: resolution === 'uxga' ? 0 : resolution === 'svga' ? 1 : 2,
                frame_rate: fps,
                exposure: exposure,
                gain: gain,
                apply_to_slave: true
            })
        });
        
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess('Settings applied successfully');
            updateSystemStatus();
        } else {
            showError('Failed to apply settings: ' + (data.message || 'Unknown error'));
        }
        
    } catch (error) {
        console.error('Failed to apply settings:', error);
        showError('Failed to apply settings');
    }
}

// Load preset
async function loadPreset() {
    const presetName = document.getElementById('preset-select').value;
    
    try {
        const response = await fetch(`/api/registers/preset/load?name=${presetName}`);
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess(`Preset "${presetName}" loaded successfully`);
            loadRegisterTable(0);
        } else {
            showError('Failed to load preset: ' + (data.message || 'Unknown error'));
        }
        
    } catch (error) {
        console.error('Failed to load preset:', error);
        showError('Failed to load preset');
    }
}

// Save preset
async function savePreset() {
    const presetName = document.getElementById('preset-name').value.trim();
    
    if (!presetName) {
        showError('Please enter a preset name');
        return;
    }
    
    try {
        const response = await fetch('/api/registers/preset/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name: presetName })
        });
        
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess(`Preset "${presetName}" saved successfully`);
            
            // Add to dropdown
            const select = document.getElementById('preset-select');
            const option = document.createElement('option');
            option.value = presetName;
            option.textContent = presetName;
            select.appendChild(option);
            
            // Clear input
            document.getElementById('preset-name').value = '';
            
        } else {
            showError('Failed to save preset: ' + (data.message || 'Unknown error'));
        }
        
    } catch (error) {
        console.error('Failed to save preset:', error);
        showError('Failed to save preset');
    }
}

// Dump all registers
async function dumpRegisters() {
    try {
        const response = await fetch('/api/registers/dump');
        const data = await response.json();
        
        // Create download link
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `ov2640_registers_${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        showSuccess('Register dump downloaded');
        
    } catch (error) {
        console.error('Failed to dump registers:', error);
        showError('Failed to dump registers');
    }
}

// Reset camera
async function resetCamera() {
    if (!confirm('Are you sure you want to reset the camera? This will reload default settings.')) {
        return;
    }
    
    try {
        const response = await fetch('/api/registers/reset', { method: 'POST' });
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess('Camera reset successfully');
            loadRegisterTable(0);
        } else {
            showError('Failed to reset camera');
        }
        
    } catch (error) {
        console.error('Failed to reset camera:', error);
        showError('Failed to reset camera');
    }
}

// Factory reset
async function factoryReset() {
    if (!confirm('WARNING: This will restore factory settings and clear all custom configurations. Continue?')) {
        return;
    }
    
    try {
        const response = await fetch('/api/system/factory-reset', { method: 'POST' });
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess('Factory reset completed. Rebooting...');
            setTimeout(() => location.reload(), 3000);
        } else {
            showError('Factory reset failed');
        }
        
    } catch (error) {
        console.error('Factory reset failed:', error);
        showError('Factory reset failed');
    }
}

// Load recent files
async function loadRecentFiles() {
    try {
        const response = await fetch('/api/files/recent');
        const files = await response.json();
        
        const fileList = document.getElementById('recent-files');
        fileList.innerHTML = '';
        
        if (files.length === 0) {
            fileList.innerHTML = '<div class="file-item">No captures yet</div>';
            return;
        }
        
        files.slice(0, 5).forEach(file => {
            const div = document.createElement('div');
            div.className = 'file-item';
            
            const size = file.size > 1024 * 1024 ? 
                (file.size / (1024 * 1024)).toFixed(1) + ' MB' : 
                (file.size / 1024).toFixed(0) + ' KB';
            
            div.innerHTML = `
                <span>${file.name}</span>
                <span>${size}</span>
            `;
            
            div.addEventListener('click', () => downloadFile(file.name));
            fileList.appendChild(div);
        });
        
    } catch (error) {
        console.error('Failed to load recent files:', error);
    }
}

// Download file
function downloadFile(filename) {
    const a = document.createElement('a');
    a.href = `/files/${filename}`;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
}

// Check slave connection
async function checkSlaveConnection() {
    try {
        const response = await fetch('/api/slave/status');
        const data = await response.json();
        slaveConnected = data.connected;
        updateSlaveStatus(slaveConnected);
    } catch (error) {
        slaveConnected = false;
        updateSlaveStatus(false);
    }
}

// Update slave status display
function updateSlaveStatus(connected) {
    const statusEl = document.getElementById('slave-status');
    const ipEl = document.getElementById('slave-ip');
    
    if (connected) {
        statusEl.className = 'status status-ok';
        statusEl.innerHTML = '<i class="fas fa-circle"></i> Slave Connected';
        ipEl.textContent = systemStatus.slave?.ip || 'Connected';
    } else {
        statusEl.className = 'status status-error';
        statusEl.innerHTML = '<i class="fas fa-circle"></i> Slave Disconnected';
        ipEl.textContent = 'Not connected';
    }
}

// Sync register to slave
async function syncRegisterToSlave(addr, value) {
    try {
        const response = await fetch('/api/registers/sync', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                bank: currentBank,
                addr: parseInt(addr, 16),
                value: value
            })
        });
        
        const data = await response.json();
        
        if (data.status !== 'ok') {
            console.warn('Failed to sync register to slave:', data.message);
        }
        
    } catch (error) {
        console.error('Failed to sync register to slave:', error);
    }
}

// Reboot system
async function rebootSystem() {
    if (!confirm('Are you sure you want to reboot the system?')) {
        return;
    }
    
    try {
        const response = await fetch('/api/system/reboot', { method: 'POST' });
        const data = await response.json();
        
        if (data.status === 'ok') {
            showSuccess('System rebooting...');
            setTimeout(() => location.reload(), 5000);
        }
        
    } catch (error) {
        console.error('Failed to reboot system:', error);
        showError('Failed to reboot system');
    }
}

// Helper function to get WiFi IP (mock)
function wifiGetIp() {
    return '192.168.4.1';
}

// Show success message
function showSuccess(message) {
    showNotification(message, 'success');
}

// Show error message
function showError(message) {
    showNotification(message, 'error');
}

// Show notification
function showNotification(message, type = 'info') {
    // Remove existing notification
    const existing = document.querySelector('.notification');
    if (existing) {
        existing.remove();
    }
    
    // Create notification
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.innerHTML = `
        <span>${message}</span>
        <button onclick="this.parentElement.remove()">&times;</button>
    `;
    
    // Add styles
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 15px 20px;
        background: ${type === 'success' ? '#27ae60' : type === 'error' ? '#e74c3c' : '#3498db'};
        color: white;
        border-radius: 8px;
        box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        display: flex;
        align-items: center;
        justify-content: space-between;
        min-width: 300px;
        max-width: 400px;
        z-index: 1000;
        animation: slideIn 0.3s ease-out;
    `;
    
    // Add keyframes
    if (!document.querySelector('#notification-styles')) {
        const style = document.createElement('style');
        style.id = 'notification-styles';
        style.textContent = `
            @keyframes slideIn {
                from { transform: translateX(100%); opacity: 0; }
                to { transform: translateX(0); opacity: 1; }
            }
            .notification button {
                background: none;
                border: none;
                color: white;
                font-size: 20px;
                cursor: pointer;
                margin-left: 15px;
                padding: 0;
                width: 20px;
                height: 20px;
                display: flex;
                align-items: center;
                justify-content: center;
            }
        `;
        document.head.appendChild(style);
    }
    
    document.body.appendChild(notification);
    
    // Auto-remove after 5 seconds
    setTimeout(() => {
        if (notification.parentElement) {
            notification.remove();
        }
    }, 5000);
}