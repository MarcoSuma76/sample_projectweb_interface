//Suma Marco - 2026-03-21.
// Script per la gestione dell'interfaccia web dell'ESP32, con funzioni per ottenere e visualizzare i dati,
//  inviare comandi al server, gestire i contatori e le configurazioni, e interagire con le funzionalità WiFi e OTA.
//  Utilizza modelli HTML predefiniti per creare dinamicamente gli elementi dell'interfaccia in base 
// alla configurazione ricevuta dal server.

// --- MODELLI ---
var tpl_char = '<div><em class="emoji">ℹ️</em><span class="key">chiave:</span><span id="chiave" class="value">valore</span></div>';
var tpl_bool = '<div><em class="emoji">👁️</em><span class="key">chiave:</span><input type="checkbox" id="chiave" disabled></div>';
var tpl_int = '<div><em class="emoji">📊</em><span class="key">chiave:</span><span id="chiave" class="value">valore</span></div>';
var tpl_float = '<div><em class="emoji">📈</em><span class="key">chiave:</span><span id="chiave" class="value">valore</span></div>';

// --- MODELLI AGGIORNATI ---

// 1. CHAR SET: Allineato a destra dell'input
var tpl_char_set = '<div><em class="emoji">📝</em><span class="key">chiave:</span><span id="chiave" class="value">valore</span></div>' +
    '<div style="margin-top:10px; display:flex; gap:5px; align-items: center;">' +
    '<input type="text" id="chiave_inputText" style="flex-grow:1; margin:0;">' +
    '<button class="bottone_aus" onclick="cambiaChar_set(\'chiave\')" style="width:auto; margin:0;">OK</button>' +
    '<div style="margin-left: 15px;"><button class="btn-reset" onclick="resetSingolo(\'chiave\')" title="Ripristina Default">↺</button></div></div>';


// 2. BOOL SET: Allineato sulla stessa riga della checkbox
var tpl_bool_set = '<div style="display:flex; align-items:center; justify-content:space-between;">' +
    '<div><em class="emoji">🔌</em><span class="key">chiave:</span><input type="checkbox" id="chiave" onchange="inviaChekBox(\'chiave\')"></div>' +
    '<div style="margin-left: 15px;"><button class="btn-reset" onclick="resetSingolo(\'chiave\')" title="Ripristina Default">↺</button></div></div>';


// 3. INT SET: Inserito nel wrapper per simmetria
var tpl_int_set = '<div class="key"><em class="emoji">🎚️</em> chiave</div>' +
    '<div class="wrapper" style="display:flex; align-items:center; gap:2px;">' +
    '<span onclick="modifica(\'chiave\',-100)">-100</span><span onclick="modifica(\'chiave\',-10)">-10</span><span onclick="modifica(\'chiave\',-1)">-1</span>' +
    '<span class="num" id="chiave" data-type="int_set" data-min="-999999" data-max="999999">valore</span>' +
    '<span onclick="modifica(\'chiave\',1)">+1</span><span onclick="modifica(\'chiave\',10)">+10</span><span onclick="modifica(\'chiave\',100)">+100</span></div>' +
    '<div style="margin-left: 15px;"><button class="btn-reset" onclick="resetSingolo(\'chiave\')" title="Ripristina Default">↺</button></div></div>';

// 4. FLOAT SET: Inserito nel wrapper
var tpl_float_set = '<div class="key"><em class="emoji">🎯</em> chiave</div>' +
    '<div class="wrapper" style="display:flex; align-items:center; gap:2px;">' +
    '<span onclick="modifica(\'chiave\',-0.1)">-.1</span><span onclick="modifica(\'chiave\',-0.01)">-.01</span><span onclick="modifica(\'chiave\',-0.001)">-.001</span>' +
    '<span class="num" id="chiave" data-type="float_set" data-min="-999999" data-max="999999">valore</span>' +
    '<span onclick="modifica(\'chiave\',0.001)">+.001</span><span onclick="modifica(\'chiave\',0.01)">+.01</span><span onclick="modifica(\'chiave\',0.1)">+.1</span></div>' +
    '<div style="margin-left: 15px;"><button class="btn-reset" onclick="resetSingolo(\'chiave\')" title="Ripristina Default">↺</button></div></div>';


