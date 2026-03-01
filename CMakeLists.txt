# CMakeLists.txt - Configurazione del progetto ESP-IDF per la centralina smart con interfaccia web e MQTT
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Centralina_Smart)