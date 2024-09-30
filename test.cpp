#include "include/em/index_map.h"

#include <string>

// Commands to test this:
//    clang++ test.cpp -std=c++20 -Wall -Wextra -pedantic-errors -Wconversion -Werror -g -o build/test && echo running... && build/test && echo ok
//    cl /EHsc test.cpp /std:c++latest /Fe:build/test /Fo:build/test.obj && echo running... && build/test.exe && echo ok

struct Data {int data = 0;};


#define CHECK_ARGS(...) \
    template class em::IndexMap<__VA_ARGS__>; \
    template class em::detail::IndexMap::KeyValueRef<em::IndexMap<__VA_ARGS__>, false>; \
    template class em::detail::IndexMap::KeyValueRef<em::IndexMap<__VA_ARGS__>, true>; \
    template class em::detail::IndexMap::KeyValueIter<em::IndexMap<__VA_ARGS__>, false>; \
    template class em::detail::IndexMap::KeyValueIter<em::IndexMap<__VA_ARGS__>, true>; \
    template class em::detail::IndexMap::KeyValueView<em::IndexMap<__VA_ARGS__>, false>; \
    template class em::detail::IndexMap::KeyValueView<em::IndexMap<__VA_ARGS__>, true>; \
    static_assert(std::ranges::random_access_range<em::IndexMap<__VA_ARGS__>::key_value_view>); \
    static_assert(std::ranges::random_access_range<em::IndexMap<__VA_ARGS__>::key_value_const_view>);

#define CHECK_ARGS_NONVOID(...) \
    CHECK_ARGS(__VA_ARGS__) \
    static_assert(std::ranges::random_access_range<em::IndexMap<__VA_ARGS__>::value_view>);

CHECK_ARGS_NONVOID(std::string, unsigned int            )
CHECK_ARGS_NONVOID(std::string, unsigned long long      )
CHECK_ARGS_NONVOID(std::string, unsigned int      , Data)
CHECK_ARGS_NONVOID(std::string, unsigned long long, Data)
CHECK_ARGS        (void       , unsigned int            )
CHECK_ARGS        (void       , unsigned long long      )
CHECK_ARGS        (void       , unsigned int      , Data)
CHECK_ARGS        (void       , unsigned long long, Data)

struct A {int x = 0;};

constexpr void Check(bool value)
{
    if (!value)
        throw std::runtime_error("Assertion failed.");
}

constexpr void DetailMustThrow(std::string_view message, auto &&func)
{
    try
    {
        func();
    }
    catch (std::exception &e)
    {
        Check(e.what() == message);
        return;
    }

    Check(false);
}

#define MUST_THROW(message, ...) DetailMustThrow(message, [&]{__VA_ARGS__;})

