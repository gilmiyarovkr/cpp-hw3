#pragma once

#include <memory>
#include <cstddef>
#include <iterator>

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

    // итератор
    template <typename ValueType>
    class ForwardListIterator {
    private:
        Node* m_node;
        friend class MyForwardList; // Чтобы список мог конструировать итератор из Node*

        explicit ForwardListIterator(Node* node) : m_node(node) {}

    public:
        // типы итератора (требование STL)
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = ValueType*;
        using reference         = ValueType&;

        // Разыменование
        reference operator*() const { return m_node->value; }
        pointer operator->() const { return &(m_node->value); }

        // Инкремент (префиксный) ++it
        ForwardListIterator& operator++() {
            if (m_node) m_node = m_node->next;
            return *this;
        }

        // Инкремент (постфиксный) it++
        ForwardListIterator operator++(int) {
            ForwardListIterator temp = *this;
            ++(*this);
            return temp;
        }

        // Сравнения
        bool operator==(const ForwardListIterator& other) const { return m_node == other.m_node; }
        bool operator!=(const ForwardListIterator& other) const { return m_node != other.m_node; }
    };

    // Алиасы для публичного использования
    using iterator       = ForwardListIterator<T>;
    using const_iterator = ForwardListIterator<const T>;

    // Методы для получения итераторов
    iterator begin() noexcept { return iterator(m_head); }
    iterator end() noexcept { return iterator(nullptr); }

    const_iterator begin() const noexcept { return const_iterator(m_head); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }

    const_iterator cbegin() const noexcept { return const_iterator(m_head); }
    const_iterator cend() const noexcept { return const_iterator(nullptr); }

    // методы класса
    MyForwardList() : m_head(nullptr), m_allocator(){}
    explicit MyForwardList(const Allocator& alloc) : m_head(nullptr), m_allocator(alloc){}
    MyForwardList(const MyForwardList&) = delete;
    MyForwardList& operator=(const MyForwardList&) = delete;

    MyForwardList(MyForwardList&& other) noexcept
        : m_head(other.m_head), m_allocator(std::move(other.m_allocator))
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
            throw;
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
