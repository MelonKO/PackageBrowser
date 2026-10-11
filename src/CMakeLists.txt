# Ядро приложения: доменная модель и сервисы.
# UI (Qt Widgets) добавляется на следующих этапах roadmap.

cmake_minimum_required(VERSION 3.16)

# Агрегация всех целей ядра. На данном этапе создаётся библиотека-каркас,
# к которой на этапах 1-3 roadmap будут добавляться CatalogService, Scanner,
# Cache, Plugin Host.
add_library(pb_core STATIC core.cpp)

target_include_directories(pb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(pb_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)