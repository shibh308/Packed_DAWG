#ifndef HEAVY_TREE_DAWG_ORDERED_MAP_HPP
#define HEAVY_TREE_DAWG_ORDERED_MAP_HPP

#include <vector>
#include <optional>

template<typename K, typename V>
class BinarySearchMap{
    std::vector<K> keys;
    std::vector<V> values;
public:
    explicit BinarySearchMap(const std::map<K, V>& map){
        for(auto [k, v] : map){
            keys.emplace_back(k);
            values.emplace_back(v);
        }
    }
    BinarySearchMap(std::vector<K> keys, std::vector<V> values) : keys(keys), values(values){
        assert(keys.size() == values.size());
        for(int i = 0; i < keys.size(); ++i){
            assert(keys[i] < keys[i + 1]);
        }
    }
    std::optional<V> find(const K key) const{
        unsigned int l = 0, r = keys.size();
        while(r - l > 3){
            unsigned int mid = (r - l) >> 1u;
            if(keys[mid] == key){
                return values[mid];
            }
            else if(keys[mid] < key){
                l = mid;
            }
            else{
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
};


#endif //HEAVY_TREE_DAWG_ORDERED_MAP_HPP
