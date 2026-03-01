/*
  Esp32_WebInterface_oggetti_func.c
*/

#include "Esp32_WebInterface.h"
#include "esp_log.h"

// --- HELPERS PRIVATI ---

// Helper per inizializzare i campi comuni di un nuovo oggetto
// Restituisce false se non c'è spazio o se il nome è già presente e compila i campi nome e tipo, resetta i puntatori e flag HA per sicurezza
static bool preparaNuovoOggetto(WebInterface_t *self, const char *nome, const char *tipo)
{
    // 1. Controllo spazio disponibile
    if (self->num_oggetti >= MAX_OGGETTI)
    {
        ESP_LOGE("WEB_INTERFACE", "Errore: Raggiunto numero massimo di oggetti (%d)", MAX_OGGETTI);
        return false;
    }

    // 2. Pulizia TOTALE della memoria dell'oggetto corrente
    memset(&self->oggetti[self->num_oggetti], 0, sizeof(Oggetto_t));

    // 1. Salviamo l'etichetta esattamente come inserita dall'utente (con spazi e maiuscole)
    strncpy(self->oggetti[self->num_oggetti].etichetta, nome, MAX_NOME - 1);

    // 3. Gestione e SANIFICAZIONE del nome
    char nome_pulito[MAX_NOME];
    strncpy(nome_pulito, nome, MAX_NOME - 1);
    nome_pulito[MAX_NOME - 1] = '\0';

    // assicuriamoci che il nome sia pulito da caratteri che potrebbero creare problemi nel web (es. '|')
    // e con home assistant (es. spazi)
    // --- DISCRIMINAZIONE SANIFICAZIONE ---
    WebInterface_Sanitize_name(nome_pulito);

    // 4. Controllo duplicati sul nome già pulito
    // Importante: così evitiamo che "Soglia Nuova" e "Soglia_Nuova" vengano visti come diversi
    if (WebInterface_nomedoppio(self, nome_pulito))
    {
        ESP_LOGE("WEB_INTERFACE", "Errore: Oggetto '%s' (da '%s') gia' esistente", nome_pulito, nome);
        return false;
    }

    // 5. Assegnazione definitiva
    // Usiamo l'indice corrente num_oggetti
    WebInterface_setNome(self, self->num_oggetti, nome_pulito);
    WebInterface_setTipo(self, self->num_oggetti, tipo);

    // Nota: num_oggetti NON viene incrementato qui,
    // ma nelle funzioni AddVar dopo che hanno finito le configurazioni.

    return true;
}

// --- FUNZIONI DI AGGIUNTA (Ex AddVar) ---

// Aggiunge un ponte con una CHAR (Sola Lettura)
int WebInterface_AddVar_Char(WebInterface_t *self, const char *nome, char *Pchar)
{
    if (!preparaNuovoOggetto(self, nome, "char"))
        return -1;
    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pchar = Pchar;

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una BOOL (Sola Lettura)
int WebInterface_AddVar_Bool(WebInterface_t *self, const char *nome, bool *Pbool, bool HA)
{
    if (!preparaNuovoOggetto(self, nome, "bool"))
        return -1;
    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pbool = Pbool;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "binary_sensor", MAX_HA_TIPO);
    }

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una INT (Sola Lettura)
int WebInterface_AddVar_Int(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM)
{
    if (!preparaNuovoOggetto(self, nome, "int"))
        return -1;
    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pint = Pint;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "sensor", MAX_HA_TIPO);
        strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM);
    }

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una FLOAT (Sola Lettura)
int WebInterface_AddVar_Float(WebInterface_t *self, const char *nome, float *Pfloat, bool HA, const char *UDM)
{
    if (!preparaNuovoOggetto(self, nome, "float"))
        return -1;

    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pfloat = Pfloat;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "sensor", MAX_HA_TIPO);
        strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM);
    }

    self->num_oggetti++;
    return idx;
}

// --- FUNZIONI DI AGGIUNTA SET (Modificabili) ---

