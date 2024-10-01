#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS
#ifdef _MSC_VER
#define DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif

#ifndef DETAIL_EM_INDEXMAP_ASSERT
#include <cassert>
#define DETAIL_EM_INDEXMAP_ASSERT(...) assert(__VA_ARGS__)
#endif

#ifndef DETAIL_EM_INDEXMAP_THROW
#if __cpp_exceptions
#define DETAIL_EM_INDEXMAP_THROW(...) (throw(__VA_ARGS__))
#else
#define DETAIL_EM_INDEXMAP_THROW(...) std::terminate()
#endif
#endif

namespace em
{
    namespace detail::IndexMap
    {
        template <int> struct Empty {};
        template <typename T, int N> struct VoidToEmptyHelper {using type = T;};
        template <typename T, int N> requires std::is_void_v<T> struct VoidToEmptyHelper<T, N> {using type = Empty<N>;};
        // Replaces `void` with an empty struct.
        template <typename T, int N = 0> using VoidToEmpty = typename VoidToEmptyHelper<T, N>::type;

        // `std::is_constructible` extended for `void`.
        template <typename T, typename ...P>
        struct is_constructible : std::is_constructible<T, P...> {};
        template <typename T, typename ...P> requires std::is_void_v<T>
        struct is_constructible<T, P...> : std::bool_constant<sizeof...(P) == 0> {};

        // Explicitly convertable to a boolean.
        template <typename T> concept BoolTestable = requires(const T &t){t ? 0 : 0;};
        // Equality-comparable, in this order.
        template <typename T, typename U> concept EqComparable = requires(T &&t, U &&u){{std::forward<T>(t) == std::forward<U>(u)} -> BoolTestable;};


        // A fake container with a size but no elements.
        class SizeOnlyContainer
        {
            std::size_t counter = 0;

          public:
            constexpr SizeOnlyContainer() {}
            constexpr SizeOnlyContainer(const auto &) {} // Construct from an allocator.

            constexpr SizeOnlyContainer(const SizeOnlyContainer &) = default;
            constexpr SizeOnlyContainer &operator=(const SizeOnlyContainer &) = default;

            // Reset when moved from.
            constexpr SizeOnlyContainer(SizeOnlyContainer &&other) noexcept : counter(other.counter) {other.counter = 0;}
            constexpr SizeOnlyContainer &operator=(SizeOnlyContainer &&other) noexcept {if (this != &other) {counter = other.counter; other.counter = 0;} return *this;}

            [[nodiscard]] constexpr std::size_t size() const noexcept {return std::size_t(counter);}
            [[nodiscard]] constexpr bool empty() const noexcept {return size() == 0;}

            constexpr Empty<0> emplace_back() noexcept {counter++; return {};} // With 0 arguments only!
            constexpr void pop_back() noexcept {counter--;}
            constexpr void clear() noexcept {counter = 0;}

            [[nodiscard]] constexpr std::size_t capacity() const noexcept {return 0;}
            constexpr void reserve(std::size_t) noexcept {}
            constexpr void shrink_to_fit() noexcept {}
        };

        template <typename T, typename KeyType, typename Allocator, template <typename...> typename ValueContainer>
        struct ContainerOrCounter
        {
            using type = ValueContainer<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;
        };
        template <typename T, typename KeyType, typename Allocator, template <typename...> typename ValueContainer>
        requires std::is_void_v<T> // Cv-qualified void? Why not.
        struct ContainerOrCounter<T, KeyType, Allocator, ValueContainer>
        {
            using type = SizeOnlyContainer;
        };


        // Given a `ValueView` or `KeyValueView``, returns its target map.
        template <typename T>
        [[nodiscard]] constexpr auto &UnderlyingMap(T &&target) {return *target.this_map;}

        // `value_view` for an index map. Never const, because instead of the const view we just return a const reference to the storage.
        template <typename IndexMap>
        struct ValueView
        {
            template <typename T>
            friend constexpr auto &UnderlyingMap(T &&target);

            IndexMap *this_map = nullptr;

            static constexpr bool IsContiguous = std::contiguous_iterator<typename IndexMap::value_container::iterator>;

          public:
            [[nodiscard]] ValueView() = default;
            // Mostly for internal use.
            [[nodiscard]] constexpr ValueView(IndexMap &this_map) : this_map(&this_map) {}

            constexpr typename IndexMap::value_container::iterator erase(typename IndexMap::value_container::iterator iter)
            {
                auto n = iter - this_map->value_storage.begin();
                this_map->erase(std::size_t(n));
                // Roundtrip the iterator through index. Otherwise the iterator validation will complain,
                //   or perhaps some custom containers might need this.
                return this_map->value_storage.begin() + n;
            }

            // Standard range members:

