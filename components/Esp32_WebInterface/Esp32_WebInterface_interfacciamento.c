/*
  Esp32_WebInterface_interfacciamento.c
*/

#include "Esp32_WebInterface.h"
#include <stdlib.h> // Per atoi e atof

// =============================================================================
// DAL WEB AL PROGRAMMA (ValueFromWeb)
// Queste funzioni aggiornano le variabili reali usando i valori ricevuti dal web
// =============================================================================

// Assegna direttamente il valore ricevuto dal web alla char puntata
void WebInterface_ValueFromWebByPchar(WebInterface_t *self, int indice, const char *valore_dal_web)
{
    // Verifica di sicurezza: indice valido, puntatore esistente e dimensione definita
    if (indice >= 0 && indice < self->num_oggetti &&
        self->oggetti[indice].Pchar != NULL &&
        self->oggetti[indice].LcharP > 0)
    {
        // 1. Copiamo direttamente dalla stringa ricevuta dal web alla RAM del programma
        // Usiamo strncpy con LcharP per evitare qualsiasi buffer overflow
        strncpy(self->oggetti[indice].Pchar, valore_dal_web, self->oggetti[indice].LcharP - 1);

        // 2. Garantiamo la chiusura della stringa (fondamentale in C)
        self->oggetti[indice].Pchar[self->oggetti[indice].LcharP - 1] = '\0';

        // 3. Sanificazione post-copia
        // Ora puliamo i caratteri '|' direttamente nella memoria finale
        // per essere sicuri che la stringa sia conforme al protocollo
        for (int i = 0; self->oggetti[indice].Pchar[i] != '\0'; i++)
        {
            if (self->oggetti[indice].Pchar[i] == '|')
            {
                self->oggetti[indice].Pchar[i] = '-';
            }
        }
    }
}

// Assegna il valore ricevuto in formato char dal web e lo applica alla bool puntata
void WebInterface_ValueFromWebByPbool(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pbool != NULL)
    {
        if (strcmp(self->oggetti[indice].value, "true") == 0 || strcmp(self->oggetti[indice].value, "1") == 0)
        {
            *(self->oggetti[indice].Pbool) = true;
        }
        else if (strcmp(self->oggetti[indice].value, "false") == 0 || strcmp(self->oggetti[indice].value, "0") == 0)
        {
            *(self->oggetti[indice].Pbool) = false;
        }
    }
}

// Assegna il valore ricevuto in formato char dal web e lo applica alla int puntata
void WebInterface_ValueFromWebByPint(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pint != NULL)
    {
        *(self->oggetti[indice].Pint) = atoi(self->oggetti[indice].value);
    }
}

// Assegna il valore ricevuto in formato char dal web e lo applica alla float puntata
void WebInterface_ValueFromWebByPfloat(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pfloat != NULL)
    {
        *(self->oggetti[indice].Pfloat) = (float)atof(self->oggetti[indice].value);
    }
}

// =============================================================================
// DAL PROGRAMMA AL WEB (ValueFromProgram)
// Queste funzioni leggono le variabili reali e aggiornano le stringhe per il web
// =============================================================================

// Questa funzione legge dalla RAM (Pchar) e restituisce il puntatore sanificato
// Viene chiamata solitamente durante il salvataggio o l'invio dati
const char *WebInterface_ValueFromProgramByPchar(WebInterface_t *self, int indice)
{
    if (indice < 0 || indice >= self->num_oggetti || self->oggetti[indice].Pchar == NULL)
    {
        return "";
    }

    Oggetto_t *obj = &self->oggetti[indice];

    // SANIFICAZIONE "AL VOLO"
    // Prima di restituire la stringa per salvarla o inviarla,
    // ci assicuriamo che non ci siano pipe che romperebbero il protocollo
    for (int i = 0; obj->Pchar[i] != '\0'; i++)
    {
        if (obj->Pchar[i] == '|')
        {
            obj->Pchar[i] = '-';
        }
    }

    return obj->Pchar;
}

// Legge lo steto della bool puntata e aggiorna il valore dell'oggetto come "true" o "false"
void WebInterface_ValueFromProgramByPbool(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pbool != NULL)
    {
        strncpy(self->oggetti[indice].value, *(self->oggetti[indice].Pbool) ? "true" : "false", MAX_VALORE - 1);
        self->oggetti[indice].value[MAX_VALORE - 1] = '\0';
    }
}

// Legge il valore della int puntata e aggiorna il valore dell'oggetto come stringa
void WebInterface_ValueFromProgramByPint(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pint != NULL)
    {
        snprintf(self->oggetti[indice].value, MAX_VALORE, "%d", *(self->oggetti[indice].Pint));
    }
}

// Legge il valore della float puntata e aggiorna il valore dell'oggetto come stringa
void WebInterface_ValueFromProgramByPfloat(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pfloat != NULL)
    {
        snprintf(self->oggetti[indice].value, MAX_VALORE, "%.3f", *(self->oggetti[indice].Pfloat));
    }
}

// =============================================================================
// ESECUZIONE PULSANTI
// =============================================================================

void WebInterface_exeFunzione(WebInterface_t *self, int indice)
{
    if (indice >= 0 && indice < self->num_oggetti && self->oggetti[indice].Pfunzione != NULL)
    {
        self->oggetti[indice].Pfunzione(); // Esegue la funzione puntata
    }
}