# Блокировка повторного включения модуля
if(TARGET compiler_flags)
    return()
endif()

# Создаем интерфейсную библиотеку для флагов компиляции
add_library(compiler_flags INTERFACE)

# Настройки стандарта C++ прописываем прямо в интерфейс,
# чтобы они автоматически наследовались всеми, кто линкуется с compiler_flags
target_compile_features(compiler_flags INTERFACE cxx_std_20)

if (MSVC)
    target_compile_options(compiler_flags INTERFACE 
        /W4          # Высокий уровень предупреждений
        /WX          # Трактовать предупреждения как ошибки
        /permissive- # Соответствие стандарту C++ без расширений MSVC
    )
else ()
    # Общие флаги для GCC и Clang
    target_compile_options(compiler_flags INTERFACE 
        -Wall 
        -Wextra 
        -pedantic 
        -Werror
        -Wshadow               # Предупреждать о затеняющих переменных
        -Wnon-virtual-dtor     # Предупреждать о деструкторах без virtual
        -Wcast-align           # Предупреждать о невыровненных кастах
        -Wunused               # Предупреждать о неиспользуемом коде
    )
    
    # Специфичные флаги для Clang (если проект компилируется им)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(compiler_flags INTERFACE 
            -Wsign-conversion  # Предупреждать об изменении знака при приведении типов
        )
    endif()
endif()

