# CMakeLists.txt - Configurazione del progetto ESP-IDF per la centralina smart con interfaccia web e MQTT
# Questo CMakeLists.txt è responsabile della configurazione del progetto ESP-IDF, 
# dell'inclusione dei componenti necessari e dell'esecuzione dello script Python
# che prepara i file web (HTML, CSS, JS) per l'interfaccia utente.

# 1. Recupera il percorso di Python usato da ESP-IDF
idf_build_get_property(python PYTHON)

# 2. Definiamo i percorsi
# PYTHON_SCRIPT punta alla root del progetto
set(PYTHON_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../../web_builder.py")

# Elenco dei file sorgente web da "osservare" per le modifiche
set(WEB_SOURCES 
    "${CMAKE_CURRENT_LIST_DIR}/web_src/index.html"
    "${CMAKE_CURRENT_LIST_DIR}/web_src/style.css"
    "${CMAKE_CURRENT_LIST_DIR}/web_src/script.js"
)

# 3. Eseguiamo lo script Python ogni volta che CMake viene configurato
message(STATUS "Esecuzione web_builder da: ${PYTHON_SCRIPT}")
execute_process(
    COMMAND ${python} "${PYTHON_SCRIPT}"
    RESULT_VARIABLE script_res
)

if(NOT script_res EQUAL 0)
    message(FATAL_ERROR "Errore durante l'esecuzione di web_builder.py")
endif()

# 4. TRUCCO MAGICO: Diciamo a CMake che se cambiano i file in WEB_SOURCES, 
# deve considerare il progetto "da riconfigurare". 
# Questo farà sì che il martello riesegua lo script Python.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${WEB_SOURCES})

# 5. Registrazione del componente (lista pulita senza commenti interni)
idf_component_register(
    SRCS 
        "Esp32_WebInterface_wifi.c"
        "Esp32_WebInterface.c"
        "Esp32_WebInterface_oggetti_func.c"
        "Esp32_WebInterface_server_func.c"
        "Esp32_WebInterface_interfacciamento.c"
        "Esp32_WebInterface_eeprom_func.c"
        "Esp32_WebInterface_HA.c"
        "Esp32_WebInterface_dns.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_http_server 
        nvs_flash 
        mqtt 
        esp_wifi 
        esp_netif 
        esp_event 
        lwip 
        app_update
)