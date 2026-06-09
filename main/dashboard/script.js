let isLogging = false;
let isConfigLoaded = false;
let isSdReady = false;

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

async function fetchData() {
    try {
        const response = await fetch('/api/data');
        const data = await response.json();

        document.getElementById('val-vib-ms2').innerText = data.vib_calib_ms2.toFixed(2);
        document.getElementById('val-vib-uncalib').innerText = data.vib_uncalib_ms2.toFixed(2);
        document.getElementById('val-vib-raw').innerText = data.vib_raw_g.toFixed(3);
        
        document.getElementById('val-fuel').innerText = data.fuel_norm.toFixed(1);
        document.getElementById('val-fuel-raw').innerText = Math.round(data.fuel_raw);
        document.getElementById('val-volt').innerText = data.voltage.toFixed(2);
        
        isSdReady = data.sd_ready;
        const sdBadge = document.getElementById('sd-status');
        if (isSdReady) {
            sdBadge.innerText = "SD: READY";
            sdBadge.className = "badge on";
        } else {
            sdBadge.innerText = "SD: ERROR";
            sdBadge.className = "badge off";
        }

        if (!isConfigLoaded) {
            document.getElementById('sampling-rate').value = data.rate;
            isConfigLoaded = true;
        }

        updateLoggingUI(data.is_logging);

    } catch (error) {
        console.error(error);
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
        badgeLog.innerText = "Logging: ON";
        badgeLog.className = "badge on";
    } else {
        btnLog.innerText = "START LOGGING";
        btnLog.className = "btn-start";
        badgeLog.innerText = "Logging: OFF";
        badgeLog.className = "badge off";
    }
}

async function toggleLogging() {
    if (!isLogging && !isSdReady) {
        alert("SD Card ga kebaca cok! Pasang dulu yang bener terus restart ESP32-nya.");
        return;
    }

    const newState = !isLogging;
    const rate = parseInt(document.getElementById('sampling-rate').value);

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                is_logging: newState,
                sampling_rate_ms: rate
            })
        });
        if (response.ok) {
            updateLoggingUI(newState);
        }
    } catch (error) {
        alert("Gagal konek ke ESP32");
    }
}

async function updateConfig() {
    const rate = parseInt(document.getElementById('sampling-rate').value);

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                sampling_rate_ms: rate
            })
        });
        if (response.ok) {
            alert("Config berhasil di-save!");
        }
    } catch (error) {
        alert("Gagal save config");
    }
}

fetchData();