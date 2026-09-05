#include <iostream>
#include <map>

#include <utility>

#include <castom_allocators.h>
#include <castom_forward_list.h>

//---------------------------------------------------------------------------
unsigned long long factorial(int n)
{
    if(n < 0)
        return 0;

    unsigned long long result = 1;
    for(int i = 1; i <= n; ++i)
    {
        result *= static_cast<unsigned long long>(i);
    }
    return result;
}

//---------------------------------------------------------------------------
int main()
{
    constexpr int MAX_ELEMENTS = 10;

    using Key = int;
    using Value = int;
    using Pair = std::pair<const Key, Value>;

    std::map<const Key, Value> stl_map;
    for(int i = 0; i < MAX_ELEMENTS; ++i)
    {
        stl_map[i] = static_cast<int>(factorial(i));
    }

    std::map<Key, Value, std::less<Key>, CastomAllocator<Pair, MemoryPool>> alloc_map{CastomAllocator<Pair, MemoryPool>(MAX_ELEMENTS)};
    for(int i = 0; i < MAX_ELEMENTS; ++i)
    {
        alloc_map[i] = static_cast<int>(factorial(i));
    }

    // вывод на экран всех значений (ключ и значение разделены пробелом) хранящихся в контейнере
    for (const auto& [key, value] : alloc_map)
    {
        std::cout << key << " " << value << "\n";
    }

    MyForwardList<int> flist_std_alloc;
    for(int i = 0; i < MAX_ELEMENTS; ++i)
    {
        flist_std_alloc.push_front(static_cast<int>(factorial(i)));
    }

    MyForwardList<int, CastomAllocator<int, MemoryPool>> flist_castom_alloc{CastomAllocator<int, MemoryPool>(MAX_ELEMENTS)};
    for(int i = 0; i < MAX_ELEMENTS; ++i)
    {
        flist_castom_alloc.push_front(static_cast<int>(factorial(i)));
    }

    // вывод на экран всех значений
    for (const auto& value : flist_castom_alloc)
    {
        std::cout << value << "\n";
    }

    return 0;
}
