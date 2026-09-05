#pragma once

#include <memory>
#include <cstddef>
#include <vector>

// Разделяемый контекст памяти
class FixedArena {
private:
    std::size_t m_max_bytes = 0;       // Точный максимальный объем в байтах
    uint8_t* m_buffer = nullptr;       // Монолитный буфер
    std::size_t m_allocated_bytes = 0; // Сколько байт занято

public:
    explicit FixedArena(std::size_t block_size, size_t blocks_count);
    FixedArena(const FixedArena&) = delete;
    FixedArena& operator=(const FixedArena&) = delete;    
    FixedArena(FixedArena&& other);

    ~FixedArena();

    void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t));

    void deallocate(void*) noexcept;

    void reset() noexcept;

    std::size_t used() const noexcept;
    std::size_t available() const noexcept;
};

// Разделяемый контекст памяти
class MemoryPool {
private:
    struct Node
    {
        Node* next;
    };

    size_t m_block_size;
    size_t m_blocks_per_chunk;
    Node* m_free_list;
    std::vector<uint8_t*> m_chunks;

    void allocate_chunk();

public:
    explicit MemoryPool(std::size_t block_size, size_t blocks_per_chunk);
    ~MemoryPool();

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&& other) = delete;

    void* allocate(std::size_t n);
    void deallocate(void* p) noexcept;
};


template <typename T, typename Type>
class CastomAllocator {
private:
    std::size_t alloc_element;
    std::shared_ptr<Type> mem_alloc;

public:
    using value_type = T;

    explicit CastomAllocator(std::size_t max_elements)
        : alloc_element(max_elements), mem_alloc(nullptr)
    {}

    template <typename U>
    CastomAllocator(const CastomAllocator<U, Type>& other) noexcept
        : alloc_element(other.alloc_element), mem_alloc(other.mem_alloc)
    {
        // Если это первый rebind и арена еще не создана
        if (mem_alloc == nullptr && alloc_element > 0) {
            // Теперь T — это РЕАЛЬНЫЙ тип узла дерева std::map!
            // Создаем арену под точный размер структуры узла
            mem_alloc = std::make_shared<Type>(sizeof(T), alloc_element);
        }
    }

    template <typename U, typename A> friend class CastomAllocator;

    T* allocate(std::size_t n)
    { // [[nodiscard]]

        if (n == 0)
            return nullptr;

        if(mem_alloc == nullptr)
        {
            //size_t total_size = alloc_element * sizeof (T);
            mem_alloc = std::make_shared<Type>(sizeof (T), alloc_element);
        }

        void* ptr = mem_alloc->allocate(n * sizeof (T));

        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept
    {
        if (mem_alloc && p)
        {
            mem_alloc->deallocate(p);
        }
    }

    template<typename U>
    struct rebind
    {
        using other = CastomAllocator<U, Type>;
    };

    template <typename U>
    bool operator==(const CastomAllocator<U, Type>& other) const noexcept { return alloc_element == other.alloc_element && mem_alloc == other.mem_alloc; }
    template <typename U>
    bool operator!=(const CastomAllocator<U, Type>& other) const noexcept { return alloc_element != other.alloc_element || mem_alloc != other.mem_alloc; }
};
