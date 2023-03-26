#ifndef HEAVY_TREE_DAWG_MAP_HPP
#define HEAVY_TREE_DAWG_MAP_HPP

#include <bit>
#include <vector>
#include <cstdint>
#include <numeric>
#include <optional>


template<typename K, typename V>
struct Map {
    virtual std::optional<V> find(K key) const = 0;
    virtual int size() const = 0;
};

template <typename T, typename U>
struct HashMap : Map<T, U> {
    static constexpr std::uint64_t z = 65521;
    static constexpr T null = 0;
    // static constexpr std::uint64_t z = 60xf332ac987401cba5;
    std::uint64_t n, d;

    std::vector<std::pair<T, U>> v;
    std::array<std::uint64_t, 4> exists;

    HashMap() : n(0), d(1),  v(2, std::make_pair(null, U())){
        exists[0] = exists[1] = exists[2] = exists[3] = 0;
    }
    explicit HashMap(const std::vector<T>& keys, const std::vector<U>& values) : n(0), d(1), v(2, std::make_pair(null, U())){
        exists[0] = exists[1] = exists[2] = exists[3] = 0;
        for(int i = 0; i < keys.size(); ++i){
            add(keys[i], values[i]);
        }
    }

    inline std::uint64_t hash(T key) const{return (z * key) & ((1u << d) - 1); }

    int size() const override { return int(n); }

    std::optional<U> find(T x) const override {
        for(std::uint64_t i = hash(x); v[i].first != null; i = (i + 1) & ((1u << d) - 1)){
            if(v[i].first == x){
                return v[i].second;
            }
        }
        return std::nullopt;
    }

    void add(T x, U val){
        assert(x != null);
        if((1u << d) < ((n + 1) * 2u)){
            resize();
        }
        std::uint64_t i = hash(x);
        for(; v[i].first != null && v[i].first != x; i = (i + 1) & ((1u << d) - 1));
        n += v[i].first == null;
        v[i] = {x, val};
        exists[i / 64] |= 1uLL << (i & 63u);
    }

    std::vector<std::pair<T, U>> items() const{
        std::vector<std::pair<T, U>> items;
        unsigned int siz = v.size();
        for(auto item : v){
            if(item.first != null){
                items.emplace_back(item);
            }
        }
        /*
        for(int i = 0; i < siz; ++i){
            std::uint64_t flag = exists[i];
            while(flag){
                std::uint64_t xo = (flag & -flag);
                items.emplace_back(v[i * 64 + std::bit_width(xo) - 1]);
                flag &= ~xo;
            }
        }
         */
        sort(items.begin(), items.end());
        return items;
    }

    void resize(){
        exists[0] = exists[1] = exists[2] = exists[3] = 0;
        ++d;
        std::vector<std::pair<T, U>> old_table;
        swap(old_table, v);
        v.assign(1u << d, {null, U()});
        assert(v.size() <= 256);
        n = 0;
        for(auto item : old_table){
            if(item.first != null){
                add(item.first, item.second);
            }
        }
    }
};


template<typename K, typename V>
struct BinarySearchMap : Map<K, V> {
    std::vector<K> keys;
    std::vector<V> values;
    explicit BinarySearchMap(const std::map<K, V>& map){
        for(auto [k, v] : map){
            keys.emplace_back(k);
            values.emplace_back(v);
        }
    }
    explicit BinarySearchMap(const HashMap<K, V>& map){
        for(auto [k, v] : map.items()){
            keys.emplace_back(k);
            values.emplace_back(v);
        }
    }
    BinarySearchMap(const std::vector<K>& keys, const std::vector<V>& values) : keys(keys), values(values){
        assert(keys.size() == values.size());
        for(int i = 0; i < static_cast<int>(keys.size()) - 1; ++i){
            assert(keys[i] < keys[i + 1]);
        }
    }
    std::optional<V> find(const K key) const override{
        constexpr int linear_search_border = 3;
        unsigned int l = 0, r = keys.size();
        while(r - l > linear_search_border){
            unsigned int mid = (r - l) >> 1u;
            if(keys[mid] == key){
                return values[mid];
            }else if(keys[mid] < key){
                l = mid;
            }else{
                r = mid;
            }
        }
        for(unsigned int i = l; i < r; ++i){
            if(keys[i] == key){
                return values[i];
            }
            else if(key < keys[i]){
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    int size() const override{
        return keys.size();
    }
};

template<typename K, typename V>
struct LinearSearchMap : Map<K, V> {
    std::vector<K> keys;
    std::vector<V> values;
    explicit LinearSearchMap(const std::map<K, V>& map){
        for(auto [k, v] : map){
            keys.emplace_back(k);
            values.emplace_back(v);
        }
    }
    explicit LinearSearchMap(const HashMap<K, V>& map){
        for(auto [k, v] : map.items()){
            keys.emplace_back(k);
            values.emplace_back(v);
        }
    }
    LinearSearchMap(const std::vector<K>& keys, const std::vector<V>& values) : keys(keys), values(values){
        assert(keys.size() == values.size());
        for(int i = 0; i < static_cast<int>(keys.size()) - 1; ++i){
            assert(keys[i] < keys[i + 1]);
        }
    }
    std::optional<V> find(const K key) const override{
        for(int i = 0; i < keys.size(); ++i){
            if(keys[i] == key){
                return values[i];
            }
            else if(key < keys[i]){
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    int size() const override{
        return keys.size();
    }
};

template <typename T, typename U>
struct StdMapWrapper : Map<T, U>{
    std::map<T, U> map;
    explicit StdMapWrapper(){}
    explicit StdMapWrapper(const HashMap<T, U>& map_){
        for(auto item : map_.items()){
            map.insert(item);
        }
    }
    StdMapWrapper(const std::vector<T>& keys, const std::vector<U>& values){
        for(int i = 0; i < keys.size(); ++i){
            map.insert({keys[i], values[i]});
        }
    }
    int size() const override{ return map.size(); }
    std::optional<U> find(T key) const override{
        auto iter = map.find(key);
        if(iter == map.end()){
            return std::nullopt;
        }
        else{
            return iter->second;
        }
    }
    bool add(T x, U val){
        map.erase(x);
        return map.insert({x, val}).second;
    }
    std::vector<std::pair<T, U>> items() const{
        return std::vector<std::pair<T, U>>(map.begin(), map.end());
    }
};

#endif //HEAVY_TREE_DAWG_MAP_HPP
