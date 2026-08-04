let isLogging = false;
let isConfigLoaded = false;
let isSdReady = false;
let isRtcReady = false;

// --- KONFIGURASI GRAFIK CANVAS ---
const MAX_DATA_POINTS = 50; // Jumlah titik yang digambar di layar

// Data Storage buat Grafik
const chartData = {
    accel: { x: [], y: [], z: [] },
    orient: { pitch: [], roll: [], yaw: [] },
    fuel: [],
    voltage: [],
    accVoltage: [],
    temperature: [],
    ignition: []
};

// Fungsi Universal buat nggambar grafik murni pake HTML5 Canvas
function drawLineChart(canvasId, dataArrays, colors, yMin, yMax) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    canvas.width = canvas.parentElement.clientWidth;
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;

    // Bersihin layar sebelum nggambar ulang
    ctx.clearRect(0, 0, width, height);

    // Bikin garis horizontal (Grid Tengah/Bawah)
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, height / 2);
    ctx.lineTo(width, height / 2);
    ctx.stroke();

    // Loop buat tiap array data (misal: X, Y, Z atau cuma 1 data)
    dataArrays.forEach((dataArr, index) => {
        if (dataArr.length < 2) return;

        ctx.strokeStyle = colors[index];
        ctx.lineWidth = 2;
        ctx.beginPath();

        const stepX = width / (MAX_DATA_POINTS - 1);
        
        for (let i = 0; i < dataArr.length; i++) {
            const x = i * stepX;
            // Normalisasi nilai ke skala Y (dibalik karena Y ke bawah itu positif di canvas)
            let normalizedY = (dataArr[i] - yMin) / (yMax - yMin);
            // Kalau nilai keluar batas, potong
            if (normalizedY > 1) normalizedY = 1;
            if (normalizedY < 0) normalizedY = 0;
            
            const y = height - (normalizedY * height);

            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
    });
}

function updateCharts() {
    // 1. Gambar Chart Akselerasi
    drawLineChart('chart-imu-accel', 
        [chartData.accel.x, chartData.accel.y, chartData.accel.z], 
        ['#EF3434', '#12B76A', '#2E90FA'], -30, 30); // Merah, Hijau, Biru Modern
    
    // 2. Gambar Chart Orientasi
    drawLineChart('chart-imu-orient', 
        [chartData.orient.pitch, chartData.orient.roll, chartData.orient.yaw], 
        ['#EF3434', '#12B76A', '#2E90FA'], -180, 180);

    // 3. Gambar Chart Fuel Stick Voltage
    drawLineChart('chart-fuel', [chartData.fuel], ['#F79009'], 0, 5); // Warning Orange

    // 4. Gambar Chart Voltase Internal + Accumulator
    drawLineChart('chart-voltage', [chartData.voltage, chartData.accVoltage], ['#98A2B3', '#F79009'], 0, 20); // Gray & Orange

    // 5. Gambar Chart Temperatur
    drawLineChart('chart-temp', [chartData.temperature], ['#EF3434'], 0, 100);

    // 6. Gambar Chart Ignition (ON/OFF)
    drawLineChart('chart-ign', [chartData.ignition], ['#E30613'], -0.2, 1.2);
}

// Fungsi buat nge-push data ke array dan ngebuang data lama
function pushData(array, value) {
    array.push(value);
    if (array.length > MAX_DATA_POINTS) {
        array.shift();
    }
}
// ------------------------------------

function switchPage(pageId, btnElement) {
    document.querySelectorAll('.view-section').forEach(section => {
        section.classList.remove('active');
    });
    
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    document.getElementById('view-' + pageId).classList.add('active');
    btnElement.classList.add('active');

    const titles = {
        'data': 'Data Monitor',
        'config': 'Configuration'
    };
    document.getElementById('page-title').innerText = titles[pageId];
}

function toggleTheme() {
    document.body.classList.toggle('dark-theme');
    const btn = document.getElementById('btn-theme');
    if (btn) {
        btn.innerText = document.body.classList.contains('dark-theme') ? "☀️" : "🌙";
    }
}

// Fungsi Sync Waktu Lokal PC/HP ke Form
function syncLocalTime() {
    const localDateTime = formatDateTimeLocalFromDevice(new Date());
    document.getElementById('rtc-datetime').value = localDateTime;
}

