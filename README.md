# IndexMap data structure

## What is this?

This is an extremely simple and fast unordered map/set implementation, that only supports integer keys and can automatically select unused keys on insertion.

Insertion is `O(1)` if there's no memory reallocation. Erasure is `O(1)`. Element access is `O(1)`. Elements are stored contiguously, but the order is not preserved on erasure.

This is effectively just a [sparse set](https://dl.acm.org/doi/pdf/10.1145/176454.176484) with the element vector added. Generation counters can optionally be added, making this equivalent to a [(dense) slot map](https://docs.rs/slotmap/latest/slotmap/).

Memory usage is `size() * sizeof(T)` for values, plus `2 * sizeof(Key) * (max_key + 1)` for the key↔index mapping.

## A minimal example <kbd>[run on gcc.godbolt.org][1]</kbd>

```cpp
#include <em/index_map.h>
#include <print>
#include <string>

int main()
{
    em::IndexMap<std::string> m;

    // Not using integers directly as keys to add some type checking.
    em::IndexMap<std::string>::key k0 = m.insert("foo").key;
    em::IndexMap<std::string>::key k1 = m.insert("bar").key;
    em::IndexMap<std::string>::key k2 = m.insert("baz").key;

    // Accessing the vector of values:
    std::print("{}\n", m.values()); // ["foo", "bar", "baz"]
    // Accessing values by key:
    std::print("[{}, {}, {}]\n", m[k0], m[k1], m[k2]); // [foo, bar, baz]

    // Keys can be cast to and from integers. This prints `[0, 1, 2]`.
    std::print("[{}, {}, {}]\n", std::to_underlying(k0), std::to_underlying(k1), std::to_underlying(k2));

    m.erase(k0);
    std::print("{}\n", m.values()); // ["baz", "bar"]
    // m[k0] // This would throw.

    // Free keys are reused:
    auto k3 = m.insert("FOO").key; // k0 == k3
    std::print("{}\n", std::to_underlying(k3)); // 0
    std::print("{}\n", m.values()); // ["baz", "bar", "FOO"]
}
```

## How it works?

The underlying structure is often called a [sparse set](https://dl.acm.org/doi/pdf/10.1145/176454.176484). There are two integer arrays (interleaved in this case), one mapping keys to the element indices, and another doing the reverse.

This makes it possible to map between keys and values in constant time, with the downside of having to keep the index array as large as the largest key currently used in the map.

## Installation

Header-only. Must use Clang 16+, GCC 10+, or MSVC 19.30+. <!-- https://gcc.godbolt.org/z/oEjvsWhd8 -->

## More details:

### Generation counters

You can associate a 'persistent data' object with each key, that survives when the key is reused. You can use it to store the generation counters (increment a counter on insertion or erasure; store it alongside the keys to detect key reuse).

Example: <kbd>[run on gcc.godbolt.org][2]</kbd>
```cpp
em::IndexMap<
    std::string, // Value
    unsigned int, // Key
    unsigned int // Persistent data
> m;

auto key = m.insert("blah").key;
std::print("{}\n", m[key]); // blah
auto generation = m.get_persistent_data(key);
std::print("{}\n", generation); // 0

// Erase and increment the counter (in any order).
m.get_persistent_data(key)++;
m.erase(key);

auto key2 = m.insert("foo").key;
std::print("{}\n", key2 == key); // true, the key got reused.
std::print("{}\n", m.get_persistent_data(key)); // 1 even after the key reuse
std::print("{}\n", m.get_persistent_data(key) == generation); // false, the generation number is different
```

### Pieces of syntax:

* The first template parameter can be `void` to not store any elements.
* `.insert()` and `.emplace()` return a struct: `struct em::IndexMap<...>::insert_result { key key; T &value; U &persistent_data; };`. Make sure the references don't dangle!
* Check for key: `m.contains(k)`
* Iterate over the elements:

  * Over the values:<br/>
    `for (auto &elem : m.values())`

  * By index:<br/>
    `for (std::size_t i = 0; i < m.size(); i++)`<br/>
    `m[i]` is the value, `m.index_to_key(i)` is the key, `m.get_persistent_data(i)` is the persistent data.

  * Over the keys and values:<br/>
    `for (auto elem : m.keys_and_values())`<br/>
    `elem.value()` is the value, `elem.key()` is the key, `elem.persistent_data()` is the persistent data.

* Mass-erase elements:
  * `em::erase(m.values(), x);` — erase all values equal to `x`
  * `em::erase_if(m.values(), [](const T &x){return x == 42;});` — erase all values for which a lambda returns true.
  * `em::erase_if(m.keys_and_values(), [](const auto &x){return x.value() == 42;});` — same, but the lamdba can access keys (`x.key()`), values (`x.value()`), persistent data (`x.persistent_data()`).

    The lambda parameter type is `const em::IndexMap<...>::key_value_const_reference &`.

* (See the header for more.)




[1]: https://gcc.godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:61,endLineNumber:29,positionColumn:61,positionLineNumber:29,selectionStartColumn:61,selectionStartLineNumber:29,startColumn:61,startLineNumber:29),source:'%23include+%3Chttps://raw.githubusercontent.com/HolyBlackCat/index_map/refs/heads/master/include/em/index_map.h%3E%0A%23include+%3Cprint%3E%0A%23include+%3Cstring%3E%0A%0Aint+main()%0A%7B%0A++++em::IndexMap%3Cstd::string%3E+m%3B%0A%0A++++//+Not+using+integers+directly+as+keys+to+add+some+type+checking.%0A++++em::IndexMap%3Cstd::string%3E::key+k0+%3D+m.insert(%22foo%22).key%3B%0A++++em::IndexMap%3Cstd::string%3E::key+k1+%3D+m.insert(%22bar%22).key%3B%0A++++em::IndexMap%3Cstd::string%3E::key+k2+%3D+m.insert(%22baz%22).key%3B%0A%0A++++//+Accessing+the+vector+of+values:%0A++++std::print(%22%7B%7D%5Cn%22,+m.values())%3B+//+%5B%22foo%22,+%22bar%22,+%22baz%22%5D%0A++++//+Accessing+values+by+key:%0A++++std::print(%22%5B%7B%7D,+%7B%7D,+%7B%7D%5D%5Cn%22,+m%5Bk0%5D,+m%5Bk1%5D,+m%5Bk2%5D)%3B+//+%5Bfoo,+bar,+baz%5D%0A%0A++++//+Keys+can+be+cast+to+and+from+integers.+This+prints+%60%5B0,+1,+2%5D%60.%0A++++std::print(%22%5B%7B%7D,+%7B%7D,+%7B%7D%5D%5Cn%22,+std::to_underlying(k0),+std::to_underlying(k1),+std::to_underlying(k2))%3B%0A++++%0A++++m.erase(k0)%3B%0A++++std::print(%22%7B%7D%5Cn%22,+m.values())%3B+//+%5B%22baz%22,+%22bar%22%5D%0A++++//+m%5Bk0%5D+//+This+would+throw.%0A%0A++++//+Free+keys+are+reused:%0A++++auto+k3+%3D+m.insert(%22FOO%22).key%3B+//+k0+%3D%3D+k3%0A++++std::print(%22%7B%7D%5Cn%22,+std::to_underlying(k3))%3B+//+0%0A++++std::print(%22%7B%7D%5Cn%22,+m.values())%3B+//+%5B%22baz%22,+%22bar%22,+%22FOO%22%5D%0A%7D'),l:'5',n:'1',o:'C%2B%2B+source+%231',t:'0')),k:53.19126676326873,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((g:!((h:compiler,i:(compiler:clang1910,filters:(b:'0',binary:'1',binaryObject:'1',commentOnly:'0',debugCalls:'1',demangle:'0',directives:'0',execute:'0',intel:'0',libraryCode:'1',trim:'1',verboseDemangling:'0'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:2,lang:c%2B%2B,libs:!(),options:'-stdlib%3Dlibc%2B%2B+-std%3Dc%2B%2B26+-Wall+-Wextra+-pedantic-errors+-D_GLIBCXX_DEBUG+-fsanitize%3Daddress+-fsanitize%3Dundefined',overrides:!(),selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1),l:'5',n:'0',o:'+x86-64+clang+19.1.0+(Editor+%231)',t:'0')),header:(),k:46.80873323673128,l:'4',m:50,n:'0',o:'',s:0,t:'0'),(g:!((h:output,i:(compilerName:'x86-64+clang+19.1.0',editorid:1,fontScale:14,fontUsePx:'0',j:2,wrap:'1'),l:'5',n:'0',o:'Output+of+x86-64+clang+19.1.0+(Compiler+%232)',t:'0')),l:'4',m:50,n:'0',o:'',s:0,t:'0')),k:46.80873323673128,l:'3',n:'0',o:'',t:'0')),l:'2',n:'0',o:'',t:'0')),version:4

[2]: https://gcc.godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:2,endLineNumber:26,positionColumn:2,positionLineNumber:26,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:'%23include+%3Chttps://raw.githubusercontent.com/HolyBlackCat/index_map/refs/heads/master/include/em/index_map.h%3E%0A%23include+%3Cprint%3E%0A%23include+%3Cstring%3E%0A%0Aint+main()%0A%7B%0A++++em::IndexMap%3C%0A++++++++std::string,+//+Value%0A++++++++unsigned+int,+//+Key%0A++++++++unsigned+int+//+Persistent+data%0A++++%3E+m%3B%0A%0A++++auto+key+%3D+m.insert(%22blah%22).key%3B%0A++++std::print(%22%7B%7D%5Cn%22,+m%5Bkey%5D)%3B+//+blah%0A++++auto+generation+%3D+m.get_persistent_data(key)%3B%0A++++std::print(%22%7B%7D%5Cn%22,+generation)%3B+//+0%0A%0A++++//+Erase+and+increment+the+counter+(in+any+order).%0A++++m.get_persistent_data(key)%2B%2B%3B%0A++++m.erase(key)%3B%0A%0A++++auto+key2+%3D+m.insert(%22foo%22).key%3B%0A++++std::print(%22%7B%7D%5Cn%22,+key2+%3D%3D+key)%3B+//+true,+the+key+got+reused.%0A++++std::print(%22%7B%7D%5Cn%22,+m.get_persistent_data(key))%3B+//+1+even+after+the+key+reuse%0A++++std::print(%22%7B%7D%5Cn%22,+m.get_persistent_data(key)+%3D%3D+generation)%3B+//+false,+the+generation+number+is+different%0A%7D'),l:'5',n:'1',o:'C%2B%2B+source+%231',t:'0')),k:53.19126676326873,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((g:!((h:compiler,i:(compiler:clang1910,filters:(b:'0',binary:'1',binaryObject:'1',commentOnly:'0',debugCalls:'1',demangle:'0',directives:'0',execute:'0',intel:'0',libraryCode:'1',trim:'1',verboseDemangling:'0'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:2,lang:c%2B%2B,libs:!(),options:'-stdlib%3Dlibc%2B%2B+-std%3Dc%2B%2B26+-Wall+-Wextra+-pedantic-errors+-D_GLIBCXX_DEBUG+-fsanitize%3Daddress+-fsanitize%3Dundefined',overrides:!(),selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1),l:'5',n:'0',o:'+x86-64+clang+19.1.0+(Editor+%231)',t:'0')),header:(),k:46.80873323673128,l:'4',m:50,n:'0',o:'',s:0,t:'0'),(g:!((h:output,i:(compilerName:'x86-64+clang+19.1.0',editorid:1,fontScale:14,fontUsePx:'0',j:2,wrap:'1'),l:'5',n:'0',o:'Output+of+x86-64+clang+19.1.0+(Compiler+%232)',t:'0')),l:'4',m:50,n:'0',o:'',s:0,t:'0')),k:46.80873323673128,l:'3',n:'0',o:'',t:'0')),l:'2',n:'0',o:'',t:'0')),version:4