// Aggiunge un ponte con una CHAR (Modificabile)
int WebInterface_AddVar_Set_Char(WebInterface_t *self, const char *nome, char *Pchar, unsigned int LcharP, const char *default_fisso)
{
    // preparaNuovoOggetto gestisce l'allocazione nell'array
    if (!preparaNuovoOggetto(self, nome, "char_set"))
        return -1;

    int idx = self->num_oggetti;

    self->oggetti[idx].LcharP = LcharP;
    self->oggetti[idx].Pchar = Pchar;
    self->oggetti[idx].valore_default = default_fisso;

    // Inizializzazione: se la RAM è vuota, carichiamo il default
    // Usiamo setValue perché contiene già la logica di Sanitize
    if (Pchar != NULL && Pchar[0] == '\0')
    {
        WebInterface_setValue(self, idx, default_fisso);
    }
    else
    {
        // Se c'era già qualcosa (es. caricato da NVS), sanifichiamo i '|'
        WebInterface_ValueFromWebByPchar(self, idx, Pchar);
    }

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una INT (Modificabile)
int WebInterface_AddVar_Set_Int(WebInterface_t *self, const char *nome, int *Pint, const char *default_fisso, bool HA, const char *UDM)
{
    // preparaNuovoOggetto esegue il memset, quindi value è già ""
    if (!preparaNuovoOggetto(self, nome, "int_set"))
        return -1;

    int idx = self->num_oggetti;

    self->oggetti[idx].min = -1000.0f;
    self->oggetti[idx].max = 1000.0f;
    self->oggetti[idx].Pint = Pint;
    self->oggetti[idx].HA = HA;

    // Gestione Home Assistant e Unità di Misura
    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "number", MAX_HA_TIPO - 1);
        self->oggetti[idx].ha_tipo[MAX_HA_TIPO - 1] = '\0';

        if (UDM != NULL)
        {
            strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM - 1);
            self->oggetti[idx].UDM[MAX_UDM - 1] = '\0';
        }
    }

    // 1. ASSEGNAZIONE DEL DEFAULT
    // Ora assegniamo semplicemente il puntatore alla stringa che passi dal main
    self->oggetti[idx].valore_default = default_fisso;

    // 2. INIZIALIZZAZIONE DELLA VARIABILE
    // Se il valore in NVS non esiste (lo vedremo dopo), inizializziamo la RAM col default
    // Chiamando setValue, convertiamo la stringa "default_fisso" nel numero reale in *Pint
    WebInterface_setValue(self, idx, default_fisso);

    // 3. LOG DI CONFERMA
    ESP_LOGI("Interface", "Registrato INT: %s [Default: %s]", nome, default_fisso);

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una FLOAT (Modificabile)
int WebInterface_AddVar_Set_Float(WebInterface_t *self, const char *nome, float *Pfloat, const char *default_fisso, bool HA, const char *UDM)
{
    // preparaNuovoOggetto esegue il memset, quindi oggetti[idx].value è inizialmente ""
    if (!preparaNuovoOggetto(self, nome, "float_set"))
        return -1;

    int idx = self->num_oggetti;

    self->oggetti[idx].min = -1000.0f;
    self->oggetti[idx].max = 1000.0f;
    self->oggetti[idx].Pfloat = Pfloat;
    self->oggetti[idx].HA = HA;

    // Gestione Home Assistant e Unità di Misura
    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "number", MAX_HA_TIPO - 1);
        self->oggetti[idx].ha_tipo[MAX_HA_TIPO - 1] = '\0';

        if (UDM != NULL)
        {
            strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM - 1);
            self->oggetti[idx].UDM[MAX_UDM - 1] = '\0';
        }
    }

    // 1. ASSEGNAZIONE DEL DEFAULT
    // Puntiamo alla stringa costante passata (es: "20.50")
    self->oggetti[idx].valore_default = default_fisso;

    // 2. INIZIALIZZAZIONE DELLA VARIABILE REALE
    // Trasformiamo la stringa di default nel valore numerico dentro *Pfloat
    if (Pfloat != NULL)
    {
        WebInterface_setValue(self, idx, default_fisso);
    }

    // 3. LOG DI CONFERMA
    ESP_LOGI("Interface", "Registrato FLOAT: %s [Default: %s]", nome, default_fisso);

    self->num_oggetti++;
    return idx;
}

