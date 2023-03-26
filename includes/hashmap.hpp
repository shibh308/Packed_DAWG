//
// Created by shibh308 on 3/26/23.
//

#ifndef HEAVY_TREE_DAWG_HASHMAP_HPP
#define HEAVY_TREE_DAWG_HASHMAP_HPP

#include <vector>
#include <cstdint>
#include <numeric>
#include <optional>


template <typename T, typename U, T del = std::numeric_limits<T>::max(), T null = std::numeric_limits<T>::max() - 1>
struct HashMap{
    static constexpr __int128_t z = 0xf332ac987401cba5;
    std::uint64_t n, q, d;

    std::vector<std::pair<T, U>> v;

    HashMap() : n(0), q(0), d(1),  v(2, make_pair(null, U())){
    }

    inline std::uint64_t hash(T key){return std::uint64_t((z * __uint128_t(key)) >> (64u - d)) & ((1uLL << d) - 1);}

    std::optional<U> find(T x){
        for(std::uint64_t i = hash(x); v[i].first != null; i = (i + 1) & ((1u << d) - 1))
            if(v[i].first == x)
                return v[i].second;
        return std::nullopt;
    }

    bool add(T x, U val){
        if(find(x).has_value())
            return false;
        if(((q + 1) << 1u) > (1u << d) || (1u << d) < 3 * n)
            resize();
        std::uint64_t i = hash(x);
        for(; v[i].first != null && v[i].first != del; i = (i + 1) & ((1u << d) - 1));
        q += (v[i].first == null);
        ++n;
        v[i] = make_pair(x, val);
        return true;
    }

    bool erase(T x){
        std::uint64_t i = hash(x);
        for(; v[i].first != null && v[i].first != x; i = (i + 1) & ((1u << d) - 1));
        if(v[i].first == null)
            return false;
        --n;
        v[i] = make_pair(del, U());
        return true;
    }

    void resize(){
        ++d;
        std::vector<std::pair<T, U>> old_table;
        q = n;
        swap(old_table, v);
        v.assign(1 << d, make_pair(null, U()));
        n = 0;
        for(int i = 0; i < old_table.size(); ++i)
            if(old_table[i].first != null && old_table[i].first != del)
                add(old_table[i].first, old_table[i].second);
    }
};

#endif //HEAVY_TREE_DAWG_HASHMAP_HPP
