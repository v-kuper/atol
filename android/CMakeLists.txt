cmake_minimum_required(VERSION 3.18.1)
project(react-native-atol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Простое определение путей
set(RN_DIR "${CMAKE_CURRENT_LIST_DIR}/../node_modules/react-native")

# Системные библиотеки
find_library(log-lib log)

# Проверим существование JSI файлов
set(JSI_SOURCES "${RN_DIR}/ReactCommon/jsi/jsi/jsi.cpp")
if(EXISTS ${JSI_SOURCES})
    message(STATUS "Found JSI source: ${JSI_SOURCES}")
else()
    message(FATAL_ERROR "JSI source not found: ${JSI_SOURCES}")
endif()

# Создаем библиотеку
add_library(react-native-atol SHARED
    ../cpp/atol.cpp
    cpp-adapter.cpp
    ${JSI_SOURCES}
)

# Include директории
target_include_directories(react-native-atol PRIVATE
    ../cpp
    ${RN_DIR}/ReactCommon
    ${RN_DIR}/ReactCommon/jsi
    ${RN_DIR}/ReactCommon/jsi/jsi
)

# Линковка
target_link_libraries(react-native-atol
    ${log-lib}
    android
)

# Базовые флаги
target_compile_options(react-native-atol PRIVATE
    -fexceptions
    -frtti
    -DFOLLY_NO_CONFIG
    -DFOLLY_MOBILE=1
    -DFOLLY_USE_LIBCPP=1
)
# Базовые флаги
target_compile_options(react-native-atol PRIVATE
    -fexceptions
    -frtti
    -DFOLLY_NO_CONFIG
    -DFOLLY_MOBILE=1
    -DFOLLY_USE_LIBCPP=1
)
