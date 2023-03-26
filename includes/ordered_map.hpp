#ifndef HEAVY_TREE_DAWG_ORDERED_MAP_HPP
#define HEAVY_TREE_DAWG_ORDERED_MAP_HPP

#include <vector>
#include <optional>


template<typename K, typename V>
struct OrderedMap {
    virtual std::optional<V> find(K key) const = 0;
    virtual int size() const = 0;
};


template<typename K, typename V>
struct BinarySearchMap : OrderedMap<K, V> {
    std::vector<K> keys;
    std::vector<V> values;
    explicit BinarySearchMap(const std::map<K, V>& map){
        for(auto [k, v] : map){
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
struct LinearSearchMap : OrderedMap<K, V> {
    std::vector<K> keys;
    std::vector<V> values;
    explicit LinearSearchMap(const std::map<K, V>& map){
        for(auto [k, v] : map){
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


#endif //HEAVY_TREE_DAWG_ORDERED_MAP_HPP