var tpl_debug = '<div><em class="emoji">📟</em><span class="key">chiave</span><textarea id="chiave" class="debug" rows="10" readonly></textarea><button class="bottone_set" style="margin-top:10px" onclick="document.getElementById(\'chiave\').textContent=\'\'">🗑️ Pulisci Log</button></div>';
var tpl_countDHMS = '<div><em class="emoji">⏳</em><span class="key">chiave:</span><div id="chiave" class="value" data-type="countDHMS" style="float:none; display:block; text-align:right; margin-bottom:10px;">valore</div><button class="bottone_aus" style="width:auto; padding:5px 15px; margin-top:5px;" onclick="resetCount(\'chiave\')">🔄 Reset</button></div>';
var tpl_countInt = '<div><em class="emoji">🔢</em><span class="key">chiave:</span><div id="chiave" class="value" data-type="countInt" style="float:none; display:block; text-align:right; margin-bottom:10px;">valore</div><button class="bottone_aus" style="width:auto; padding:5px 15px; margin-top:5px;" onclick="resetCount(\'chiave\')">🔄 Reset</button></div>';
var tpl_button = '<div><em class="emoji">⚡</em><span class="key">chiave</span><button id="chiave" class="bottone_dinamico" onclick="invia(\'chiave\')">valore</button></div>';

