# Блокировка повторного включения модуля
if(CPACK_GENERATOR)
    return()
endif()

# Главный генератор пакетов для Linux-дистрибутивов (Debian, Ubuntu)
set(CPACK_GENERATOR "DEB")

# Основные метаданные пакета
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "example@example.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "GKR <example@example.com>")

# Автоматическое определение архитектуры системы (например, amd64, i386)
# Без этого CPack может подставить системное имя вроде x86_64, что некорректно для DEB
if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
    execute_process(COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# Шаблон имени готового файла: имя-пакета_версия_архитектура.deb
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

# Секция пакета в репозитории (разное/утилиты)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# Подключаем встроенный модуль CPack (должен идти строго в самом конце настройки)
include(CPack)

