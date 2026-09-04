#include <iostream>
#include <map>
#include <memory>
#include <cstddef>
#include <new>
#include <vector>
#include <chrono>

// Разделяемый контекст памяти
class FixedArena {
private:
    std::size_t m_max_bytes = 0;       // Точный максимальный объем в байтах
    uint8_t* m_buffer = nullptr;       // Монолитный буфер
    std::size_t m_allocated_bytes = 0; // Сколько байт занято

public:
    explicit FixedArena(std::size_t byte_count) : m_max_bytes(byte_count), m_buffer(new uint8_t[byte_count]), m_allocated_bytes(0) {}

    ~FixedArena() {
        delete[] m_buffer;
    }

    FixedArena(const FixedArena&) = delete;
    FixedArena& operator=(const FixedArena&) = delete;

    FixedArena(FixedArena&& other){
        if(this != &other)
        {
            delete[] m_buffer;
            m_buffer = other.m_buffer;
            m_max_bytes = other.m_max_bytes;
            m_allocated_bytes = other.m_allocated_bytes;
            other.m_buffer = nullptr;
            other.m_max_bytes = 0;
            other.m_allocated_bytes = 0;
        }
    }

    void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t))
    {
        std::size_t align_offset = (m_allocated_bytes + align - 1) & ~(align - 1);
        if(align_offset + bytes > m_max_bytes)
        {
            throw std::bad_alloc();
        }

        void *ptr = m_buffer + align_offset;
        m_allocated_bytes = align_offset + bytes;

        return ptr;
    }

    void deallocate(void*) noexcept {}

    void reset() noexcept {m_allocated_bytes = 0;}

    std::size_t used() const noexcept {return m_allocated_bytes;}
    std::size_t available() const noexcept {return m_max_bytes - m_allocated_bytes;}
};

// STL-совместимый аллокатор
template <typename T>
class FixedMapAllocator {
private:
    std::size_t alloc_element;
    std::shared_ptr<FixedArena> mem_alloc;

public:
    using value_type = T;

    explicit FixedMapAllocator(std::size_t max_elements)
        : alloc_element(max_elements), mem_alloc(nullptr)
    {}

    template <typename U>
    FixedMapAllocator(const FixedMapAllocator<U>& other) noexcept : alloc_element(other.alloc_element), mem_alloc(other.mem_alloc) {}

    template <typename U> friend class FixedMapAllocator;