// Funzione per ottenere la configurazione degli oggetti web dal server, creare dinamicamente gli elementi HTML corrispondenti e memorizzare i valori in variabili globali, con gestione speciale per il titolo
async function getOggettiWeb() {
    try {
        const r = await fetch("getOggettiWeb");
        const t = await r.text();

        // Dividiamo per '|' per ottenere i singoli oggetti
        let blocchi = t.split("|");

        // Puliamo il contenitore prima di ricaricare (opzionale, dipende dal tuo setup)
        // document.getElementById("main").innerHTML = ""; 

        for (let i = 0; i < blocchi.length; i++) {
            let riga = blocchi[i].trim();
            if (riga === "") continue;

            // Separiamo Metadati e Valore usando il primo '='
            let equalIndex = riga.indexOf('=');
            if (equalIndex === -1) continue;

            let meta = riga.substring(0, equalIndex);
            let valore = riga.substring(equalIndex + 1);

            // Separiamo i metadati (tipo:chiave:etichetta) usando i due punti
            let partiMeta = meta.split(':');
            let tipo = partiMeta[0];
            let chiave = partiMeta[1];
            let etichetta = partiMeta[2] || chiave; // Se non c'è etichetta, usa la chiave

            // --- GESTIONE SPECIALE MQTT ---
            if (tipo === "mqttconfig") {
                let dati = valore.split(','); // Dividiamo host, porta, user, pw
                if (document.getElementById('mqtt_host')) document.getElementById('mqtt_host').value = dati[0] || "";
                if (document.getElementById('mqtt_port')) document.getElementById('mqtt_port').value = dati[1] || "1883";
                if (document.getElementById('mqtt_user')) document.getElementById('mqtt_user').value = dati[2] || "";
                if (document.getElementById('mqtt_pass')) document.getElementById('mqtt_pass').value = dati[3] || "";
                continue; // Non creare un elemento grafico nel main
            }
            // ------------------------------

            window[chiave] = valore; // Memorizza globalmente

            if (tipo === "titolo") {
                document.getElementById("titolo").innerHTML = `🤖 ${valore}`;
                document.title = valore;
                continue;
            }

            let html = window["tpl_" + tipo];
            if (html) {
                let div = document.createElement("div");
                div.className = "oggetto";
                div.id = "container_" + chiave;

                // 1. Sostituiamo chiave e valore nel template originale
                let render = html.replace(/chiave/g, chiave).replace(/valore/g, valore);

                // 2. Sostituiamo il testo "chiave:" con "etichetta:" per la vista utente
                // Questo trasforma "luce_cantina:" in "Luce Cantina:"
                render = render.replace('>' + chiave + ':', '>' + etichetta + ':');

                // 3. Aggiungiamo il tastino per rinominare (solo se è un tipo interattivo)
                if (["bool_set", "int_set", "float_set"].includes(tipo)) {
                    render += `
                    <div id="edit_box_${chiave}" class="rename-container" style="display:none;">
                     <input type="text" id="new_label_${chiave}" value="${etichetta}" 
                       onkeyup="if(event.key==='Enter') inviaNuovaEtichetta('${chiave}')">
                       <div class="rename-buttons">
                       <button onclick="inviaNuovaEtichetta('${chiave}')" class="btn-ok">OK</button>
                       <button onclick="toggleRename('${chiave}')" class="btn-cancel">X</button>
                       </div></div><div style="text-align:right;"><small id="link_${chiave}" onclick="toggleRename('${chiave}')" 
                       style="color:#007bff; cursor:pointer; font-size:0.75em; text-decoration:underline;">
                       ✏️ Rinomina</small></div>`;
                }

                div.innerHTML = render;

                if (tipo.includes("count")) document.getElementById("count_container").appendChild(div);
                else document.getElementById("main").appendChild(div);
            }
        }
    } catch (e) { console.error("Err Caricamento:", e); }
}
// Funzione per ottenere i dati dal server, aggiornare le variabili globali e i valori visualizzati, con gestione speciale per log e contatori
async function getData() {
    try {
        const r = await fetch("getDati");
        const t = await r.text();
        t.split("|").filter(x => x.includes("=")).forEach(item => {
            let [id, val] = item.split("=");
            if (id === "system_log") {
                let elLog = document.getElementById("system_log");
                if (elLog && val !== "-!x" && val.length > 0) {
                    elLog.insertAdjacentText('beforeend', val + "\n");
                    elLog.scrollTop = elLog.scrollHeight;
                }
                return;
            }
            // Gestione speciale per il Wi-Fi RSSI, con icona colorata in base alla forza del segnale
            if (id === "wifi_rssi") {
                let el = document.getElementById(id);
                if (el) {
                    let rssi = parseInt(val);
                    let icona = "🔴";
                    if (rssi > -60) icona = "🟢";      // Segnale Ottimo
                    else if (rssi > -80) icona = "🟡"; // Segnale Sufficiente

                    el.innerHTML = `${icona} ${val} dBm`;
                }
                return; // Passa al prossimo elemento del forEach
            }

            //per lo stato MQTT, mostriamo un'icona verde o rossa invece del testo
            if (id === "mqtt_status") {
                let el = document.getElementById(id);
                if (el) {
                    // 1 = Connesso (Verde), 0 = Disconnesso (Rosso)
                    el.innerHTML = (val === "1") ? "🔗" : "❌";
                }
                return;
            }
            // Per gli altri dati, aggiorniamo normalmente
            let el = document.getElementById(id);
            if (!el) return;
            window[id] = val;
            if (el.type === "checkbox") el.checked = (val === "true" || val === "1");
            else if (el.tagName === "TEXTAREA") { if (val !== "-!x" && val.length > 0) el.insertAdjacentText('afterbegin', val + "\n"); }
            else if (el.getAttribute("data-type") === "countDHMS") el.innerHTML = formattaSecondi(val);
            else if (el.tagName !== "BUTTON") el.innerHTML = val;
        });
    } catch (e) { }
}