            using value_type             = typename IndexMap::value_type;
            using size_type              = std::size_t; // Forcing `std::size_t` for simplicity, like everywhere else.
            using difference_type        = std::ptrdiff_t;
            using reference              = value_type &; // No fancy pointers/refs support for now.
            using const_reference        = const value_type &;
            using iterator               = typename IndexMap::value_container::iterator;
            using const_iterator         = typename IndexMap::value_container::const_iterator;
            using reverse_iterator       = typename IndexMap::value_container::reverse_iterator;
            using const_reverse_iterator = typename IndexMap::value_container::const_reverse_iterator;
            using pointer                = value_type *;
            using const_pointer          = const value_type *;

            [[nodiscard]] constexpr size_type size() const noexcept {return this_map->size();}
            [[nodiscard]] constexpr bool empty() const noexcept {return this_map->empty();}

            [[nodiscard]] constexpr iterator begin() const noexcept {return const_cast<typename IndexMap::value_container &>(std::as_const(*this_map).values()).begin();}
            [[nodiscard]] constexpr iterator end() const noexcept {return const_cast<typename IndexMap::value_container &>(std::as_const(*this_map).values()).end();}
            [[nodiscard]] constexpr const_iterator cbegin() const noexcept {return std::as_const(*this_map).values().cbegin();}
            [[nodiscard]] constexpr const_iterator cend() const noexcept {return std::as_const(*this_map).values().cend();}

            [[nodiscard]] constexpr pointer data() noexcept requires IsContiguous {return std::to_address(begin());}
            [[nodiscard]] constexpr const_pointer data() const noexcept requires IsContiguous {return std::to_address(cbegin());}