function formatDateTimeLocalFromDevice(date) {
    const localDate = new Date(date.getTime());
    const year = localDate.getFullYear();
    const month = String(localDate.getMonth() + 1).padStart(2, '0');
    const day = String(localDate.getDate()).padStart(2, '0');
    const hours = String(localDate.getHours()).padStart(2, '0');
    const minutes = String(localDate.getMinutes()).padStart(2, '0');
    const seconds = String(localDate.getSeconds()).padStart(2, '0');
    return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}`;
}

async function fetchData() {
    try {
        const response = await fetch('/api/data');
        const data = await response.json();

        // Update Teks IMU
        document.getElementById('val-pitch').innerText = data.pitch.toFixed(2);
        document.getElementById('val-roll').innerText = data.roll.toFixed(2);
        document.getElementById('val-yaw').innerText = data.yaw.toFixed(2);
        document.getElementById('val-acc-x').innerText = data.accX.toFixed(2);
        document.getElementById('val-acc-y').innerText = data.accY.toFixed(2);
        document.getElementById('val-acc-z').innerText = data.accZ.toFixed(2);
        
        // Update Teks Fuel Stick & Voltase
        document.getElementById('val-fuel-raw').innerText = data.fuel_voltage.toFixed(2);
        document.getElementById('val-volt').innerText = data.voltage.toFixed(2);
        document.getElementById('val-acc-volt').innerText = data.acc_voltage.toFixed(2);
        document.getElementById('val-ign-chart').innerText = data.ignition ? "ON (1)" : "OFF (0)";

        // Update Teks Temperatur
        document.getElementById('val-temp').innerText = data.temperature.toFixed(2);
        
        // --- Update Teks Status (AC) ---
        // RTC
        isRtcReady = (data.rtc_ready !== undefined) ? data.rtc_ready : true;
        const rtcBadge = document.getElementById('rtc-time');
        if (isRtcReady) {
            rtcBadge.innerText = "⌚ RTC: " + (data.rtc_time || "---- --:--:--");
            rtcBadge.className = "badge neutral";
        } else {
            rtcBadge.innerText = "⌚ RTC: DISCONNECTED";
            rtcBadge.className = "badge off";
        }
        // Battery: Green >30%, Red <=30%
        const battBadge = document.getElementById('batt-status');
        if (data.batt_perc !== undefined && data.batt_perc !== null && !isNaN(data.batt_perc)) {
            let battVal = Math.round(data.batt_perc);
            battBadge.innerText = "🔋 Batt: " + battVal + "%";
            if (battVal > 30) {
                battBadge.className = "badge on";
            } else {
                battBadge.className = "badge off";
            }
        } else {
            battBadge.innerText = "🔋 Batt: --%";
            battBadge.className = "badge neutral";
        }
        
        // SD Card & Storage
        isSdReady = data.sd_ready;
        const sdBadge = document.getElementById('sd-status');
        if (isSdReady) {
            let usedPerc = data.sd_used_perc ? Math.round(data.sd_used_perc) : 0;
            sdBadge.innerText = "💾 SD: READY (" + usedPerc + "%)";
            sdBadge.className = "badge on";
        } else {
            sdBadge.innerText = "💾 SD: ERROR";
            sdBadge.className = "badge off";
        }

        // Ignition
        const ignBadge = document.getElementById('ign-status');
        if (data.ignition) {
            ignBadge.innerText = "🔑 Ignition: ON";
            ignBadge.className = "badge on";
        } else {
            ignBadge.innerText = "🔑 Ignition: OFF";
            ignBadge.className = "badge off";
        }

        // WiFi AP Status
        const wifiBadge = document.getElementById('wifi-status');
        if (data.wifi_on) {
            wifiBadge.innerText = "📶 WiFi: AP Mode";
            wifiBadge.className = "badge on";
        } else {
            wifiBadge.innerText = "📶 WiFi: OFF";
            wifiBadge.className = "badge off";
        }
        // --------------------------------

        // Push data ke array untuk grafik
        pushData(chartData.accel.x, data.accX);
        pushData(chartData.accel.y, data.accY);
        pushData(chartData.accel.z, data.accZ);
        pushData(chartData.orient.pitch, data.pitch);
        pushData(chartData.orient.roll, data.roll);
        pushData(chartData.orient.yaw, data.yaw);
        pushData(chartData.fuel, data.fuel_voltage);
        pushData(chartData.voltage, data.voltage);
        pushData(chartData.accVoltage, data.acc_voltage);
        pushData(chartData.ignition, data.ignition ? 1 : 0);
        pushData(chartData.temperature, data.temperature);

        // Render Ulang Grafik
        updateCharts();

        if (!isConfigLoaded) {
            // Konversi dari ms (di ESP) ke Hz (di Web)
            let currentHz = Math.round(1000 / data.rate);
            document.getElementById('sampling-rate').value = currentHz;
            isConfigLoaded = true;
        }

        updateLoggingUI(data.is_logging);

    } catch (error) {
        console.error("Fetch Data Error:", error);
    }
    setTimeout(fetchData, 200);
}

function updateLoggingUI(status) {
    isLogging = status;
    const btnLog = document.getElementById('btn-toggle-log');
    const badgeLog = document.getElementById('log-status');

    if (isLogging) {
        btnLog.innerText = "STOP LOGGING";
        btnLog.className = "btn-stop";
        badgeLog.innerText = "📝 Logging: ON";
        badgeLog.className = "badge on";
    } else {
        btnLog.innerText = "START LOGGING";
        btnLog.className = "btn-start";
        badgeLog.innerText = "📝 Logging: OFF";
        badgeLog.className = "badge off";
    }
}

// Sanitasi Input Sampling Rate (Hanya Menerima Integer Positif)
document.addEventListener('DOMContentLoaded', () => {
    const samplingInput = document.getElementById('sampling-rate');
    if (samplingInput) {
        samplingInput.addEventListener('keydown', (e) => {
            if (['e', 'E', '.', ',', '-', '+'].includes(e.key)) {
                e.preventDefault();
            }
        });
    }
});

async function toggleLogging() {
    // Validasi Sampling Rate (1 sampai 20 Hz)
    const rateInputVal = document.getElementById('sampling-rate').value;
    const rateHz = parseInt(rateInputVal, 10);
    if (!rateInputVal || isNaN(rateHz) || rateHz < 1 || rateHz > 20) {
        alert("Error: Sampling Rate must be a whole number between 1 and 20 Hz.");
        return;
    }

    // Validasi SD Card Status saat mau START LOGGING
    if (!isLogging && !isSdReady) {
        alert("Error: Cannot start logging. SD Card is not mounted or has failed.");
        return;
    }

    const newState = !isLogging;
    const rateMs = Math.round(1000 / rateHz);

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                is_logging: newState,
                sampling_rate_ms: rateMs
            })
        });

        const resData = await response.json();

        if (response.ok && resData.status === "OK") {
            updateLoggingUI(resData.is_logging !== undefined ? resData.is_logging : newState);
        } else {
            alert(resData.message || "Error: Failed to toggle logging state.");
            updateLoggingUI(resData.is_logging !== undefined ? resData.is_logging : false);
        }
    } catch (error) {
        alert("Failed to communicate with ESP32");
    }
}

async function updateConfig() {
    // Validasi Sampling Rate (1 sampai 20 Hz)
    const rateInputVal = document.getElementById('sampling-rate').value;
    const rateHz = parseInt(rateInputVal, 10);
    if (!rateInputVal || isNaN(rateHz) || rateHz < 1 || rateHz > 20) {
        alert("Error: Sampling Rate must be a whole number between 1 and 20 Hz.");
        return;
    }

    const rtcInput = document.getElementById('rtc-datetime').value;
    const rateMs = Math.round(1000 / rateHz);
    
    // Siapin timestamp Unix buat RTC kalau diisi
    let unixTimestamp = 0;
    if (rtcInput) {
        unixTimestamp = Math.floor(new Date(rtcInput).getTime() / 1000);
    }

    const timezoneOffsetMin = -new Date().getTimezoneOffset();

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                sampling_rate_ms: rateMs,
                rtc_timestamp: unixTimestamp,
                timezone_offset_min: timezoneOffsetMin
            })
        });

        const resData = await response.json();

        if (response.ok && resData.status === "OK") {
            alert("Config saved successfully!");
            document.getElementById('rtc-datetime').value = "";
        } else {
            alert(resData.message || "Error: Failed to save config.");
        }
    } catch (error) {
        alert("Failed to communicate with ESP32");
    }
}

function toggleSidebar() {
    const sidebar = document.querySelector('.sidebar');
    const overlay = document.querySelector('.sidebar-overlay');
    
    if (window.innerWidth <= 768) {
        sidebar.classList.toggle('active');
        overlay.classList.toggle('active');
    } else {
        sidebar.classList.toggle('collapsed');
    }
}

async function downloadAllZip() {
    const btn = document.getElementById('btn-download-zip');
    const originalText = btn ? btn.innerText : '📦 Download All (ZIP)';

    try {
        if (btn) btn.innerText = 'Fetching file list...';

        const res = await fetch('/api/files');
        if (!res.ok) throw new Error('Failed to fetch file list');
        const files = await res.json();

        if (!Array.isArray(files) || files.length === 0) {
            alert('No CSV log files found on SD card.');
            if (btn) btn.innerText = originalText;
            return;
        }

        const zip = new JSZip();
        const total = files.length;

        for (let i = 0; i < total; i++) {
            const fileObj = files[i];
            const fileName = typeof fileObj === 'string' ? fileObj : fileObj.name;
            if (!fileName) continue;

            if (btn) btn.innerText = `Downloading... ${i + 1}/${total}`;

            const fileRes = await fetch(`/download?file=${encodeURIComponent(fileName)}`);
            if (!fileRes.ok) {
                console.error(`Failed to download ${fileName}`);
                continue;
            }
            const blob = await fileRes.blob();
            zip.file(fileName, blob);
        }

        if (btn) btn.innerText = 'Zipping files...';
        const zipBlob = await zip.generateAsync({ type: 'blob' });

        const link = document.createElement('a');
        link.href = URL.createObjectURL(zipBlob);
        link.download = 'DAQ_Telemetry_Logs.zip';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        URL.revokeObjectURL(link.href);

    } catch (error) {
        console.error('Download ZIP error:', error);
        alert('Failed to download ZIP: ' + error.message);
    } finally {
        if (btn) btn.innerText = originalText;
    }
}

fetchData();