    T* allocate(std::size_t n)
    { // [[nodiscard]]

        if (n == 0)
            return nullptr;

        if(mem_alloc == nullptr)
        {
            size_t total_size = alloc_element * sizeof (T);
            mem_alloc = std::make_shared<FixedArena>(total_size);
        }

        void* ptr = mem_alloc->allocate(n * sizeof(T));

//        std::cout << "[Allocated] Request: " << n << " x " << sizeof(T)
//                  << " bytes. \n";

        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept
    {
        mem_alloc->deallocate(p);
    }

    template<typename U>
    struct rebind
    {
        typedef FixedMapAllocator<U> other;
    };

    template <typename U>
    bool operator==(const FixedMapAllocator<U>& other) const noexcept { return alloc_element == other.alloc_element && mem_alloc == other.mem_alloc; }
    template <typename U>
    bool operator!=(const FixedMapAllocator<U>& other) const noexcept { return alloc_element != other.alloc_element || mem_alloc != other.mem_alloc; }
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

    void allocate_chunk()
    {
        size_t chunk_size = m_block_size * m_blocks_per_chunk;
        uint8_t* new_chunk = new uint8_t[chunk_size];
        if(new_chunk == nullptr)
            throw std::bad_alloc();

        m_chunks.push_back(new_chunk);

        for(size_t i = 0; i < m_blocks_per_chunk; ++i)
        {
            Node* curr = reinterpret_cast<Node*>(new_chunk + (i * m_block_size));
            curr->next = m_free_list;
            m_free_list = curr;
        }
    }

public:
    explicit MemoryPool(std::size_t block_size, size_t blocks_per_chunk)
        : m_block_size(block_size < sizeof(Node) ? sizeof(Node) : block_size),
          m_blocks_per_chunk(blocks_per_chunk),
          m_free_list(nullptr)
    {
        size_t remainder = m_block_size % alignof(std::max_align_t);
        if(remainder != 0)
        {
            m_block_size += alignof(std::max_align_t) - remainder;
        }
    }

    ~MemoryPool()
    {
        for(uint8_t* c : m_chunks)
        {
            delete[] c;
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&& other) = delete;

    void* allocate(std::size_t n)
    {
        if(m_free_list == nullptr)
        {
            allocate_chunk();
        }

        Node* node = m_free_list;
        m_free_list = node->next;
        return node;
    }

    void deallocate(void* p) noexcept
    {
        if(p == nullptr)
            return;

        Node* node = reinterpret_cast<Node*>(p);
        node->next = m_free_list;
        m_free_list = node;
    }
};

// STL-совместимый аллокатор
template <typename T>
class PoolMapAllocator {
private:
    std::size_t alloc_element;
    std::shared_ptr<MemoryPool> mem_alloc;

public:
    using value_type = T;

    explicit PoolMapAllocator(std::size_t max_elements)
        : alloc_element(max_elements), mem_alloc(nullptr)
    {}

    template <typename U>
    PoolMapAllocator(const PoolMapAllocator<U>& other) noexcept : alloc_element(other.alloc_element), mem_alloc(other.mem_alloc) {}

    template <typename U> friend class PoolMapAllocator;

    T* allocate(std::size_t n)
    { // [[nodiscard]]

        if (n == 0)
            return nullptr;

        if(mem_alloc == nullptr)
        {
            //size_t total_size = alloc_element * sizeof (T);
            mem_alloc = std::make_shared<MemoryPool>(sizeof (T), alloc_element);
        }

        void* ptr = mem_alloc->allocate(n * sizeof (T));

        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept
    {
        mem_alloc->deallocate(p);
    }

    template<typename U>
    struct rebind
    {
        typedef PoolMapAllocator<U> other;
    };

    template <typename U>
    bool operator==(const PoolMapAllocator<U>& other) const noexcept { return alloc_element == other.alloc_element && mem_alloc == other.mem_alloc; }
    template <typename U>
    bool operator!=(const PoolMapAllocator<U>& other) const noexcept { return alloc_element != other.alloc_element || mem_alloc != other.mem_alloc; }
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
    CastomAllocator(const CastomAllocator<U, Type>& other) noexcept : alloc_element(other.alloc_element), mem_alloc(other.mem_alloc) {}

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
        typedef CastomAllocator<U, Type> other;
    };

    template <typename U>
    bool operator==(const CastomAllocator<U, Type>& other) const noexcept { return alloc_element == other.alloc_element && mem_alloc == other.mem_alloc; }
    template <typename U>
    bool operator!=(const CastomAllocator<U, Type>& other) const noexcept { return alloc_element != other.alloc_element || mem_alloc != other.mem_alloc; }
};

//---------------------------------------------------------------------------
unsigned long long factorial(int n)
{
    if(n < 0)
        return 0;

    unsigned long long result = 1;
    for(int i = 1; i < n; ++i)
    {
        result *= i;
    }
    return result;
}

//---------------------------------------------------------------------------
template <typename T, typename Allocator = std::allocator<T>>
class MyForwardList
{
private:
    struct Node
    {
        T value;
        Node* next;

        template<typename... Args>
        Node(Node* nextNode, Args&&... args)
            : value(std::forward<Args>(args)...), next(nextNode) {}
    };

    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using AllocTraits = std::allocator_traits<NodeAllocator>;

    Node* m_head = nullptr;
    NodeAllocator m_allocator;

public:
    MyForwardList() : m_head(nullptr), m_allocator(){}
    explicit MyForwardList(const Allocator& alloc) : m_head(nullptr), m_allocator(alloc){}
    MyForwardList(const MyForwardList&) = delete;
    MyForwardList& operator=(const MyForwardList&) = delete;
    MyForwardList(const MyForwardList&& other) noexcept
        : m_head(other.m_head), m_allocator(std::move(other.alloc))
    {
        other.m_head = nullptr;
    }

    ~MyForwardList()
    {
        clear();
    }

    template<typename... Args>
    void emplace_front(Args&&... args)
    {
        Node* newNode = AllocTraits::allocate(m_allocator, 1);
        try
        {
            AllocTraits::construct(m_allocator, newNode, m_head, std::forward<Args>(args)...);
        }
        catch (...)
        {
            AllocTraits::deallocate(m_allocator, newNode, 1);
        }

        m_head = newNode;
    }

    void push_front(const T& value)
    {
        emplace_front(value);
    }

    void push_front(T&& value)
    {
        emplace_front(std::move(value));
    }

    void clear()
    {
        Node* current = m_head;
        while(current != nullptr)
        {
            Node* next = current->next;
            AllocTraits::destroy(m_allocator, current);
            AllocTraits::deallocate(m_allocator, current, 1);
            current = next;
        }
        m_head = nullptr;
    }

    bool empty() const
    {
        return m_head == nullptr;
    }
};



//---------------------------------------------------------------------------
int main()
{
    constexpr std::size_t MAX_ELEMENTS = 3;

    using Key = int;
    using Value = int;
    using Pair = std::pair<const Key, Value>;

    using FixedMap = std::map<Key, Value, std::less<Key>, CastomAllocator<Pair, MemoryPool>>;

    std::cout << "--- Инициализация std::map ---\n";
    FixedMap test_map{CastomAllocator<Pair, MemoryPool>(MAX_ELEMENTS)};

    std::cout << "\n--- Заполнение в пределах лимита ---\n";
    try {
        test_map[1] = 1;
        test_map[2] = 2;
        test_map[3] = 3;
        std::cout << "Успешно добавлено. Текущий размер map: " << test_map.size() << "\n";
    } catch (const std::bad_alloc&) {
        std::cerr << "Ошибка: не удалось выделить память в пределах лимита!\n";
    }

    std::cout << "\n--- Попытка превысить лимит (4-й рабочий элемент) ---\n";
    try {
        test_map[4] = 4;
        std::cout << "ОШИБКА: Лимит превышен, но исключение не выброшено!\n";
    } catch (const std::bad_alloc&) {
        std::cout << "Успех: Перехвачено ожидаемое исключение bad_alloc!\n";
        std::cout << "Элементов в карте на момент падения: " << test_map.size() << "\n";
    }

    auto it = test_map.end();
    test_map.erase(test_map.begin());

    std::cout << std::endl;

    // auto start_time = std::chrono::high_resolution_clock::now();
    // for(int j = 0; j < 1000; ++j)
    // {
    //     std::map<const Key, Value> stl_map;
    //     for(int i = 0; i < 10; ++i)
    //     {
    //         stl_map[i] = static_cast<int>(factorial(i));
    //     }
    // }
    // auto end_time = std::chrono::high_resolution_clock::now();
    // auto duration = end_time - start_time;
    // double dt = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
    // std::cout << "Time work stl map = " << dt << "ns" << std::endl;

    // start_time = std::chrono::high_resolution_clock::now();
    // for(int j = 0; j < 1000; ++j)
    // {
    //     FixedMap my_map{FixedMapAllocator<Pair>(10)};
    //     for(int i = 0; i < 10; ++i)
    //     {
    //         my_map[i] = static_cast<int>(factorial(i));
    //     }
    // }
    // end_time = std::chrono::high_resolution_clock::now();
    // duration = end_time - start_time;
    // dt = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
    // std::cout << "Time work fixed map = " << dt << "ns" << std::endl;



    // using PoolMap = std::map<Key, Value, std::less<Key>, PoolMapAllocator<Pair>>;
    // std::cout << "--- Инициализация PoolMap ---\n";

    // start_time = std::chrono::high_resolution_clock::now();
    // for(int j = 0; j < 1000; ++j)
    // {
    //     PoolMap test_pool_map{PoolMapAllocator<Pair>(10)};
    //     for(int i = 0; i < 10; ++i)
    //     {
    //         test_pool_map[i] = static_cast<int>(factorial(i));
    //     }
    // }
    // end_time = std::chrono::high_resolution_clock::now();
    // duration = end_time - start_time;
    // dt = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
    // std::cout << "Time work pool map = " << dt << "ns" << std::endl;


    // // TODO вывод значений


    // std::cout << "--- Инициализация MyForwardList ---\n";

    // start_time = std::chrono::high_resolution_clock::now();
    // for(int j = 0; j < 1000; ++j)
    // {
    //     MyForwardList<int> my_fl;
    //     for(int i = 0; i < 10; ++i)
    //     {
    //         my_fl.push_front(static_cast<int>(factorial(i)));
    //     }
    // }
    // end_time = std::chrono::high_resolution_clock::now();
    // duration = end_time - start_time;
    // dt = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
    // std::cout << "Time my forward list = " << dt << "ns" << std::endl;

    // std::cout << "--- Инициализация MyForwardList 2 ---\n";

    // start_time = std::chrono::high_resolution_clock::now();
    // for(int j = 0; j < 1000; ++j)
    // {
    //     MyForwardList<int, FixedMapAllocator<int>> my_fl{FixedMapAllocator<int>(10)};
    //     for(int i = 0; i < 10; ++i)
    //     {
    //         my_fl.push_front(static_cast<int>(factorial(i)));
    //     }
    // }
    // end_time = std::chrono::high_resolution_clock::now();
    // duration = end_time - start_time;
    // dt = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
    // std::cout << "Time my forward list = " << dt << "ns" << std::endl;

    // std::cout << std::endl;





    return 0;
}
