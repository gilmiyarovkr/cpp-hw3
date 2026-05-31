#include <iostream>
#include <map>
#include <memory>
#include <cstddef>
#include <new>

#include <memory>
#include <memory_resource> // monotonic_buffer_resource

// Разделяемый контекст памяти.
// Позволяет выделять память под узлы фиксированного размера, когда этот размер станет известен.
class FixedArenaState {
private:
    std::size_t max_elements;   // Максимальное количество элементов
    std::size_t element_size = 0; // Размер одного элемента (установится при первом allocate)
    char* buffer = nullptr;     // Указатель на выделенный блок памяти
    std::size_t allocated_count = 0; // Количество уже выданных элементов

public:
    explicit FixedArenaState(std::size_t count) : max_elements(count) {}

    // Освобождаем всю память за раз при уничтожении состояния
    ~FixedArenaState() {
        delete[] buffer;
    }

    // Запрещаем копирование, чтобы избежать двойного удаления
    FixedArenaState(const FixedArenaState&) = delete;
    FixedArenaState& operator=(const FixedArenaState&) = delete;

    void* allocate_element(std::size_t bytes) {
        // Ленивая инициализация буфера при первом запросе памяти
        if (buffer == nullptr) {
            element_size = bytes;
            buffer = new char[max_elements * element_size];
        }

        // Проверка: map всегда запрашивает по 1 узлу. Если запрошено больше или превышен лимит — ошибка.
        if (bytes > element_size || allocated_count >= max_elements) {
            throw std::bad_alloc();
        }

        void* ptr = buffer + (allocated_count * element_size);
        allocated_count++;
        return ptr;
    }

    // По условию, одиночное освобождение памяти ничего не делает
    void deallocate_element(void*) noexcept {}

    std::size_t get_allocated_count() const noexcept {
        return allocated_count;
    }

    std::size_t get_max_elements() const noexcept {
        return max_elements;
    }
};

// STL-совместимый аллокатор
template <typename T>
class FixedMapAllocator {
private:
    std::shared_ptr<FixedArenaState> state; // Разделяемое состояние для корректного rebind внутри map

public:
    using value_type = T;

    // Конструктор инициализации с указанием лимита элементов
    explicit FixedMapAllocator(std::size_t max_elements)
        : state(std::make_shared<FixedArenaState>(max_elements)) {}

    // Шаблонный конструктор копирования (Обязателен для std::map)
    template <typename U>
    FixedMapAllocator(const FixedMapAllocator<U>& other) noexcept : state(other.state) {}

    // Доступ к состоянию для других инстанций шаблона
    template <typename U>
    friend class FixedMapAllocator;

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n == 0)
            return nullptr;

        // std::map запрашивает память порциями по n = 1, но размер sizeof(T) — это размер узла дерева.
        void* ptr = state->allocate_element(n * sizeof(T));

        std::cout << "[Allocated] Узел размером " << sizeof(T) << " байт. Занято: "
                  << state->get_allocated_count() << " из " << state->get_max_elements()
                  << " элементов.\n";

        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept {
        state->deallocate_element(p);
    }

    // Сравнение аллокаторов. Они равны, если разделяют одно состояние.
    template <typename U>
    bool operator==(const FixedMapAllocator<U>& other) const noexcept {
        return state == other.state;
    }

    template <typename U>
    bool operator!=(const FixedMapAllocator<U>& other) const noexcept {
        return state != other.state;
    }
};

//---------------------------------------------------------------------------
int main(int, char **) 
{
    const std::size_t MAX_ELEMENTS = 3;

    using Key = int;
    using Value = int;
    using Pair = std::pair<const Key, Value>;

    // Создаем тип карты с нашим фиксированным аллокатором
    using FixedMap = std::map<Key, Value, std::less<Key>, FixedMapAllocator<Pair>>;

    std::cout << "--- Инициализация std::map (Лимит: " << MAX_ELEMENTS << " элемента) ---\n";
    FixedMap my_map{FixedMapAllocator<Pair>(MAX_ELEMENTS)};

    std::cout << "\n--- Заполнение в пределах лимита ---\n";
    try {
        my_map[1] = 1;
        my_map[2] = 2;
        my_map[3] = 3;
        std::cout << "Успешно добавлено " << my_map.size() << " элемента.\n";
    } catch (const std::bad_alloc& e) {
        std::cerr << "Ошибка: не удалось выделить память в пределах лимита!\n";
    }

    std::cout << "\n--- Попытка превысить лимит (4-й элемент) ---\n";
    try {
        my_map[4] = 4; // Должно вызвать исключение bad_alloc
    } catch (const std::bad_alloc& e) {
        std::cout << "Перехвачено ожидаемое исключение: Аллокатор заблокировал превышение лимита!\n";
    }

    std::cout << "\n--- Завершение работы. Вся память арены освободится автоматически ---\n";
    return 0;
}
