#include "include/castom_allocators.h"

// Разделяемый контекст памяти
FixedArena::FixedArena(std::size_t block_size, size_t blocks_count)
{
    // Округляем размер блока вверх до кратности 16 (или другого max_align_t)
    std::size_t alignment = alignof(std::max_align_t);
    std::size_t aligned_block_size = (block_size + alignment - 1) & ~(alignment - 1);

    m_max_bytes = aligned_block_size * blocks_count;
    m_buffer = new uint8_t[m_max_bytes];
    m_allocated_bytes = 0;
}

FixedArena::FixedArena(FixedArena&& other)
{
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

FixedArena::~FixedArena()
{
    delete[] m_buffer;
}

void* FixedArena::allocate(std::size_t bytes, std::size_t align)
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

void FixedArena::deallocate(void*) noexcept
{
}

void FixedArena::reset() noexcept
{
    m_allocated_bytes = 0;
}

std::size_t FixedArena::used() const noexcept
{
    return m_allocated_bytes;
}

std::size_t FixedArena::available() const noexcept
{
    return m_max_bytes - m_allocated_bytes;
}


// Разделяемый контекст памяти
void MemoryPool::allocate_chunk()
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

MemoryPool::MemoryPool(std::size_t block_size, size_t blocks_per_chunk)
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

MemoryPool::~MemoryPool()
{
    for(uint8_t* c : m_chunks)
    {
        delete[] c;
    }
}

void* MemoryPool::allocate(std::size_t n)
{
    if (n > m_block_size)
    {
        throw std::bad_alloc();
    }

    if(m_free_list == nullptr)
    {
        allocate_chunk();
    }

    Node* node = m_free_list;
    m_free_list = node->next;
    return node;
}

void MemoryPool::deallocate(void* p) noexcept
{
    if(p == nullptr)
        return;

    Node* node = reinterpret_cast<Node*>(p);
    node->next = m_free_list;
    m_free_list = node;
}