// Aggiunge un ponte con una BOOL (Modificabile)
int WebInterface_AddVar_Set_Bool(WebInterface_t *self, const char *nome, bool *Pbool, const char *default_fisso, bool HA)
{
    // preparaNuovoOggetto esegue il memset, pulendo tutto
    if (!preparaNuovoOggetto(self, nome, "bool_set"))
        return -1;

    int idx = self->num_oggetti;

    self->oggetti[idx].Pbool = Pbool;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        // Per i bool modificabili su Home Assistant usiamo tipicamente "switch"
        strncpy(self->oggetti[idx].ha_tipo, "switch", MAX_HA_TIPO - 1);
        self->oggetti[idx].ha_tipo[MAX_HA_TIPO - 1] = '\0';
    }

    // 1. ASSEGNAZIONE DEL DEFAULT
    // Salviamo il puntatore alla stringa passata (es: "true" o "false")
    self->oggetti[idx].valore_default = default_fisso;

    // 2. INIZIALIZZAZIONE DELLA VARIABILE REALE
    // Converte la stringa "true"/"false" nel valore booleano reale in *Pbool
    if (Pbool != NULL)
    {
        WebInterface_setValue(self, idx, default_fisso);
    }

    // 3. LOG DI CONFERMA
    ESP_LOGI("Interface", "Registrato BOOL: %s [Default: %s]", nome, default_fisso);

    self->num_oggetti++;
    return idx;
}

// --- FUNZIONI SPECIALI ---

// Aggiunge un bottone che esegue una funzione al click
int WebInterface_AddButton(WebInterface_t *self, const char *nome, const char *valore, void (*Pfunzione)(), bool HA)
{
    if (!preparaNuovoOggetto(self, nome, "button"))
        return -1;
    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pfunzione = Pfunzione;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "button", MAX_HA_TIPO);
    }

    strncpy(self->oggetti[idx].value, valore, MAX_VALORE - 1);
    self->num_oggetti++;
    return idx;
}

// Aggiunge un oggetto di debug (Sola Lettura, non esposto a Home Assistant) che visualizza una stringa (es. log di sistema)

int WebInterface_AddDebug(WebInterface_t *self, const char *nome, unsigned int dimensione)
{
    // 1. preparaNuovoOggetto gestisce Sanitize del nome e memset della struttura
    if (!preparaNuovoOggetto(self, nome, "debug"))
        return -1;

    int idx = self->num_oggetti;

    // 2. ALLOCAZIONE DINAMICA
    // Creiamo il buffer nello Heap dell'ESP32
    self->oggetti[idx].Pchar = (char *)malloc(dimensione);

    if (self->oggetti[idx].Pchar == NULL)
    {
        ESP_LOGE("WEB_INTERFACE", "Errore: Memoria insufficiente per il debug '%s'", nome);
        return false;
    }

    // 3. Configurazione parametri
    self->oggetti[idx].LcharP = dimensione;

    // 4. Inizializzazione pulita
    // Azzeriamo il buffer e mettiamo il segnale "-!x" per il Web
    memset(self->oggetti[idx].Pchar, 0, dimensione);
    strcpy(self->oggetti[idx].Pchar, "-!x");

    self->num_oggetti++;
    return idx;
}

// Aggiunge un contatore tempo legato a una variabile int che rappresenta i secondi totali.
// da web si puo resettare a zero
int WebInterface_AddCountInt(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM)
{
    if (!preparaNuovoOggetto(self, nome, "countInt"))
        return -1;
    // L'indice è l'ultimo slot occupato
    int idx = self->num_oggetti;
    self->oggetti[idx].Pint = Pint;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        strncpy(self->oggetti[idx].ha_tipo, "number", MAX_HA_TIPO);
        strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM);
    }

    self->num_oggetti++;
    return idx;
}

// Aggiunge un contatore tempo (verra convertito dal client in Giorni:Ore:Minuti:Secondi) legato a una variabile int (secondi)
// da web non si puo resettare a zero
int WebInterface_AddCountDHMS(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM)
{
    if (!preparaNuovoOggetto(self, nome, "countDHMS"))
        return -1;

    int idx = self->num_oggetti;
    self->oggetti[idx].Pint = Pint;
    self->oggetti[idx].HA = HA;

    if (HA)
    {
        // Per Home Assistant lo esponiamo come un sensore numerico
        strncpy(self->oggetti[idx].ha_tipo, "sensor", MAX_HA_TIPO);
        strncpy(self->oggetti[idx].UDM, UDM, MAX_UDM);
    }

    self->num_oggetti++;
    return idx;
}