int main()
{
    // Value of `max_size()`.
    static_assert(em::IndexMap<void, unsigned char>::max_size() == 256);
    static_assert(em::IndexMap<void, std::size_t  >::max_size() == std::size_t(-1)); // For `std::size_t`, cap the size at `... - 1`.

    // The type of the mutable `values()` view.
    static_assert(std::is_same_v<em::IndexMap<void>::value_view, void>);
    static_assert(!std::is_same_v<em::IndexMap<A>::value_view, void>);

    // The assignment operator of `key_value_reference` operator is `&`-qualified.
    static_assert(!std::is_assignable_v<em::IndexMap<A>::key_value_reference, em::IndexMap<A>::key_value_reference>);
    static_assert(std::is_assignable_v<em::IndexMap<A>::key_value_reference &, em::IndexMap<A>::key_value_reference>);

    std::vector<int> v; v.push_back(1);

    // Compile-time checks.
    constexpr auto basic_checks = []<typename M>() consteval
    {
        M m;

        // ---

        typename M::key k0 = m.emplace(10).key;
        typename M::key k1 = m.emplace(20).key;
        typename M::key k2 = m.emplace(30).key;
        typename M::key k3 = m.emplace(40).key;

        Check(std::size_t(k0) == 0);
        Check(std::size_t(k1) == 1);
        Check(std::size_t(k2) == 2);
        Check(std::size_t(k3) == 3);

        Check(m.size() == 4);
        Check(m.values().size() == 4);
        Check(m.keys_size() == 4);

        Check(m.values()[0].x == 10);
        Check(m.values()[1].x == 20);
        Check(m.values()[2].x == 30);
        Check(m.values()[3].x == 40);
        Check(m[0].x == 10);
        Check(m[1].x == 20);
        Check(m[2].x == 30);
        Check(m[3].x == 40);

        Check(m[k0].x == 10);
        Check(m[k1].x == 20);
        Check(m[k2].x == 30);
        Check(m[k3].x == 40);

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k1) == 1);
        Check(m.key_to_index(k2) == 2);
        Check(m.key_to_index(k3) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k1);
        Check(m.index_to_key(2) == k2);
        Check(m.index_to_key(3) == k3);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));
        Check(!m.contains(typename M::key(4)) && !m.contains_relaxed(typename M::key(4)));

        // ---

        m.erase(k1);

        Check(m.size() == 3);
        Check(m.values().size() == 3);
        Check(m.keys_size() == 4);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(!m.contains(k1) && m.contains_relaxed(k1));
        Check(m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.values()[0].x == 10);
        Check(m.values()[1].x == 40); // This got swapped from the last position.
        Check(m.values()[2].x == 30);
        Check(m[0].x == 10);
        Check(m[1].x == 40);
        Check(m[2].x == 30);

        Check(m[k0].x == 10);
        Check(m[k2].x == 30);
        Check(m[k3].x == 40);

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index(k2) == 2);
        Check(m.key_to_index_relaxed(k1) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key(2) == k2);
        Check(m.index_to_key_relaxed(3) == k1);

        // ---

        Check(m.emplace(200).key == k1);
        m.erase(k2); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.

        Check(m.size() == 3);
        Check(m.values().size() == 3);
        Check(m.keys_size() == 4);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(!m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.values()[0].x == 10);
        Check(m.values()[1].x == 40); // This got swapped from the last position.
        Check(m.values()[2].x == 200);

        Check(m[k0].x == 10);
        Check(m[k1].x == 200);
        Check(m[k3].x == 40);

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index(k1) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key(2) == k1);
        Check(m.index_to_key_relaxed(3) == k2);

        // ---

        m.erase(k0);

        Check(m.size() == 2);
        Check(m.values().size() == 2);
        Check(m.keys_size() == 4);

        Check(!m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(!m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.values()[0].x == 200);
        Check(m.values()[1].x == 40);

        Check(m[k1].x == 200);
        Check(m[k3].x == 40);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key_relaxed(2) == k0);
        Check(m.index_to_key_relaxed(3) == k2);

        // ---

        typename M::insert_result ins_result = m.insert({.x = 42});
        Check(ins_result.key == k0);
        Check(ins_result.value.x == 42);

        Check(!m.remove_unused_key()); // The last index is still used.
        m.erase(k0);
        Check(!m.remove_unused_key()); // The last index is still used.

        m.erase(k3);

        Check(m.insert({.x = 42}).key == k3);
        m.erase(k3);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k3) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k3);
        Check(m.index_to_key_relaxed(2) == k0);
        Check(m.index_to_key_relaxed(3) == k2);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 3);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k2) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k2);
        Check(m.index_to_key_relaxed(2) == k0);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 2);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k0) == 1);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k0);

        Check(!m.remove_unused_key());

        Check(m.insert({.x = 1000}).key == k0);
        Check(!m.remove_unused_key());
        m.erase(k1);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 1);

        Check(m.key_to_index(k0) == 0);
        Check(m.index_to_key(0) == k0);

        Check(!m.remove_unused_key());

        m.erase(k0);
        Check(m.size() == 0);
        Check(m.keys_size() == 1);

        Check(m.remove_unused_key());
        Check(m.size() == 0);
        Check(m.keys_size() == 0);

        Check(!m.remove_unused_key());
    };
    basic_checks.operator()<em::IndexMap<A>>();

    { // Exception checks.
        em::IndexMap<A> m;

        (void)m.emplace(10);
        (void)m.emplace(20);
        (void)m.emplace(30);
        (void)m.emplace(40);

        m.erase(em::IndexMap<A>::key(1));
        (void)m.insert({200});
        m.erase(em::IndexMap<A>::key(2));

        Check(m.key_to_index(em::IndexMap<A>::key(0)) == 0);
        Check(m.key_to_index(em::IndexMap<A>::key(1)) == 2);
        MUST_THROW("Invalid index map key.", (void)m.key_to_index(em::IndexMap<A>::key(2))); // == 3
        Check(m.key_to_index(em::IndexMap<A>::key(3)) == 1);
        MUST_THROW("Invalid index map key.", (void)m.key_to_index(em::IndexMap<A>::key(4)));
        MUST_THROW("Invalid index map key.", (void)m.key_to_index(em::IndexMap<A>::key((unsigned int)-1)));

        Check(m.key_to_index_relaxed(em::IndexMap<A>::key(0)) == 0);
        Check(m.key_to_index_relaxed(em::IndexMap<A>::key(1)) == 2);
        Check(m.key_to_index_relaxed(em::IndexMap<A>::key(2)) == 3);
        Check(m.key_to_index_relaxed(em::IndexMap<A>::key(3)) == 1);
        MUST_THROW("Invalid index map key.", (void)m.key_to_index_relaxed(em::IndexMap<A>::key(4)));
        MUST_THROW("Invalid index map key.", (void)m.key_to_index_relaxed(em::IndexMap<A>::key((unsigned int)-1)));

        Check(m.index_to_key(0) == em::IndexMap<A>::key(0));
        Check(m.index_to_key(1) == em::IndexMap<A>::key(3));
        Check(m.index_to_key(2) == em::IndexMap<A>::key(1));
        MUST_THROW("Invalid index map index.", (void)m.index_to_key(3)); // == em::IndexMap<A>::key(2)
        MUST_THROW("Invalid index map index.", (void)m.index_to_key(4));
        MUST_THROW("Invalid index map index.", (void)m.index_to_key(std::size_t(-1)));

        Check(m.index_to_key_relaxed(0) == em::IndexMap<A>::key(0));
        Check(m.index_to_key_relaxed(1) == em::IndexMap<A>::key(3));
        Check(m.index_to_key_relaxed(2) == em::IndexMap<A>::key(1));
        Check(m.index_to_key_relaxed(3) == em::IndexMap<A>::key(2));
        MUST_THROW("Invalid index map index.", (void)m.index_to_key_relaxed(4));
        MUST_THROW("Invalid index map index.", (void)m.index_to_key_relaxed(std::size_t(-1)));

        MUST_THROW("Invalid index map index.", (void)m.erase(3));
        MUST_THROW("Invalid index map index.", (void)m.erase(4));
        MUST_THROW("Invalid index map index.", (void)m.erase(std::size_t(-1)));

        MUST_THROW("Invalid index map key.", (void)m.erase(em::IndexMap<A>::key(2)));
        MUST_THROW("Invalid index map key.", (void)m.erase(em::IndexMap<A>::key((unsigned int)-1)));
        MUST_THROW("Invalid index map key.", (void)m.erase(em::IndexMap<A>::key(4)));
    }

    // Check the persistent data feature.
    constexpr auto persistent_data_checks = []<typename M>() consteval
    {
        M m;

        typename M::insert_result i0 = m.emplace(10);
        Check(i0.key == typename M::key(0));
        Check(i0.value.x == 10);
        i0.persistent_data.data = 100;

        typename M::insert_result i1 = m.emplace(20);
        Check(i1.key == typename M::key(1));
        Check(i1.value.x == 20);
        i1.persistent_data.data = 200;

        typename M::insert_result i2 = m.emplace(30);
        Check(i2.key == typename M::key(2));
        Check(i2.value.x == 30);
        i2.persistent_data.data = 300;

        typename M::insert_result i3 = m.emplace(40);
        Check(i3.key == typename M::key(3));
        Check(i3.value.x == 40);
        i3.persistent_data.data = 400;

        m.erase(i1.key);
        Check(m.emplace(21).key == i1.key);
        m.erase(i2.key); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.

        Check(m.get_persistent_data(i0.key).data == 100);
        Check(m.get_persistent_data(i1.key).data == 200);
        Check(m.get_persistent_data(i2.key).data == 300);
        Check(m.get_persistent_data(i3.key).data == 400);
        Check(m.get_persistent_data(0).data == 100);
        Check(m.get_persistent_data(1).data == 400);
        Check(m.get_persistent_data(2).data == 200);
        Check(m[0].x == 10);
        Check(m[1].x == 40);
        Check(m[2].x == 21);

        (void)m.emplace(3000);

        m.erase(1); // Erase by index.

        Check(m.get_persistent_data(i0.key).data == 100);
        Check(m.get_persistent_data(i1.key).data == 200);
        Check(m.get_persistent_data(i2.key).data == 300);
        Check(m.get_persistent_data(i3.key).data == 400);
        Check(m.get_persistent_data(0).data == 100);
        Check(m.get_persistent_data(1).data == 300);
        Check(m.get_persistent_data(2).data == 200);
    };
    persistent_data_checks.operator()<em::IndexMap<A, unsigned int, Data>>();

    // Check `T == void`.
    constexpr auto void_value_checks = []<typename M>() consteval
    {
        M m;

        // ---

        typename M::key k0 = m.emplace().key;
        typename M::key k1 = m.emplace().key;
        typename M::key k2 = m.emplace().key;
        typename M::key k3 = m.emplace().key;

        Check(std::size_t(k0) == 0);
        Check(std::size_t(k1) == 1);
        Check(std::size_t(k2) == 2);
        Check(std::size_t(k3) == 3);

        Check(m.size() == 4);
        Check(m.values().size() == 4);
        Check(m.keys_size() == 4);

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k1) == 1);
        Check(m.key_to_index(k2) == 2);
        Check(m.key_to_index(k3) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k1);
        Check(m.index_to_key(2) == k2);
        Check(m.index_to_key(3) == k3);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));
        Check(!m.contains(typename M::key(4)) && !m.contains_relaxed(typename M::key(4)));

        // ---

        m.erase(k1);

        Check(m.size() == 3);
        Check(m.values().size() == 3);
        Check(m.keys_size() == 4);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(!m.contains(k1) && m.contains_relaxed(k1));
        Check(m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index(k2) == 2);
        Check(m.key_to_index_relaxed(k1) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key(2) == k2);
        Check(m.index_to_key_relaxed(3) == k1);

        // ---

        Check(m.emplace().key == k1);
        m.erase(k2); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.

        Check(m.size() == 3);
        Check(m.values().size() == 3);
        Check(m.keys_size() == 4);

        Check(m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(!m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.key_to_index(k0) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index(k1) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k0);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key(2) == k1);
        Check(m.index_to_key_relaxed(3) == k2);

        // ---

        m.erase(k0);

        Check(m.size() == 2);
        Check(m.values().size() == 2);
        Check(m.keys_size() == 4);

        Check(!m.contains(k0) && m.contains_relaxed(k0));
        Check(m.contains(k1) && m.contains_relaxed(k1));
        Check(!m.contains(k2) && m.contains_relaxed(k2));
        Check(m.contains(k3) && m.contains_relaxed(k3));

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index(k3) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key(1) == k3);
        Check(m.index_to_key_relaxed(2) == k0);
        Check(m.index_to_key_relaxed(3) == k2);

        // ---

        typename M::insert_result ins_result = m.emplace();
        Check(ins_result.key == k0);

        Check(!m.remove_unused_key()); // The last index is still used.
        m.erase(k0);
        Check(!m.remove_unused_key()); // The last index is still used.

        m.erase(k3);

        Check(m.emplace().key == k3);
        m.erase(k3);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k3) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.key_to_index_relaxed(k2) == 3);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k3);
        Check(m.index_to_key_relaxed(2) == k0);
        Check(m.index_to_key_relaxed(3) == k2);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 3);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k2) == 1);
        Check(m.key_to_index_relaxed(k0) == 2);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k2);
        Check(m.index_to_key_relaxed(2) == k0);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 2);

        Check(m.key_to_index(k1) == 0);
        Check(m.key_to_index_relaxed(k0) == 1);
        Check(m.index_to_key(0) == k1);
        Check(m.index_to_key_relaxed(1) == k0);

        Check(!m.remove_unused_key());

        Check(m.emplace().key == k0);
        Check(!m.remove_unused_key());
        m.erase(k1);

        Check(m.remove_unused_key());
        Check(m.size() == 1);
        Check(m.keys_size() == 1);

        Check(m.key_to_index(k0) == 0);
        Check(m.index_to_key(0) == k0);

        Check(!m.remove_unused_key());

        m.erase(k0);
        Check(m.size() == 0);
        Check(m.keys_size() == 1);

        Check(m.remove_unused_key());
        Check(m.size() == 0);
        Check(m.keys_size() == 0);

        Check(!m.remove_unused_key());
    };
    void_value_checks.operator()<em::IndexMap<void>>();

    // Check the `.values()` interface.
    constexpr auto value_range_checks = []<typename M>() consteval
    {
        M m;
        (void)m.emplace(10);
        (void)m.emplace(20);
        (void)m.emplace(30);
        (void)m.emplace(40);

        m.erase(typename M::key(1));
        Check(m.emplace(21).key == typename M::key(1));
        m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
        (void)m.emplace(31);

        // [10,40,21,31]

        int i = 0;

        for (auto it = m.values().begin(); it != m.values().end();)
        {
            if (i == 0)
                Check(it->x == 10);
            else if (i == 1)
            {
                it = m.values().erase(it);
                i++;
                continue;
            }
            else if (i == 2)
                Check(it->x == 31);
            else if (i == 3)
                Check(it->x == 21);

            it++;
            i++;
        }
        Check(i == 4);
    };
    value_range_checks.operator()<em::IndexMap<A>>();

    // Check the `.keys_and_values()` interface.
    constexpr auto key_value_range_basic_checks = []<typename M>() consteval
    {
        constexpr bool has_values = M::has_value_type;
        constexpr bool has_pers_data = M::has_persistent_data_type;

        M m;

        if constexpr (has_values)
        {
            if constexpr (has_pers_data)
            {
                m.emplace(10).persistent_data.data = 100;
                m.emplace(20).persistent_data.data = 200;
                m.emplace(30).persistent_data.data = 300;
                m.emplace(40).persistent_data.data = 400;
            }
            else
            {
                (void)m.emplace(10);
                (void)m.emplace(20);
                (void)m.emplace(30);
                (void)m.emplace(40);
            }

            m.erase(typename M::key(1));
            Check(m.emplace(21).key == typename M::key(1));
            m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
            (void)m.emplace(31);
        }
        else
        {
            if constexpr (has_pers_data)
            {
                m.emplace().persistent_data.data = 100;
                m.emplace().persistent_data.data = 200;
                m.emplace().persistent_data.data = 300;
                m.emplace().persistent_data.data = 400;
            }
            else
            {
                (void)m.emplace();
                (void)m.emplace();
                (void)m.emplace();
                (void)m.emplace();
            }

            m.erase(typename M::key(1));
            Check(m.emplace().key == typename M::key(1));
            m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
            (void)m.emplace();
        }
        m.erase(m.emplace().key); // Make the capacity larger than the size.

        // [0:10, 3:40, 1:21, 2:31]

        int i = 0;

        for (auto it = m.keys_and_values().begin(); it != m.keys_and_values().end();)
        {
            Check(&it->map() == &m);

            if (i == 0)
            {
                Check(it->key() == typename M::key(0));
                Check(it->index() == 0);
                if constexpr (has_values)
                    Check(it->value().x == 10);
                if constexpr (has_pers_data)
                    Check(it->persistent_data().data == 100);
            }
            else if (i == 1)
            {
                Check(it->key() == typename M::key(3));
                Check(it->index() == 1);
                if constexpr (has_values)
                    Check(it->value().x == 40);
                if constexpr (has_pers_data)
                    Check(it->persistent_data().data == 400);

                it = m.keys_and_values().erase(it);
                i++;
                continue;
            }
            else if (i == 2)
            {
                Check(it->key() == typename M::key(2));
                Check(it->index() == 1);
                if constexpr (has_values)
                    Check(it->value().x == 31);
                if constexpr (has_pers_data)
                    Check(it->persistent_data().data == 300);
            }
            else if (i == 3)
            {
                Check(it->key() == typename M::key(1));
                Check(it->index() == 2);
                if constexpr (has_values)
                    Check(it->value().x == 21);
                if constexpr (has_pers_data)
                    Check(it->persistent_data().data == 200);
            }

            it++;
            i++;
        }
        Check(i == 4);
    };
    key_value_range_basic_checks.operator()<em::IndexMap<A   , unsigned int, Data>>();
    key_value_range_basic_checks.operator()<em::IndexMap<void, unsigned int, Data>>();
    key_value_range_basic_checks.operator()<em::IndexMap<A   , unsigned int, void>>();
    key_value_range_basic_checks.operator()<em::IndexMap<void, unsigned int, void>>();

    // Rich iterator interface.
    constexpr auto key_value_range_advanced_checks = []<typename M>() consteval
    {
        M m;

        (void)m.insert({10});
        (void)m.insert({20});
        (void)m.insert({30});
        (void)m.insert({40});

        m.erase(typename M::key(1));
        Check(m.insert({21}).key == typename M::key(1));
        m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
        (void)m.insert({31});

        // [0:10, 3:40, 1:21, 2:31]

        { // Iterator sanity checks.
            auto it = m.keys_and_values().begin();

            // Increment and decrement, prefix and suffix.
            Check(it->value() == std::vector{10});
            Check(it++->value() == std::vector{10});
            Check(it->value() == std::vector{40});
            Check(it--->value() == std::vector{40});
            Check(it->value() == std::vector{10});
            Check((++it)->value() == std::vector{40});
            Check((--it)->value() == std::vector{10});
            it++;

            Check((*it).value() == std::vector{40});

            Check((it - 1)->value() == std::vector{10});
            Check((it + 1)->value() == std::vector{21});
            Check((it + 2)->value() == std::vector{31});
            Check((2 + it)->value() == std::vector{31});

            Check(it + 2 - it == 2);

            Check(it == it);
            Check(it != it + 1);
            Check(it != it - 1);

            Check( (it <  it + 1)); Check(!(it <  it)); Check(!(it + 1 <  it));
            Check( (it <= it + 1)); Check( (it <= it)); Check(!(it + 1 <= it));
            Check(!(it >  it + 1)); Check(!(it >  it)); Check( (it + 1 >  it));
            Check(!(it >= it + 1)); Check( (it >= it)); Check( (it + 1 >= it));
            Check((it <=> it + 1) == std::strong_ordering::less);
            Check((it <=> it) == std::strong_ordering::equal);
            Check((it + 1 <=> it) == std::strong_ordering::greater);

            Check(it[-1].value() == std::vector{10});
            Check(it[1].value() == std::vector{21});
            Check(it[2].value() == std::vector{31});

            Check((it += 2)->value() == std::vector{31});
            Check(it->value() == std::vector{31});
            Check((it -= 2)->value() == std::vector{40});
            Check(it->value() == std::vector{40});
        }


        Check(m[0] == std::vector{10});
        Check(m[1] == std::vector{40});
        Check(m[2] == std::vector{21});
        Check(m[3] == std::vector{31});
        Check(m[typename M::key(0)] == std::vector{10});
        Check(m[typename M::key(1)] == std::vector{21});
        Check(m[typename M::key(2)] == std::vector{31});
        Check(m[typename M::key(3)] == std::vector{40});

        iter_swap(m.keys_and_values().begin(), m.keys_and_values().begin() + 2);
        // [0:21, 3:40, 1:10, 2:31]

        Check(m[0] == std::vector{21});
        Check(m[1] == std::vector{40});
        Check(m[2] == std::vector{10});
        Check(m[3] == std::vector{31});
        Check(m[typename M::key(0)] == std::vector{10});
        Check(m[typename M::key(1)] == std::vector{21});
        Check(m[typename M::key(2)] == std::vector{31});
        Check(m[typename M::key(3)] == std::vector{40});

        m.keys_and_values()[0] = iter_move(m.keys_and_values().begin() + 2);

        Check(m[0] == std::vector{10});
        Check(m[1] == std::vector{40});
        Check(m[2] == std::vector<int>{});
        Check(m[3] == std::vector{31});
        Check(m[typename M::key(0)] == std::vector{10});
        Check(m[typename M::key(1)] == std::vector<int>{});
        Check(m[typename M::key(2)] == std::vector{31});
        Check(m[typename M::key(3)] == std::vector{40});
    };
    key_value_range_advanced_checks.operator()<em::IndexMap<std::vector<int>>>();

    // Non-member erase functions.
    constexpr auto nonmember_erase_checks = []<typename M>() consteval
    {
        // `.values()`:

        { // `erase` all.
            M m;
            (void)m.insert(42);
            (void)m.insert(42);
            (void)m.insert(42);
            Check(em::erase(m.values(), 42) == 3);
            Check(m.empty());
        }

        { // `erase_if` all.
            M m;
            (void)m.insert(42);
            (void)m.insert(43);
            (void)m.insert(44);
            Check(em::erase_if(m.values(), [](int x){return x > 40;}) == 3);
            Check(m.empty());
        }

        { // `erase` some.
            M m;

            (void)m.insert(10);
            (void)m.insert(20);
            (void)m.insert(30);
            (void)m.insert(40);

            m.erase(typename M::key(1));
            Check(m.insert(21).key == typename M::key(1));
            m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
            (void)m.insert(31);

            // [0:10, 3:40, 1:21, 2:31]

            Check(em::erase(m.values(), 21) == 1);

            Check(m.size() == 3);
            Check(m[0] == 10);
            Check(m[1] == 40);
            Check(m[2] == 31);

            Check(m.size() == 3);
            Check(m[typename M::key(0)] == 10);
            Check(m[typename M::key(2)] == 31);
            Check(m[typename M::key(3)] == 40);
        }

        { // `erase_if` some.
            M m;

            (void)m.insert(10);
            (void)m.insert(20);
            (void)m.insert(30);
            (void)m.insert(40);

            m.erase(typename M::key(1));
            Check(m.insert(21).key == typename M::key(1));
            m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
            (void)m.insert(31);

            // [0:10, 3:40, 1:21, 2:31]

            Check(em::erase_if(m.values(), [](int x){return x == 21;}) == 1);

            Check(m.size() == 3);
            Check(m[0] == 10);
            Check(m[1] == 40);
            Check(m[2] == 31);

            Check(m.size() == 3);
            Check(m[typename M::key(0)] == 10);
            Check(m[typename M::key(2)] == 31);
            Check(m[typename M::key(3)] == 40);
        }

        // `.keys_and_values()`:

        { // `erase_if` all.
            M m;
            (void)m.insert(42);
            (void)m.insert(43);
            (void)m.insert(44);
            Check(em::erase_if(m.keys_and_values(), [](const typename M::key_value_const_reference &x){return x.value() > 40;}) == 3);
            Check(m.empty());
        }

        { // `erase_if` some.
            M m;

            (void)m.insert(10);
            (void)m.insert(20);
            (void)m.insert(30);
            (void)m.insert(40);

            m.erase(typename M::key(1));
            Check(m.insert(21).key == typename M::key(1));
            m.erase(typename M::key(2)); // This finally desyncs `dense_to_sparse` and `sparse_to_dense` indices.
            (void)m.insert(31);

            // [0:10, 3:40, 1:21, 2:31]

            Check(em::erase_if(m.keys_and_values(), [](const typename M::key_value_const_reference &x){return x.value() == 21;}) == 1);

            Check(m.size() == 3);
            Check(m[0] == 10);
            Check(m[1] == 40);
            Check(m[2] == 31);

            Check(m.size() == 3);
            Check(m[typename M::key(0)] == 10);
            Check(m[typename M::key(2)] == 31);
            Check(m[typename M::key(3)] == 40);
        }
    };
    nonmember_erase_checks.operator()<em::IndexMap<int>>();

    constexpr auto clear_checks = []<typename M>() consteval
    {
        { // Soft clear.
            M m;
            (void)m.insert(10);
            (void)m.insert(20);

            m.soft_clear();

            Check(m.size() == 0);
            Check(m.keys_size() == 2);
        }

        { // Hard clear.
            M m;
            (void)m.insert(10);
            (void)m.insert(20);

            m.clear();

            Check(m.size() == 0);
            Check(m.keys_size() == 0);
        }
    };
    clear_checks.operator()<em::IndexMap<int>>();
}