// Funzione per modificare un valore numerico (intero o float) in base al delta, con rispetto dei limiti min/max e aggiornamento del display, chiamando poi la funzione invia per salvare il nuovo valore
function modifica(n, delta) {
    let el = document.getElementById(n); if (!el) return;
    let tipo = el.getAttribute("data-type");
    let min = el.hasAttribute("data-min") ? Number(el.getAttribute("data-min")) : -999999;
    let max = el.hasAttribute("data-max") ? Number(el.getAttribute("data-max")) : 999999;
    let v = parseFloat(window[n]) + delta;
    if (v < min) v = min; if (v > max) v = max;
    if (tipo === "int_set") v = Math.round(v);
    else v = parseFloat(v.toFixed(3));
    window[n] = v; el.innerHTML = v; invia(n);
}

// Funzione per inviare un dato al server, con il nome della variabile e il suo valore, utilizzando POST e aggiornando i dati dopo l'invio
function invia(n) {
    fetch("putDati", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: n + "=" + window[n]
    }).then(() => getData());
}

// Funzione per inviare lo stato del checkbox al server quando viene modificato, aggiornando la variabile globale e chiamando la funzione invia
function inviaChekBox(n) { window[n] = document.getElementById(n).checked; invia(n); }

// Funzione per inviare il nuovo valore di un campo di testo al server quando viene modificato, aggiornando la variabile globale e chiamando la funzione invia
function cambiaChar_set(n) {
    let el = document.getElementById(n + "_inputText");
    // Applichiamo la sanificazione prima di salvare e inviare
    let valorePulito = sanitizeInput(el.value);

    // Aggiorniamo il campo visivo se conteneva pipe
    if (el.value !== valorePulito) {
        el.value = valorePulito;
    }

    window[n] = valorePulito;
    invia(n);
}

// Funzione per resettare un contatore, con conferma, azzerando la variabile globale e chiamando la funzione invia
function resetCount(n) { if (confirm("Resettare?")) { window[n] = 0; invia(n); } }

// Funzione per formattare un numero di secondi in un formato leggibile (giorni, ore, minuti, secondi), utilizzata per i contatori di tipo countDHMS
function formattaSecondi(sec) {
    let s = parseInt(sec); if (isNaN(s)) return sec;
    let d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60), r = s % 60;
    return (d > 0 ? d + "d " : "") + [h, m, r].map(v => v < 10 ? "0" + v : v).join(":");
}
// Variabile globale per memorizzare l'ID dell'intervallo di aggiornamento dei dati, in modo da poterlo cancellare quando si cambia l'intervallo o si eseguono azioni che richiedono un refresh manuale
let invID;

// Funzione per cambiare l'intervallo di aggiornamento dei dati, cancellando il precedente e impostando un nuovo intervallo in base al valore del select "refresh"
function cambiaIntervallo() {
    if (invID) clearInterval(invID);
    invID = setInterval(getData, document.getElementById("refresh").value);
}

// Inizializzazione: carica gli oggetti web e imposta l'intervallo di aggiornamento
getOggettiWeb();
// Imposta l'intervallo di aggiornamento dei dati in base al valore del select "refresh"
cambiaIntervallo();

// Funzione per memorizzare i dati nella EEPROM, con conferma
document.getElementById("memorizzaDati").onclick = () => { if (confirm("Salvare i dati modificabili nella Eeprom?")) fetch("memorizzaDati", { method: "POST" }); };

// Funzione per azzerare i dati, con conferma, feedback visivo e ricaricamento della pagina dopo 5 secondi
document.getElementById("azzeraDati").onclick = () => {
    if (confirm("Riporto i dati modificabili ai valori di defoult?")) {
        clearInterval(invID);
        document.getElementById("azzeraDati").innerText = "Riavvio...";
        fetch("azzeraDati", { method: "POST" });
        setTimeout(() => { location.reload(); }, 5000);
    }
};

// Funzione per il reset dell'ESP32, con conferma, feedback visivo e ricaricamento della pagina dopo 5 secondi
document.getElementById("resetEsp32").onclick = () => {
    if (confirm("Riavviare Esp?")) {
        clearInterval(invID);
        document.getElementById("resetEsp32").innerText = "Riavvio...";
        fetch("resetEsp32", { method: "POST" });
        setTimeout(() => { location.reload(); }, 5000);
    }
};

