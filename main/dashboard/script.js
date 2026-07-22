let isLogging = false;
let isConfigLoaded = false;
let isSdReady = false;

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
    if (document.body.classList.contains('dark-theme')) {
        btn.innerText = "☀️ Light Mode";
    } else {
        btn.innerText = "🌙 Dark Mode";
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
    return `${year}-${month}-${day}T${hours}:${minutes}`;
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
        document.getElementById('rtc-time').innerText = "⌚ RTC: " + (data.rtc_time || "---- --:--:--");
        // Battery
        document.getElementById('batt-status').innerText = "🔋 Batt: " + (data.batt_perc ? Math.round(data.batt_perc) + "%" : "--%");
        
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

async function toggleLogging() {
    if (!isLogging && !isSdReady) {
        alert("SD Card not mounted.");
        return;
    }

    const newState = !isLogging;
    const rateHz = parseInt(document.getElementById('sampling-rate').value);
    
    // Konversi Hz ke Milidetik (ms)
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
        if (response.ok) {
            updateLoggingUI(newState);
        }
    } catch (error) {
        alert("Failed to communicate with ESP32");
    }
}

async function updateConfig() {
    const rateHz = parseInt(document.getElementById('sampling-rate').value);
    const rtcInput = document.getElementById('rtc-datetime').value;
    
    // Konversi Hz ke ms buat ESP32
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
        if (response.ok) {
            alert("Config saved successfully");
        }
    } catch (error) {
        alert("Failed to save config");
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

fetchData();