const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>بوابة الإنذار</title>
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
        button { background: #ffaa33; font-weight: bold; cursor: pointer; }
        .btn-secondary { background: #4e73df; color: white; }
        .btn-success { background: #28a745; color: white; }
        .btn-info { background: #17a2b8; color: white; }
        .flex-btns { display: flex; gap: 10px; flex-wrap: wrap; }
        .flex-btns button { flex: 1; }
        hr { margin: 20px 0; border-color: #444; }
        .note { font-size: 0.8em; color: #aaa; margin-top: 5px; text-align: center; }
    </style>
</head>
<body>
<div class="container">
    <h1>🔊 بوابة الإنذار</h1>
    <div class="status-card">
        <div class="status-row"><span>🌐 الشبكة الخلوية:</span><span id="networkStatus" class="badge-off">غير متصلة</span></div>
        <div class="status-row"><span>📶 قوة الإشارة:</span><span id="signalDisplay">0%</span></div>
        <div class="status-row"><span>📡 نقطة الوصول (WiFi):</span><span id="wifiStatus" class="badge-off">متوقفة</span></div>
        <div class="status-row"><span>⏰ الوقت:</span><span id="currentTime">--</span></div>
        <div class="status-row"><span>🔫 الوضع:</span><span id="smsMode" class="badge-off">صامت</span></div>
        <div class="status-row"><span>🎛️ العتبة:</span><span id="thresholdVal">--</span></div>
        <div class="status-row"><span>📊 القاعدة:</span><span id="idleBaseVal">--</span></div>
    </div>
    <div class="live-data">
        🎤 الصوت: <strong id="soundValue">0</strong> &nbsp;|&nbsp;
        🔢 العداد: <strong id="counterValue">0</strong>
    </div>
    <div class="flex-btns">
        <button id="toggleModeBtn" class="btn-secondary">🔇 تبديل الصامت</button>
        <button id="calibrateBtn" style="background:#f0ad4e;">⚙️ معايرة</button>
        <button id="toggleWifiBtn" class="btn-info">📡 تبديل الواي فاي</button>
        <button id="updateNetworkBtn" class="btn-info">🔄 تحديث حالة الشبكة</button>
        <button id="syncTimeBtn" class="btn-info">🕒 مزامنة الوقت</button>
        <button id="reconnectBtn" class="btn-info">📡 إعادة الاتصال بالشبكة</button>
    </div>
    <hr>
    <h2>⚙️ إعدادات متقدمة</h2>
    <div class="flex-btns">
        <input type="number" id="newIdleBase" placeholder="القاعدة">
        <input type="number" id="newThreshold" placeholder="العتبة">
        <button id="updateThresholdBtn" class="btn-success">تحديث</button>
    </div>
    <form id="settingsForm">
        <label>رقم الهاتف (بدون مفتاح، سيُضاف +967 تلقائياً):</label>
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
    phoneInput.addEventListener('focus', () => phoneFocused = true);
    phoneInput.addEventListener('blur', () => phoneFocused = false);
    msgInput.addEventListener('focus', () => msgFocused = true);
    msgInput.addEventListener('blur', () => msgFocused = false);

    function fetchStatus() {
        fetch('/state')
            .then(r => r.json())
            .then(d => {
                document.getElementById('networkStatus').innerText = d.networkReady ? "✅ متصلة" : "❌ غير متصلة";
                document.getElementById('networkStatus').className = d.networkReady ? "badge badge-on" : "badge badge-off";
                document.getElementById('signalDisplay').innerText = d.signalStrength + "%";
                document.getElementById('wifiStatus').innerText = d.wifiEnabled ? "✅ شغالة" : "❌ متوقفة";
                document.getElementById('wifiStatus').className = d.wifiEnabled ? "badge badge-on" : "badge badge-off";
                document.getElementById('currentTime').innerText = d.currentDateTime || "--";
                document.getElementById('smsMode').innerText = d.smsEnabled ? "ارسال" : "صامت";
                document.getElementById('smsMode').className = d.smsEnabled ? "badge badge-on" : "badge badge-off";
                document.getElementById('thresholdVal').innerText = d.threshold;
                document.getElementById('idleBaseVal').innerText = d.idleBase;
                document.getElementById('soundValue').innerText = d.lastSound;
                document.getElementById('counterValue').innerText = d.counter;
                if (!phoneFocused && d.phone) phoneInput.value = d.phone;
                if (!msgFocused && d.msg) msgInput.value = d.msg;
            })
            .catch(e => console.error(e));
    }

    document.getElementById('toggleModeBtn').onclick = () => fetch('/setMode', { method: 'POST' }).then(() => fetchStatus());
    document.getElementById('calibrateBtn').onclick = () => {
        fetch('/calibrate', { method: 'POST' }).then(res => res.text()).then(msg => alert(msg)).then(() => setTimeout(fetchStatus, 2000));
    };
    document.getElementById('toggleWifiBtn').onclick = () => {
        fetch('/toggleWiFi', { method: 'POST' }).then(() => fetchStatus());
    };
    document.getElementById('updateNetworkBtn').onclick = () => {
        fetch('/updatenetwork', { method: 'POST' }).then(() => fetchStatus());
    };
    document.getElementById('syncTimeBtn').onclick = () => {
        fetch('/synctime', { method: 'POST' }).then(res => res.text()).then(msg => alert(msg)).then(() => fetchStatus());
    };
    document.getElementById('reconnectBtn').onclick = () => {
        fetch('/reconnect', { method: 'POST' }).then(res => res.text()).then(msg => alert(msg)).then(() => setTimeout(fetchStatus, 5000));
    };
    document.getElementById('updateThresholdBtn').onclick = () => {
        let idleBase = document.getElementById('newIdleBase').value;
        let thr = document.getElementById('newThreshold').value;
        let url = '/setThreshold';
        if(idleBase) url += '?idleBase=' + idleBase;
        if(thr) url += (idleBase?'&':'?')+'threshold='+thr;
        fetch(url, { method: 'POST' }).then(() => { alert('تم التحديث'); fetchStatus(); });
    };
    document.getElementById('settingsForm').onsubmit = (e) => {
        e.preventDefault();
        let phone = phoneInput.value;
        let msg = msgInput.value;
        fetch('/save?phone=' + encodeURIComponent(phone) + '&msg=' + encodeURIComponent(msg), { method: 'POST' })
            .then(() => { alert('تم حفظ الإعدادات'); fetchStatus(); });
    };
    setInterval(fetchStatus, 2000);
    fetchStatus();
</script>
</body>
</html>
)rawliteral";