// Funzione per resettare un singolo contatore, con conferma, azzerando la variabile globale e chiamando la funzione invia, 
// specifica per i contatori di tipo countDHMS o countInt
function resetSingolo(n) {
    if (confirm("Ripristinare " + n + " al valore iniziale?")) {
        fetch("resetSingolo", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "nome=" + n
        }).then(() => getData()); // Aggiorna la UI dopo il reset
    }
}

//chiede le reti wifi disponibili e le popola nella select, disabilitando il bottone durante la scansione e mostrando un feedback visivo
async function scansionaWiFi() {
    const select = document.getElementById('wifi_ssid_select');
    const btn = document.getElementById('btn_scan');

    // 1. Feedback visivo (Stile moderno)
    select.classList.add('select-scanning');
    select.innerHTML = '<option value="">Ricerca in corso...</option>';
    btn.disabled = true;
    const originalBtnText = btn.innerText;
    btn.innerText = "Scansione...";

    try {
        // 2. Usiamo il metodo POST e l'URL esatto come nella vecchia versione
        const response = await fetch("scanWiFi", { method: "POST" });

        if (!response.ok) throw new Error("Errore server");

        const reti = await response.json();

        // 3. Pulizia e popolamento
        select.classList.remove('select-scanning');
        select.innerHTML = '';

        if (reti.length === 0) {
            select.innerHTML = '<option value="">Nessuna rete trovata</option>';
        } else {
            // Seleziona una rete come prima opzione vuota
            let defaultOpt = document.createElement("option");
            defaultOpt.value = "";
            defaultOpt.text = "Seleziona una rete...";
            select.add(defaultOpt);

            // Usiamo il formato array (r.ssid e r.rssi) come voleva il tuo ESP32
            reti.forEach(r => {
                let opt = document.createElement("option");
                opt.value = r.ssid;
                opt.text = `${r.ssid} (${r.rssi} dBm)`;
                select.add(opt);
            });
        }
    } catch (e) {
        console.error("Errore scansione:", e);
        select.classList.remove('select-scanning');
        select.innerHTML = '<option value="">Errore nella scansione</option>';
    } finally {
        // 4. Ripristino pulsante
        btn.disabled = false;
        btn.innerText = originalBtnText;
    }
}

// Funzione per connettersi alla rete WiFi selezionata, inviando SSID e password al server tramite POST
function connettiWiFi() {
    let s = document.getElementById("wifi_ssid_select").value;
    let p = document.getElementById("wifi_pass_input").value;
    if (!s) { alert("Seleziona una rete!"); return; }
    fetch("setupWiFi", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "ssid=" + encodeURIComponent(s) + "&pass=" + encodeURIComponent(p)
    }).then(r => r.text()).then(res => alert(res));
}

// Funzione per resettare il Wi-Fi (rimozione credenziali)
function resetWiFi() {
    if (confirm("Vuoi cancellare le credenziali Wi-Fi? L'ESP tornerà in modalità Access Point.")) {
        fetch("clearWiFi", { method: "POST" }).then(() => {
            alert("Wi-Fi resettato. Riavvio...");
        });
    }
}


// Funzione per gestire l'aggiornamento OTA, con feedback visivo e progress bar
function startOTA() {
    const fileInput = document.getElementById('ota_file');
    const status = document.getElementById('ota_status');
    const progressBar = document.getElementById('ota_progress_bar');
    if (fileInput.files.length === 0) return;
    const file = fileInput.files[0];
    let xhr = new XMLHttpRequest();
    xhr.open('POST', '/update');
    document.getElementById('ota_progress_container').style.display = 'block';
    xhr.upload.onprogress = (e) => { if (e.lengthComputable) { let p = Math.round((e.loaded / e.total) * 100); progressBar.style.width = p + '%'; status.innerHTML = p + "%"; } };
    xhr.onload = () => { if (xhr.status === 200) { status.innerHTML = "✅ OK!"; setTimeout(() => location.reload(), 5000); } };
    xhr.send(file);
}