            [[nodiscard]] constexpr reference operator[](size_type i) noexcept {return const_cast<typename IndexMap::value_container &>(std::as_const(*this_map).values())[i];}
            [[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept {return std::as_const(*this_map).values()[i];}
        };

        template <typename IndexMap>
        struct SelectValueView
        {
            using type = ValueView<IndexMap>;
        };
        template <typename IndexMap> requires std::is_same_v<typename IndexMap::value_container, SizeOnlyContainer>
        struct SelectValueView<IndexMap>
        {
            using type = void;
        };


        // A reference-like class to an element and its key.
        template <typename IndexMap, bool IsConst>
        class KeyValueRef
        {
            template <typename IndexMap2, bool IsConst2>
            friend class KeyValueIter;

            template <typename U>
            using MaybeConst = std::conditional_t<IsConst, const U, U>;

            MaybeConst<IndexMap> *this_map = nullptr;
            // `-1` makes it so that singular iterators compare not equal with everything else. Which isn't mandatory, but makes things safer.
            std::size_t this_index = std::size_t(-1);

            using Ref = std::conditional_t<IsConst, typename IndexMap::value_const_reference, typename IndexMap::value_reference>;
            using PersistentDataRef = std::conditional_t<IsConst, typename IndexMap::persistent_data_const_reference, typename IndexMap::persistent_data_reference>;

          public:
            using map_type = MaybeConst<IndexMap>;

            [[nodiscard]] KeyValueRef() = default;
            // Primarily for internal use.
            [[nodiscard]] constexpr KeyValueRef(MaybeConst<IndexMap> &this_map, std::size_t this_index) : this_map(&this_map), this_index(this_index) {}

            // Convert non-const to const reference.
            [[nodiscard]] constexpr KeyValueRef(const KeyValueRef<IndexMap, !IsConst> &other) requires IsConst : this_map(&other.map()), this_index(other.index()) {}

            // `&`-qualify the assignment operator, to not imply that you can assign to dereferenced iterators.
            KeyValueRef(const KeyValueRef &) = default;
            KeyValueRef &operator=(const KeyValueRef &) & = default;

            // All of this is noexcept and hard fails if the iterator is invalid: [

            [[nodiscard]] constexpr map_type &map() const noexcept {return *this_map;}
            [[nodiscard]] constexpr std::size_t index() const noexcept {return this_index;}
            [[nodiscard]] constexpr typename IndexMap::key key() const noexcept {return this_map->index_to_key_unsafe(this_index);}
            [[nodiscard]] constexpr Ref value() const noexcept requires map_type::has_value_type {return (*this_map)[this_index];}
            [[nodiscard]] constexpr PersistentDataRef persistent_data() const noexcept requires map_type::has_persistent_data_type {return this_map->get_persistent_data(this_map->index_to_key(this_index));}

            // ]

            // This is what `iter_move` returns.
            struct moved
            {
                KeyValueRef ref;

                // Back conversion, possibly to a const ref. Some standard concepts check this.
                template <bool IsConst2>
                [[nodiscard]] constexpr operator KeyValueRef<IndexMap, IsConst2>() const requires(IsConst <= IsConst2) {return ref;}
            };
            // Since this is reference-like, `this` is const. `other` is const just for consistency.
            constexpr const KeyValueRef &operator=(const moved &&other) const noexcept(map_type::has_value_type <= std::is_nothrow_move_assignable_v<typename IndexMap::value_type>) requires(!IsConst)
            {
                DETAIL_EM_INDEXMAP_ASSERT(this_map == other.ref.this_map);
                this_map->move_elem(other.ref.this_index, this_index);
                return *this;
            }
            // A non-const overload. Needed to resolve some ambiguities.
            constexpr const KeyValueRef &operator=(const moved &&other)       noexcept(map_type::has_value_type <= std::is_nothrow_move_assignable_v<typename IndexMap::value_type>) requires(!IsConst)
            {
                return std::as_const(*this) = std::move(other);
            }

            // Work around silly MSVC bugs. Last tested on 19.41.
            #if !defined(__clang__) && (defined(_MSC_VER) || (defined(__GNUC__) && __GNUC__ <= 12))
            #define DETAIL_EM_INDEXMAP_ACCESS_WORKAROUND
            constexpr auto &__detail_this_map()       {return this_map;}
            constexpr auto &__detail_this_map() const {return this_map;}
            constexpr auto &__detail_this_index()       {return this_index;}
            constexpr auto &__detail_this_index() const {return this_index;}
            #endif
        };

        // An iterator-like class to an element and its key.
        template <typename IndexMap, bool IsConst>
        class KeyValueIter
        {
            KeyValueRef<IndexMap, IsConst> state;

          public:
            using map_type = typename KeyValueRef<IndexMap, IsConst>::map_type;

            [[nodiscard]] constexpr KeyValueIter() {}
            // Primarily for internal use.
            [[nodiscard]] constexpr KeyValueIter(map_type &this_map, std::size_t this_index) : state(this_map, this_index) {}

            // Convert non-const to const iterators.
            [[nodiscard]] constexpr KeyValueIter(const KeyValueIter<IndexMap, !IsConst> &other) requires IsConst : state(other->map(), other->index()) {}

            using value_type = KeyValueRef<IndexMap, IsConst>; // Would use `void` here to
            using reference = KeyValueRef<IndexMap, IsConst>;
            using difference_type = typename IndexMap::difference_type;
            using iterator_category = std::random_access_iterator_tag;

            // This is what `operator->` returns.
            struct pointer
            {
                reference ref;
                [[nodiscard]] constexpr reference *operator->() && noexcept {return &ref;}
            };


            // We dereference to a const reference to make things a bit easier, and a bit more fool-proof.
            [[nodiscard]] constexpr reference operator*() const noexcept {return state;}
            [[nodiscard]] constexpr pointer operator->() const noexcept {return {state};}

            #ifdef DETAIL_EM_INDEXMAP_ACCESS_WORKAROUND
            #define this_map __detail_this_map()
            #define this_index __detail_this_index()
            #endif

            constexpr KeyValueIter &operator++() noexcept
            {
                state.this_index++;
                return *this;
            }
            constexpr KeyValueIter operator++(int) noexcept
            {
                KeyValueIter ret = *this;
                ++*this;
                return ret;
            }

            constexpr KeyValueIter &operator--() noexcept
            {
                state.this_index--;
                return *this;
            }
            constexpr KeyValueIter operator--(int) noexcept
            {
                KeyValueIter ret = *this;
                --*this;
                return ret;
            }

            friend constexpr KeyValueIter &operator+=(KeyValueIter &a, difference_type n) noexcept {a.state.this_index += (std::size_t)n; return a;}
            friend constexpr KeyValueIter &operator-=(KeyValueIter &a, difference_type n) noexcept {a.state.this_index -= (std::size_t)n; return a;}

            [[nodiscard]] friend constexpr KeyValueIter operator+(const KeyValueIter &a, difference_type n) noexcept {KeyValueIter ret = a; ret += n; return ret;}
            [[nodiscard]] friend constexpr KeyValueIter operator+(difference_type n, const KeyValueIter &a) noexcept {KeyValueIter ret = a; ret += n; return ret;}
            [[nodiscard]] friend constexpr KeyValueIter operator-(const KeyValueIter &a, difference_type n) noexcept {KeyValueIter ret = a; ret -= n; return ret;}
            [[nodiscard]] friend constexpr difference_type operator-(const KeyValueIter &b, const KeyValueIter &a) noexcept {return std::ptrdiff_t(b.state.this_index) - std::ptrdiff_t(a.state.this_index);}

            [[nodiscard]] constexpr reference operator[](difference_type n) const noexcept {return *(*this + n);}

            // Only compare the indices for speed. This is allowed because "The domain of == for forward iterators is that of iterators over the same underlying sequence." (https://en.cppreference.com/w/cpp/named_req/ForwardIterator)
            [[nodiscard]] friend constexpr bool operator==(const KeyValueIter &a, const KeyValueIter &b) noexcept {return a.state.this_index == b.state.this_index;}
            [[nodiscard]] friend constexpr std::strong_ordering operator<=>(const KeyValueIter &a, const KeyValueIter &b) noexcept {return a.state.this_index <=> b.state.this_index;}


            // This is a customization point accepted by `std::ranges::iter_move()`.
            [[nodiscard]] friend constexpr typename reference::moved iter_move(const KeyValueIter &iter) noexcept requires(!IsConst)
            {
                return typename reference::moved(iter.state);
            }

            // This is a customization point accepted by `std::ranges::iter_swap()`.
            friend constexpr void iter_swap(const KeyValueIter &a, const KeyValueIter &b) noexcept(map_type::has_value_type <= std::is_nothrow_swappable_v<typename IndexMap::value_type>) requires(!IsConst)
            {
                // Work around silly MSVC bugs. Last tested on 19.41.
                #if defined(_MSC_VER) && !defined(__clang__)
                #define DETAIL_EM_INDEXMAP_MSVC_CONSTNESS_WORKAROUND
                if constexpr (!IsConst) {
                #endif
                DETAIL_EM_INDEXMAP_ASSERT(a->this_map == b->this_map);
                a->this_map->swap_elems(a->this_index, b->this_index);
                #ifdef DETAIL_EM_INDEXMAP_MSVC_CONSTNESS_WORKAROUND
                }
                #endif
            }

            #ifdef DETAIL_EM_INDEXMAP_ACCESS_WORKAROUND
            #undef this_map
            #undef this_index
            #endif
        };

        template <typename IndexMap, bool IsConst>
        class KeyValueView
        {
            template <typename T>
            friend constexpr auto &UnderlyingMap(T &&target);

            typename KeyValueIter<IndexMap, IsConst>::map_type *this_map = nullptr;

          public:
            [[nodiscard]] constexpr KeyValueView() {}
            // Primarily for internal use.
            [[nodiscard]] constexpr KeyValueView(typename KeyValueIter<IndexMap, IsConst>::map_type &map) : this_map(&map) {}

            [[nodiscard]] constexpr KeyValueIter<IndexMap, IsConst> erase(KeyValueIter<IndexMap, IsConst> iter) noexcept requires (!IsConst)
            {
                DETAIL_EM_INDEXMAP_ASSERT(&this->begin()->map() == &iter->map());
                iter->map().erase(iter->index());
                return iter;
            }

            // Standard range members:

            using value_type             = KeyValueRef<IndexMap, IsConst>;
            using size_type              = std::size_t; // Forcing `std::size_t` for simplicity, like everywhere else.
            using difference_type        = std::ptrdiff_t;
            using reference              = KeyValueRef<IndexMap, IsConst>;
            using const_reference        = KeyValueRef<IndexMap, true>;
            using iterator               = KeyValueIter<IndexMap, IsConst>;
            using const_iterator         = KeyValueIter<IndexMap, true>;
            using reverse_iterator       = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;
            using pointer                = std::conditional_t<IsConst, const value_type *, value_type *>;
            using const_pointer          = const value_type *;

            [[nodiscard]] constexpr size_type size() const noexcept {return this_map->size();}
            [[nodiscard]] constexpr bool empty() const noexcept {return this_map->empty();}

            [[nodiscard]] constexpr iterator begin() const noexcept {return iterator(*this_map, 0);}
            [[nodiscard]] constexpr iterator end() const noexcept {return iterator(*this_map, size());}
            [[nodiscard]] constexpr const_iterator cbegin() const noexcept {return const_iterator(*this_map, 0);}
            [[nodiscard]] constexpr const_iterator cend() const noexcept {return const_iterator(*this_map, size());}

            [[nodiscard]] constexpr reference operator[](size_type i) noexcept {return begin()[std::ptrdiff_t(i)];}
            [[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept {return cbegin()[std::ptrdiff_t(i)];}
        };
    }

    template <
        // The element type or `void`.
        typename T,
        // The type used for keys, or none.
        std::unsigned_integral KeyType = unsigned int,
        // This is stored persistently, even after a key is erased. Good for storing the generation counter.
        // Also you can store your data here instead of in `T`, to have stable addresses.
        typename PersistentData = void,
        // The allocator. We rebind it to the correct type internally.
        typename Allocator = std::allocator<KeyType>,
        // Indices are stored in this.
        template <typename...> typename IndexContainer = std::vector,
        // Values are stored in this.
        template <typename...> typename ValueContainer = std::vector
    >
    requires (sizeof(KeyType) <= sizeof(std::size_t)) // For simplicity.
    class IndexMap
    {
      public:
        enum class key : KeyType {};

        static constexpr bool has_value_type = !std::is_void_v<T>;
        using value_type = T;
        using value_reference       = detail::IndexMap::VoidToEmpty<std::add_lvalue_reference_t<      T>, 0>;
        using value_const_reference = detail::IndexMap::VoidToEmpty<std::add_lvalue_reference_t<const T>, 0>;

        static constexpr bool has_persistent_data_type = !std::is_void_v<PersistentData>;
        using persistent_data_type = PersistentData;
        using persistent_data_reference       = detail::IndexMap::VoidToEmpty<std::add_lvalue_reference_t<      PersistentData>, 1>;
        using persistent_data_const_reference = detail::IndexMap::VoidToEmpty<std::add_lvalue_reference_t<const PersistentData>, 1>;

        // Could use `KeyType` here, but this is way simpler, and also allows us to use
        //   the full range of keys (without reserving one for the max size, as long as `sizeof(KeyType) < sizeof(std::size_t)`).
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        // Usually `std::vector<T>`, or a placeholder if `T == void`.
        using value_container = typename detail::IndexMap::ContainerOrCounter<T, KeyType, Allocator, ValueContainer>::type;

        struct insert_result
        {
            IndexMap::key key{};
            DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS value_reference value;
            DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS persistent_data_reference persistent_data;
        };

      private:
        struct IndexEntry
        {
            // Those have the same value initially, but quickly become desynchronized.
            KeyType sparse_to_dense{};
            KeyType dense_to_sparse{};

            DETAIL_EM_INDEXMAP_NO_UNIQUE_ADDRESS detail::IndexMap::VoidToEmpty<PersistentData, 1> sparse_data{};
        };
        using index_container = IndexContainer<IndexEntry, typename std::allocator_traits<Allocator>::template rebind_alloc<IndexEntry>>;

        index_container indices;
        value_container value_storage;

        struct KeyAndIndex
        {
            key k{};
            std::size_t i{};
            constexpr KeyAndIndex(const IndexMap &self, key k) : k(k), i(self.key_to_index_unsafe(k)) {}
            constexpr KeyAndIndex(const IndexMap &self, std::size_t i) : k(self.index_to_key_unsafe(i)), i(i) {}
        };

        constexpr void swap_indices_only(KeyAndIndex a, KeyAndIndex b)
        {
            DETAIL_EM_INDEXMAP_ASSERT(valid_index(a.i) && valid_index(b.i));
            swap_indices_only_relaxed(a, b);
        }
        constexpr void swap_indices_only_relaxed(KeyAndIndex a, KeyAndIndex b)
        {
            DETAIL_EM_INDEXMAP_ASSERT(valid_index_relaxed(a.i) && valid_index_relaxed(b.i));
            std::swap(indices[std::size_t(a.k)].sparse_to_dense, indices[std::size_t(b.k)].sparse_to_dense);
            std::swap(indices[a.i].dense_to_sparse, indices[b.i].dense_to_sparse);
        }

        constexpr void swap_elems_low(KeyAndIndex a, KeyAndIndex b) {swap_indices_only(a, b); if constexpr (has_value_type) std::ranges::swap((*this)[a.i], (*this)[b.i]);}
        constexpr void move_elem_low(KeyAndIndex a, KeyAndIndex b) {swap_indices_only(a, b); if constexpr (has_value_type) (*this)[b.i] = std::move((*this)[a.i]);}

        constexpr void can_increase_size_or_throw()
        {
            if (size() >= max_size())
                DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Index map is too large."));
        }

        [[nodiscard]] constexpr insert_result add_key_for_inserted_value(value_reference value)
        {
            if (size() <=/*sic*/ indices.size()) // Since we already inserted at this point, we're using `<=` here.
            {
                IndexEntry &e = indices[value_storage.size() - 1];
                return {key(e.dense_to_sparse), value, e.sparse_data};
            }
            else
            {
                struct Guard
                {
                    IndexMap *self;
                    constexpr ~Guard()
                    {
                        if (self)
                            self->value_storage.pop_back();
                    }
                };
                Guard guard{this}; // This destroys the last value if adding a key throws.

                KeyType k = KeyType(indices.size());
                IndexEntry &e = indices.emplace_back();
                e.sparse_to_dense = k;
                e.dense_to_sparse = k;

                guard.self = nullptr;
                return {key(k), value, e.sparse_data};
            }
        }

        [[nodiscard]] constexpr insert_result force_key_for_inserted_value(key k, value_reference value)
        {
            contains_relaxed_or_throw(k);
            if (contains(k))
                DETAIL_EM_INDEXMAP_THROW(std::out_of_range("This index map key is already in use."));
            swap_indices_only_relaxed({*this, value_storage.size()}, {*this, k});
            return {k, value, indices[std::size_t(k)].sparse_data};
        }

      public:
        [[nodiscard]] IndexMap() = default;
        [[nodiscard]] constexpr IndexMap(const Allocator &alloc) : indices(alloc), value_storage(alloc) {}

        // How many values are currently inserted.
        [[nodiscard]] constexpr std::size_t size() const noexcept {return value_storage.size();}
        // Even if this returns true, the map might still be holding some
        [[nodiscard]] constexpr bool empty() const noexcept {return value_storage.empty();}

        // `numeric_limits<KeyType>::max() + 1` (no +1 if `sizeof(KeyType)` == `sizeof(std::size_t)`).
        [[nodiscard]] static constexpr std::size_t max_size() noexcept {return std::size_t(std::numeric_limits<KeyType>::max()) + (sizeof(KeyType) < sizeof(std::size_t));}


        // Element tests:
        //   Here `relaxed` means including keys that used to be valid, got erased, but still have their data lingering behind.

        [[nodiscard]] constexpr bool contains           (key         k) const noexcept {return contains_relaxed(k) && valid_index(key_to_index_relaxed(k));}
        [[nodiscard]] constexpr bool contains_relaxed   (key         k) const noexcept {return std::size_t(k) < indices.size();}
        [[nodiscard]] constexpr bool valid_index        (std::size_t i) const noexcept {return i < size();}
        [[nodiscard]] constexpr bool valid_index_relaxed(std::size_t i) const noexcept {return i < indices.size();}

        constexpr void contains_or_throw           (key         k) const {if (!contains           (k)) DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Invalid index map key."));}
        constexpr void contains_relaxed_or_throw   (key         k) const {if (!contains_relaxed   (k)) DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Invalid index map key."));}
        constexpr void valid_index_or_throw        (std::size_t i) const {if (!valid_index        (i)) DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Invalid index map index."));}
        constexpr void valid_index_relaxed_or_throw(std::size_t i) const {if (!valid_index_relaxed(i)) DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Invalid index map index."));}

        // Mapping between keys and indices:
        //   Here `relaxed` means including keys that used to be valid, got erased, but still have their data lingering behind.
        //   And `unsafe` means non-throwing versions that only have assertions.

        [[nodiscard]] constexpr std::size_t key_to_index        (key         k) const          {contains_or_throw           (k); return key_to_index_unsafe(k);}
        [[nodiscard]] constexpr std::size_t key_to_index_relaxed(key         k) const          {contains_relaxed_or_throw   (k); return key_to_index_unsafe(k);}
        [[nodiscard]] constexpr std::size_t key_to_index_unsafe (key         k) const noexcept {DETAIL_EM_INDEXMAP_ASSERT(contains_relaxed(k)); return std::size_t(indices[std::size_t(k)].sparse_to_dense);}
        [[nodiscard]] constexpr key         index_to_key        (std::size_t i) const          {valid_index_or_throw        (i); return index_to_key_unsafe(i);}
        [[nodiscard]] constexpr key         index_to_key_relaxed(std::size_t i) const          {valid_index_relaxed_or_throw(i); return index_to_key_unsafe(i);}
        [[nodiscard]] constexpr key         index_to_key_unsafe (std::size_t i) const noexcept {DETAIL_EM_INDEXMAP_ASSERT(valid_index_relaxed(i)); return key(indices[i].dense_to_sparse);}


        // Element access:

        // By key.
        [[nodiscard]] constexpr value_reference       operator[](key k)       requires has_value_type {return value_storage[key_to_index(k)];}
        [[nodiscard]] constexpr value_const_reference operator[](key k) const requires has_value_type {return value_storage[key_to_index(k)];}

        // By index.
        [[nodiscard]] constexpr value_reference       operator[](std::size_t i)       requires has_value_type {valid_index_or_throw(i); return value_storage[i];}
        [[nodiscard]] constexpr value_const_reference operator[](std::size_t i) const requires has_value_type {valid_index_or_throw(i); return value_storage[i];}


        // Insertion:

        [[nodiscard]] constexpr insert_result emplace(auto &&... params                             ) requires detail::IndexMap::is_constructible<T, decltype(params)...>::value {can_increase_size_or_throw(); return add_key_for_inserted_value(value_storage.emplace_back(decltype(params)(params)...));}
        [[nodiscard]] constexpr insert_result insert (const detail::IndexMap::VoidToEmpty<T>  &value) requires std::is_copy_constructible_v<T>                                   {can_increase_size_or_throw(); return add_key_for_inserted_value(value_storage.emplace_back(          value            ));}
        [[nodiscard]] constexpr insert_result insert (      detail::IndexMap::VoidToEmpty<T> &&value) requires std::is_move_constructible_v<T>                                   {can_increase_size_or_throw(); return add_key_for_inserted_value(value_storage.emplace_back(std::move(value)           ));}

        // This forces a specific key for the new element. Throws if the key is already is use.
        // Throws if `prepare_keys_for_insertion` wasn't called with at least `std::size_t(k) + 1` before.
        [[nodiscard]] constexpr insert_result emplace_at(key k, auto &&... params                             ) requires detail::IndexMap::is_constructible<T, decltype(params)...>::value {return force_key_for_inserted_value(k, value_storage.emplace_back(decltype(params)(params)...));}
                      constexpr insert_result insert_at (key k, const detail::IndexMap::VoidToEmpty<T>  &value) requires std::is_copy_constructible_v<T>                                   {return force_key_for_inserted_value(k, value_storage.emplace_back(          value            ));}
                      constexpr insert_result insert_at (key k,       detail::IndexMap::VoidToEmpty<T> &&value) requires std::is_move_constructible_v<T>                                   {return force_key_for_inserted_value(k, value_storage.emplace_back(std::move(value)           ));}

        // Increase `keys_size()` to `n`.
        // Can help with performance or if you're preparing to `insert_at()`/`emplace_at()`.
        constexpr void prepare_keys_for_insertion(std::size_t n)
        {
            std::size_t i = indices.size();
            if (i >= n)
                return;
            if (n > max_size())
                DETAIL_EM_INDEXMAP_THROW(std::out_of_range("Index map would be too large."));
            indices.resize(n);
            for (; i < n; i++)
            {
                IndexEntry &e = indices[i];
                e.sparse_to_dense = KeyType(i);
                e.dense_to_sparse = KeyType(i);
            }
        }


        // Erasure:

        // Erase by key. Throws if the key is invalid.
        constexpr void erase(key k)
        {
            contains_or_throw(k);
            move_elem_low({*this, value_storage.size() - 1}, {*this, k});
            value_storage.pop_back();
        }
        // Erase by index. Throws if the index is invalid.
        constexpr void erase(std::size_t i)
        {
            valid_index_or_throw(i);
            move_elem_low({*this, value_storage.size() - 1}, {*this, i});
            value_storage.pop_back();
        }

        // Reduces `keys_size()` by one if possible and returns true. Returns false if not possible.
        // This is the opposite of `prepare_keys_for_insertion()`.
        // This by itself doesn't free any memory, but you can then call `keys_shrink_to_fit()` to free it.
        // You can call this as `while (remove_unused_key()) {}` after every erasure to always remove all unused keys. Or less iterations, or whenever you want.
        constexpr bool remove_unused_key() noexcept
        {
            if (indices.empty())
                return false;
            std::size_t i = std::size_t(indices.back().sparse_to_dense);
            if (i < size())
                return false;
            swap_indices_only_relaxed({*this, i}, {*this, indices.size() - 1});
            indices.pop_back();
            return true;
        }

        // Clear everything, including persistent data. But keep allocated memory.
        constexpr void clear() noexcept {value_storage.clear(); indices.clear();}
        // Clear values, but keep the keys and persistent data (i.e. `keys_size()` is preserved).
        constexpr void soft_clear() noexcept {value_storage.clear();}


        // Persistent data:

        // Returns the persistent data by key. The key must pass `contains_relaxed(k)`, in other words be less than `keys_size()`.
        [[nodiscard]] constexpr persistent_data_reference       get_persistent_data(key k)       requires has_persistent_data_type {contains_relaxed_or_throw(k); return indices[std::size_t(k)].sparse_data;}
        [[nodiscard]] constexpr persistent_data_const_reference get_persistent_data(key k) const requires has_persistent_data_type {contains_relaxed_or_throw(k); return indices[std::size_t(k)].sparse_data;}
        // Returns the persistent data by index. The index must pass `valid_index(i)`. You can't access data of freed keys using this, only by key.
        [[nodiscard]] constexpr persistent_data_reference       get_persistent_data(std::size_t i)       requires has_persistent_data_type {return get_persistent_data(index_to_key(i));}
        [[nodiscard]] constexpr persistent_data_const_reference get_persistent_data(std::size_t i) const requires has_persistent_data_type {return get_persistent_data(index_to_key(i));}


        // Memory management:

        [[nodiscard]] constexpr std::size_t keys_size() const noexcept {return indices.size();}
        [[nodiscard]] constexpr std::size_t keys_capacity() const noexcept {return indices.capacity();}
        constexpr void keys_reserve(std::size_t n) {DETAIL_EM_INDEXMAP_ASSERT(n <= max_size()); indices.reserve(n);}
        constexpr void keys_shrink_to_fit() noexcept {indices.shrink_to_fit();}

        // There's no `values_size()` because that's just `size()`.
        [[nodiscard]] constexpr std::size_t values_capacity() const noexcept {return value_storage.capacity();}
        constexpr void values_reserve(std::size_t n) {DETAIL_EM_INDEXMAP_ASSERT(n <= max_size()); value_storage.reserve(n);}
        constexpr void values_shrink_to_fit() noexcept {value_storage.shrink_to_fit();}


        // Changing element indices:

        // Swap the indices of two elements. Like `std::swap(m[i], m[j])`, but also swaps their keys.
        constexpr void swap_elems(std::size_t i, std::size_t j) noexcept(has_value_type <= std::is_nothrow_swappable_v<T>) {swap_elems_low({*this, i}, {*this, j});}
        // Move the element at index `from_i` to `to_i`. Like `m[to_i] = std::move(m[from_i])`, but also swaps their keys.
        // Remember that classes typically don't support self-move-assignment, and we don't work around that in any way,
        //   so moving to the same index can break the element value (put it into valid but unspecified state).
        constexpr void move_elem(std::size_t from_i, std::size_t to_i) noexcept(has_value_type <= std::is_nothrow_move_assignable_v<T>) {move_elem_low({*this, from_i}, {*this, to_i});}


        // Range of values:

        friend detail::IndexMap::ValueView<IndexMap>;
        using value_view = typename detail::IndexMap::SelectValueView<IndexMap>::type;

        // Access the container directly.
        [[nodiscard]] constexpr const value_container &values() const noexcept {return value_storage;}
        // Access the container directly.
        [[nodiscard]] constexpr value_view values() noexcept requires has_value_type {return *this;}


        // Range of keys with values:

        // Stores a pointer to a map and an index, and lets you access all information about it.
        using key_value_reference = detail::IndexMap::KeyValueRef<IndexMap, false>;
        using key_value_const_reference = detail::IndexMap::KeyValueRef<IndexMap, true>;
        // A random-access iterator that dereferences to `key_value_reference`.
        using key_value_iterator = detail::IndexMap::KeyValueIter<IndexMap, false>;
        using key_value_const_iterator = detail::IndexMap::KeyValueIter<IndexMap, true>;
        // A range of those iterators.
        using key_value_view = detail::IndexMap::KeyValueView<IndexMap, false>;
        using key_value_const_view = detail::IndexMap::KeyValueView<IndexMap, true>;

        // A random-access range, where each element has following methods:
        //     `.index()`, `.key()`, `.value()`, `.persistent_data()`, and also `.map()` that returns a reference to the target map.
        [[nodiscard]] constexpr key_value_view       keys_and_values()       noexcept {return *this;}
        [[nodiscard]] constexpr key_value_const_view keys_and_values() const noexcept {return *this;}
    };

    // Erasing elements.

    // `erase_if(m.values(), lambda)`
    template <typename F, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::BoolTestable<std::invoke_result_t<F &, const std::add_lvalue_reference_t<T>>>
    constexpr std::size_t erase_if(detail::IndexMap::ValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>> &values, F &&func)
    {
        std::size_t ret = 0;
        std::size_t n = values.size();
        auto &map = detail::IndexMap::UnderlyingMap(values);
        while (true)
        {
            if (n == 0)
                return ret;
            n--;
            if (std::invoke(func, std::as_const(map[n])))
            {
                map.erase(n);
                ret++;
            }
        }
    }
    template <typename F, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::BoolTestable<std::invoke_result_t<F &, const std::add_lvalue_reference_t<T>>>
    constexpr std::size_t erase_if(detail::IndexMap::ValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>> &&values, F &&func)
    {
        return (erase_if)(values, func);
    }

    // `erase(m.values(), value)`
    template <typename Elem, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::EqComparable<const T &, Elem &&>
    constexpr std::size_t erase(detail::IndexMap::ValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>> &values, Elem &&value)
    {
        return (erase_if)(values, [&](const T &elem){return elem == value;});
    }
    template <typename Elem, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::EqComparable<const T &, Elem &&>
    constexpr std::size_t erase(detail::IndexMap::ValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>> &&values, Elem &&value)
    {
        return (erase_if)(values, [&](const T &elem){return elem == value;});
    }

    // `erase_if(m.keys_and_values(), lambda)`
    template <typename F, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::BoolTestable<std::invoke_result_t<F &, typename detail::IndexMap::KeyValueRef<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>, true>>>
    constexpr std::size_t erase_if(detail::IndexMap::KeyValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>, false> &keys_and_values, F &&func)
    {
        auto &map = detail::IndexMap::UnderlyingMap(keys_and_values);
        std::size_t ret = 0;
        std::size_t n = map.size();
        while (true)
        {
            if (n == 0)
                return ret;
            n--;
            if (std::invoke(func, std::as_const(keys_and_values)[n]))
            {
                map.erase(n);
                ret++;
            }
        }
    }
    template <typename F, typename T, std::unsigned_integral KeyType, typename PersistentData, typename Allocator, template <typename...> typename IndexContainer, template <typename...> typename ValueContainer>
    requires (!std::is_void_v<T>) && detail::IndexMap::BoolTestable<std::invoke_result_t<F &, typename detail::IndexMap::KeyValueRef<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>, true>>>
    constexpr std::size_t erase_if(detail::IndexMap::KeyValueView<IndexMap<T, KeyType, PersistentData, Allocator, IndexContainer, ValueContainer>, false> &&keys_and_values, F &&func)
    {
        return (erase_if)(keys_and_values, func);
    }
}
