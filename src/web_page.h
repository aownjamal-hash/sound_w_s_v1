const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>بوابة الإنذار - إدارة السجلات</title>
    <style>
        body { font-family: 'Tahoma', sans-serif; background: #1e1e2f; color: #eee; margin: 0; padding: 20px; }
        .container { max-width: 600px; margin: auto; background: #2d2d3a; border-radius: 20px; padding: 20px; }
        h1, h2 { text-align: center; color: #ffaa33; }
        .status-card { background: #1e1e2a; border-radius: 15px; padding: 15px; margin-bottom: 20px; }
        .status-row { display: flex; justify-content: space-between; margin: 10px 0; }
        .badge-on { background: #28a745; padding: 4px 12px; border-radius: 20px; }
        .badge-off { background: #dc3545; padding: 4px 12px; border-radius: 20px; }
        .live-data { background: #00000055; border-radius: 15px; padding: 10px; text-align: center; margin: 10px 0; }
        input, button { width: 100%; padding: 12px; margin: 8px 0; border-radius: 10px; border: none; }
        input { background: #3a3a4a; color: white; }
        button { background: #ffaa33; font-weight: bold; cursor: pointer; transition: 0.3s; }
        button:hover { opacity: 0.8; }
        .btn-secondary { background: #4e73df; color: white; }
        .btn-success { background: #28a745; color: white; }
        .btn-info { background: #17a2b8; color: white; }
        .btn-danger { background: #dc3545; color: white; }
        .flex-btns { display: flex; gap: 10px; flex-wrap: wrap; }
        .flex-btns button { flex: 1; min-width: 120px; }
        hr { margin: 20px 0; border-color: #444; }
        .note { font-size: 0.8em; color: #aaa; margin-top: 5px; text-align: center; }
    </style>
</head>
<body>
<div class="container">
    <h1>🔊 بوابة الإنذار</h1>
    <div class="status-card">
        <div class="status-row"><span>🌐 الشبكة الخلوية:</span><span id="networkStatus" class="badge-off">تحميل...</span></div>
        <div class="status-row"><span>📶 قوة الإشارة:</span><span id="signalDisplay">0%</span></div>
        <div class="status-row"><span>📡 WiFi:</span><span id="wifiStatus" class="badge-off">--</span></div>
        <div class="status-row"><span>⏰ الوقت:</span><span id="currentTime">--</span></div>
        <div class="status-row"><span>🔫 الوضع:</span><span id="smsMode" class="badge-off">--</span></div>
    </div>
    
    <div class="live-data">
        🎤 الصوت: <strong id="soundValue">0</strong> &nbsp;|&nbsp;
        🔢 العداد: <strong id="counterValue">0</strong>
    </div>

    <div class="flex-btns">
        <button id="toggleModeBtn" class="btn-secondary">🔇 تبديل الصامت</button>
        <button id="calibrateBtn" style="background:#f0ad4e;">⚙️ معايرة</button>
        <button id="viewLogsBtn" class="btn-info">📋 عرض السجل</button>
        <button id="clearLogsBtn" class="btn-danger">🗑️ مسح السجل</button>
        <button id="updateNetworkBtn" class="btn-info">🔄 تحديث الشبكة</button>
        <button id="syncTimeBtn" class="btn-info">🕒 مزامنة الوقت</button>
        <button id="reconnectBtn" class="btn-info">📡 إعادة الاتصال بالشبكة</button>
    </div>

    <hr>
    <h2>⚙️ الإعدادات</h2>
    <form id="settingsForm">
        <label>رقم الهاتف:</label>
        <input type="text" id="phone" name="phone" value="%PHONE%">
        <label>نص الرسالة:</label>
        <input type="text" id="msg" name="msg" value="%MSG%">
        <button type="submit" class="btn-success">حفظ الإعدادات</button>
    </form>
    <div class="note">🔹 حالة الشبكة وقوة الإشارة لا تحدث تلقائياً. اضغط "تحديث حالة الشبكة" لقراءتها من المودم.</div>
</div>

<script>
    let phoneFocused = false, msgFocused = false;
    const phoneInput = document.getElementById('phone');
    const msgInput = document.getElementById('msg');

    phoneInput.onfocus = () => phoneFocused = true;
    phoneInput.onblur = () => phoneFocused = false;
    msgInput.onfocus = () => msgFocused = true;
    msgInput.onblur = () => msgFocused = false;

    function fetchStatus() {
        fetch('/state').then(r => r.json()).then(d => {
            document.getElementById('networkStatus').innerText = d.networkReady ? "✅ متصلة" : "❌ غير متصلة";
            document.getElementById('networkStatus').className = d.networkReady ? "badge-on" : "badge-off";
            document.getElementById('signalDisplay').innerText = d.signalStrength + "%";
            document.getElementById('wifiStatus').innerText = d.wifiEnabled ? "✅ شغالة" : "❌ متوقفة";
            document.getElementById('wifiStatus').className = d.wifiEnabled ? "badge-on" : "badge-off";
            document.getElementById('currentTime').innerText = d.currentDateTime || "--";
            document.getElementById('smsMode').innerText = d.smsEnabled ? "إرسال" : "صامت";
            document.getElementById('smsMode').className = d.smsEnabled ? "badge-on" : "badge-off";
            document.getElementById('soundValue').innerText = d.lastSound;
            document.getElementById('counterValue').innerText = d.counter;
            if (!phoneFocused) phoneInput.value = d.phone;
            if (!msgFocused) msgInput.value = d.msg;
        });
    }

    // Log Management Actions
    document.getElementById('viewLogsBtn').onclick = () => window.open('/viewlogs', '_blank');
    
    document.getElementById('clearLogsBtn').onclick = () => {
        if(confirm('هل أنت متأكد من مسح جميع السجلات؟')) {
            fetch('/clearlogs', { method: 'POST' })
                .then(r => r.text())
                .then(msg => alert(msg));
        }
    };

    document.getElementById('toggleModeBtn').onclick = () => fetch('/setMode', { method: 'POST' }).then(() => fetchStatus());
    document.getElementById('calibrateBtn').onclick = () => {
        fetch('/calibrate', { method: 'POST' }).then(r => r.text()).then(m => alert(m));
    };
    document.getElementById('updateNetworkBtn').onclick = () => fetch('/updatenetwork', { method: 'POST' }).then(() => fetchStatus());
    document.getElementById('syncTimeBtn').onclick = () => fetch('/synctime', { method: 'POST' }).then(r => r.text()).then(m => alert(m));
    document.getElementById('reconnectBtn').onclick = () => fetch('/reconnectnetwork', { method: 'POST' }).then(() => fetchStatus());
    document.getElementById('settingsForm').onsubmit = (e) => {
        e.preventDefault();
        fetch('/save?phone=' + encodeURIComponent(phoneInput.value) + '&msg=' + encodeURIComponent(msgInput.value), { method: 'POST' })
            .then(() => alert('تم الحفظ'));
    };

    setInterval(fetchStatus, 3000);
    fetchStatus();
</script>
</body>
</html>
)rawliteral";