// Funzione per mostrare o nascondere la password WiFi, con cambio icona
// Funzione universale per mostrare/nascondere password
function togglePass(inputId, spanElement) {
    const input = document.getElementById(inputId);
    if (!input) return;

    const isPassword = input.type === 'password';
    input.type = isPassword ? 'text' : 'password';

    // Cambia l'emoji dell'elemento cliccato
    spanElement.textContent = isPassword ? '🙈' : '👁️';
}

// Funzione per sanificare l'input, sostituendo le pipe | con trattini -, per evitare problemi di parsing lato server
function sanitizeInput(text) {
    if (typeof text !== 'string') return text;
    return text.replace(/\|/g, '-');
}

// Aggiunge un listener a tutti gli input di testo per sanificare l'input in tempo reale, evitando che l'utente inserisca caratteri non consentiti come la pipe |
// Utilizziamo la delega degli eventi per gestire input creati dinamicamente
document.body.addEventListener('input', function (e) {
    if (e.target && e.target.type === 'text') {
        const originale = e.target.value;
        const pulito = sanitizeInput(originale);
        if (originale !== pulito) {
            e.target.value = pulito;
            console.warn("Carattere '|' non consentito, sostituito con '-'");
        }
    }
});

// Funzione per inviare la nuova etichetta al server, con conferma, sanificazione dell'input e aggiornamento della UI dopo l'invio
function inviaNuovaEtichetta(chiave) {
    let nuovaLabel = document.getElementById("new_label_" + chiave).value;
    nuovaLabel = nuovaLabel.replace(/[|:=]/g, "-");

    if (nuovaLabel.trim() === "") return;

    fetch("/setLabel", {
        method: "POST",
        body: `nome=${chiave}&label=${encodeURIComponent(nuovaLabel)}`,
        headers: { "Content-Type": "application/x-www-form-urlencoded" }
    }).then(r => {
        if (r.ok) {
            // Invece di un alert fastidioso, facciamo reload o aggiorniamo il testo
            location.reload();
        } else {
            alert("Errore");
        }
    });
}
// Funzione per mostrare o nascondere la casella di modifica dell'etichetta, specifica per i tipi che supportano la rinomina (bool_set, int_set, float_set)
function toggleRename(chiave) {
    let box = document.getElementById('edit_box_' + chiave);
    let link = document.getElementById('link_' + chiave);

    if (box.style.display === 'none') {
        box.style.display = 'block';
        link.style.display = 'none'; // Nascondiamo il link "Rinomina" quando il box è aperto
    } else {
        box.style.display = 'none';
        link.style.display = 'inline'; // Lo rimostriamo quando chiudiamo
    }
}

// Funzione per salvare i parametri MQTT
function salvaMQTT() {
    let host = document.getElementById('mqtt_host').value;
    let port = document.getElementById('mqtt_port').value;
    let user = document.getElementById('mqtt_user').value;
    let pass = document.getElementById('mqtt_pass').value;

    if (!host) { alert("Inserisci l'indirizzo del broker!"); return; }

    let body = `host=${encodeURIComponent(host)}&port=${port}&user=${encodeURIComponent(user)}&pass=${encodeURIComponent(pass)}`;

    fetch("setMQTT", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: body
    }).then(r => {
        if (r.ok) alert("Parametri MQTT salvati! Riavvio...");
    });
}

// Funzione per resettare solo i dati MQTT
function resetMQTT() {
    if (confirm("Vuoi azzerare i parametri MQTT?")) {
        fetch("resetMQTT", { method: "POST" }).then(() => location.reload());
    }
}

// Funzione per inviare il comando di abilitazione/disabilitazione
function toggleMqtt(enable) {
    fetch("/setMqttEnable", {
        method: "POST",
        body: `enabled=${enable ? 1 : 0}`,
        headers: { "Content-Type": "application/x-www-form-urlencoded" }
    });
}

