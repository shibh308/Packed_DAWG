#ifndef HEAVY_TREE_DAWG_DAWG_HPP
#define HEAVY_TREE_DAWG_DAWG_HPP

#include <vector>
#include <string>
#include <map>
#include <queue>
#include <cstring>

#include "full_text_index.hpp"
#include "level_ancestor.hpp"
#include "map.hpp"


using ULong = std::uint64_t ;
constexpr unsigned int WORD_SIZE = 64;
constexpr unsigned int CHAR_BITS = 8;
constexpr unsigned int ALPHA = 8; // WORD_SIZE / CHAR_BITS;
constexpr unsigned int LOG_ALPHA = 3;


inline unsigned int get_lsb_pos(ULong val){
    if(val == 0){
        return ALPHA;
    }
    unsigned int ctz = __builtin_ctzll(val);
    return ctz / CHAR_BITS;
}

inline ULong get_head(std::string_view str, unsigned int i){
    ULong head = *reinterpret_cast<const ULong*>(str.data() + i);
    if(str.length() < CHAR_BITS){
        return head & ((1uLL << (CHAR_BITS * str.length())) - 1u);
    }
    else{
        return head;
    }
}

inline unsigned int get_lcp(std::string_view str1, unsigned int ofs1, std::string_view str2, unsigned int ofs2, unsigned int max_len){
    unsigned int max_iter = max_len / ALPHA;
    auto* ptr1 = reinterpret_cast<const ULong*>(str1.data() + ofs1);
    auto* ptr2 = reinterpret_cast<const ULong*>(str2.data() + ofs2);
    int i = 0;
    while(*ptr1 == *ptr2 && i < max_iter){
        ++i;
        ++ptr1;
        ++ptr2;
    }
    return std::min(i * ALPHA + get_lsb_pos(*ptr1 ^ *ptr2), max_len);
}

struct DAWGBase{
    struct Node{
        HashMap<unsigned char, int> ch;
        int slink, len;
        explicit Node(int len) : slink(-1), len(len){}
    };

    std::vector<Node> nodes;
    int final_node = 0;

    explicit DAWGBase(std::string_view text){
        nodes.emplace_back(0);
        for(int i = 0; i < text.size(); ++i){
            add_node(i, text[i]);
        }
        nodes.shrink_to_fit();
        std::clog << "DAWG Base construct end" << std::endl;
    }

    void add_node(int i, unsigned char c){
        int new_node = nodes.size();
        int target_node = (nodes.size() == 1 ? 0 : final_node);
        final_node = new_node;
        nodes.emplace_back(i + 1);

        for(; target_node != -1 &&
              !nodes[target_node].ch.find(c).has_value(); target_node = nodes[target_node].slink){
            nodes[target_node].ch.add(c, new_node);
        }
        if(target_node == -1){
            nodes[new_node].slink = 0;
        }else{
            int sp_node = nodes[target_node].ch.find(c).value();
            if(nodes[target_node].len + 1 == nodes[sp_node].len){
                nodes[new_node].slink = sp_node;
            }else{
                int clone_node = nodes.size();
                nodes.emplace_back(nodes[target_node].len + 1);
                nodes[clone_node].ch = nodes[sp_node].ch;
                nodes[clone_node].slink = nodes[sp_node].slink;
                for(; target_node != -1 && nodes[target_node].ch.find(c).has_value() &&
                      nodes[target_node].ch.find(c).value() == sp_node; target_node = nodes[target_node].slink){
                    nodes[target_node].ch.add(c, clone_node);
                }
                nodes[sp_node].slink = nodes[new_node].slink = clone_node;
            }
        }
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class SimpleDAWG : FullTextIndex {
    std::vector<MapType<unsigned char, int>> children;
public:
    explicit SimpleDAWG(const DAWGBase& base) {
        for(auto& node : base.nodes){
            children.emplace_back(node.ch);
        }
        children.shrink_to_fit();
    }
    explicit SimpleDAWG(std::string_view text) : SimpleDAWG(DAWGBase(text)) {}
    std::optional<int> get_node(std::string_view pattern) const override {
        int node = 0;
        for(auto c : pattern){
            auto res = children[node].find(c);
            if(res.has_value()){
                node = res.value();
            }
            else{
                return std::nullopt;
            }
        }
        return node;
    }
    virtual std::uint64_t num_bytes() const{
        std::uint64_t size = 0;
        size += 3 * sizeof(std::size_t);
        for(auto& map : children){
            size += map.num_bytes();
        }
        return size;
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWG : FullTextIndex {
protected:
    std::string text;
    std::string_view text_view;
    std::vector<int> heavy_edge_to;
    std::vector<MapType<unsigned char, int>> light_edges;
    std::vector<int> poses;
public:
    explicit HeavyTreeDAWG(std::string_view text) : text(text), text_view(text) {
        auto base = DAWGBase(text_view);
        int n = base.nodes.size();

        std::vector<int> tps_order(n);
        std::vector<int> in_degree(n, 0);
        for(int x = 0; x < n; ++x){
            for(auto [_key, y] : base.nodes[x].ch.items()){
                ++in_degree[y];
            }
        }
        std::queue<int> que;
        que.emplace(0);
        assert(in_degree[0] == 0);
        int cnt = 0;
        while(!que.empty()){
            int x = que.front();
            que.pop();
            tps_order[cnt] = x;
            ++cnt;
            for(auto [_key, y] : base.nodes[x].ch.items()){
                --in_degree[y];
                if(in_degree[y] == 0){
                    que.push(y);
                }
            }
        }
        assert(tps_order.size() == n);
        std::vector<int> path_cnt(n, 0);
        poses.resize(n, -1);
        int sink = tps_order.back();
        // assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        poses[sink] = text.size();
        std::vector<unsigned char> heavy_edge_label(n, 0);
        heavy_edge_to.resize(n, -1);
        for(auto it = tps_order.rbegin(); it != tps_order.rend(); ++it){
            int x = *it;
            int path_cnt_max = 0;
            for(auto [key, y] : base.nodes[x].ch.items()){
                path_cnt[x] += path_cnt[y];
                if(path_cnt_max < path_cnt[y]){
                    path_cnt_max = path_cnt[y];
                    heavy_edge_to[x] = y;
                    assert(poses[y] != -1);
                    poses[x] = poses[y] - 1;
                    heavy_edge_label[x] = key;
                }
            }
            assert(1 <= path_cnt[x] && path_cnt[x] <= n);
        }
        for(int x = 0; x < n; ++x){
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [key, y] : base.nodes[x].ch.items()){
                if(heavy_edge_label[x] != key){
                    keys.emplace_back(key);
                    values.emplace_back(y);
                }
            }
            light_edges.emplace_back(keys, values);
        }
        heavy_edge_to.shrink_to_fit();
        light_edges.shrink_to_fit();
        poses.shrink_to_fit();
    }

public:
    std::optional<int> get_node(std::string_view pattern) const override{
        unsigned int node = 0;
        for(unsigned int i = 0; i < pattern.length();){
            int pos = poses[node];
            int lcp = get_lcp(text_view, pos, pattern, i, std::min(text.length() - pos, pattern.length() - i));
            node = get_anc(node, lcp);
            i += lcp;
            if(i == pattern.length()){
                break;
            }
            auto light_to = light_edges[node].find(pattern[i]);
            if(light_to){
                node = light_to.value();
            }
            else{
                return std::nullopt;
            }
            ++i;
        }
        return node;
    }
    virtual inline int get_anc(int node, int k) const{
        for(int i = 0; i < k; ++i){
            node = heavy_edge_to[node];
        }
        return node;
    }
    virtual std::uint64_t num_bytes() const{
        std::uint64_t size = 0;
        size += text.capacity() * sizeof(unsigned char) + 3 * sizeof(std::size_t);
        size += 2 * sizeof(std::size_t);
        size += heavy_edge_to.capacity() * sizeof(int) + 3 * sizeof(std::size_t);
        size += 3 * sizeof(std::size_t);
        for(auto& map : light_edges){
            size += map.num_bytes();
        }
        size += poses.size() * sizeof(int) + 3 * sizeof(std::size_t);
        return size;
    }
};

template <template <typename, typename> typename MapType, typename LAType> requires std::is_base_of_v<LevelAncestor, LAType>
class HeavyTreeDAWGWithLA : HeavyTreeDAWG<MapType> {
private:
    LAType la;
public:
    explicit HeavyTreeDAWGWithLA(std::string_view text) : HeavyTreeDAWG<MapType>(text), la(this->heavy_edge_to){
        this->heavy_edge_to.clear();
        this->heavy_edge_to.shrink_to_fit();
    }
    std::optional<int> get_node(std::string_view pattern) const override{
        return HeavyTreeDAWG<MapType>::get_node(pattern);
    }
    inline int get_anc(int node, int k) const override{
        return la.get_anc(node, k);
    }
    virtual std::uint64_t num_bytes() const{
        return HeavyTreeDAWG<MapType>::num_bytes() + la.num_bytes();
    }
};


template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyPathDAWG : FullTextIndex {
    std::string hh_string;
    std::vector<MapType<unsigned char, int>> light_edges;
    int source;
public:
    explicit HeavyPathDAWG(const DAWGBase& base){
        int n = base.nodes.size();
        std::vector<int> tps_order(n);
        std::vector<int> in_degree(n, 0);
        for(int x = 0; x < n; ++x){
            for(auto [_key, y] : base.nodes[x].ch.items()){
                ++in_degree[y];
            }
        }
        std::queue<int> que;
        que.emplace(0);
        assert(in_degree[0] == 0);
        int cnt = 0;
        while(!que.empty()){
            int x = que.front();
            que.pop();
            tps_order[cnt] = x;
            ++cnt;
            for(auto [_key, y] : base.nodes[x].ch.items()){
                --in_degree[y];
                if(in_degree[y] == 0){
                    que.push(y);
                }
            }
        }
        assert(tps_order.size() == n);
        std::vector<int> path_cnt(n, 0);
        int sink = tps_order.back();
        // assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        std::vector<int> heavy_edge_to(n, -1);
        std::vector<unsigned char> heavy_edge_label(n);
        for(auto it = tps_order.rbegin(); it != tps_order.rend(); ++it){
            int x = *it;
            int path_cnt_max = 0;
            for(auto [key, y] : base.nodes[x].ch.items()){
                path_cnt[x] += path_cnt[y];
                if(path_cnt_max < path_cnt[y]){
                    path_cnt_max = path_cnt[y];
                    heavy_edge_to[x] = y;
                    heavy_edge_label[x] = key;
                }
            }
            assert(1 <= path_cnt[x] && path_cnt[x] <= n);
        }

        std::vector<std::vector<std::pair<unsigned char, int>>> heavy_tree(n);
        for(int x = 0; x < n; ++x){
            if(x != sink){
                heavy_tree[heavy_edge_to[x]].emplace_back(heavy_edge_label[x], x);
            }
        }
        cnt = 0;
        que.emplace(sink);
        while(!que.empty()){
            int x = que.front();
            tps_order[cnt] = x;
            ++cnt;
            que.pop();
            for(auto [_key, y] : heavy_tree[x]){
                que.push(y);
            }
        }
        path_cnt.assign(n, 0);
        // root (source) direction
        std::vector<int> hh_edge_source(n, -1);
        // leaf (sink) direction
        std::vector<int> hh_edge_sink(n, -1);
        std::vector<unsigned char> hh_edge_label(n);
        for(auto it = tps_order.rbegin(); it != tps_order.rend(); ++it){
            int x = *it;
            if(heavy_tree[x].empty()){
                path_cnt[x] = 1;
            }
            else{
                int path_cnt_max = 0;
                for(auto [key, y] : heavy_tree[x]){
                    if(path_cnt_max < path_cnt[y]){
                        path_cnt_max = path_cnt[y];
                        hh_edge_source[x] = y;
                        hh_edge_label[y] = key;
                    }
                    path_cnt[x] += path_cnt[y];
                }
                hh_edge_sink[hh_edge_source[x]] = x;
            }
        }
        std::vector<int> path_nodes(n);
        std::vector<int> path_nodes_inv(n);
        hh_string.resize(n, '\0');
        cnt = 0;
        for(int i = 0; i < n; ++i){
            if(hh_edge_source[i] == -1){
                // heavy path start
                std::vector<int> path;
                for(int x = i; x != -1; x = hh_edge_sink[x]){
                    path_nodes[cnt] = x;
                    path_nodes_inv[x] = cnt;
                    if(hh_edge_sink[x] != -1){
                        hh_string[cnt] = hh_edge_label[x];
                    }
                    ++cnt;
                }
            }
        }
        assert(cnt == n);
        for(int i = 0; i < n; ++i){
            int x = path_nodes[i];
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [key, y] : base.nodes[x].ch.items()){
                if(key != hh_string[i]){
                    keys.emplace_back(key);
                    values.emplace_back(path_nodes_inv[y]);
                }
            }
            light_edges.emplace_back(keys, values);
        }
        source = path_nodes_inv[0];
        sink = path_nodes_inv[sink];
        light_edges.shrink_to_fit();
    }
    explicit HeavyPathDAWG(std::string_view text) : HeavyPathDAWG(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{
        unsigned int node = source;
        for(unsigned int i = 0; i < pattern.length();){
            int lcp = get_lcp(pattern, i, hh_string, node, pattern.length() - i);
            node += lcp;
            i += lcp;
            if(i == pattern.length()){
                break;
            }
            auto light_to = light_edges[node].find(pattern[i]);
            if(light_to){
                node = light_to.value();
            }
            else{
                return std::nullopt;
            }
            ++i;
        }
        return node;
    }
    virtual std::uint64_t num_bytes() const{
        std::uint64_t size = 0;
        size += sizeof(int);
        size += hh_string.capacity() * sizeof(unsigned char) + 3 * sizeof(std::uint64_t);
        size += 3 * sizeof(std::uint64_t);
        for(auto& x : light_edges){
            size += x.num_bytes();
        }
        return size;
    }
};

#endif //HEAVY_TREE_DAWG_DAWG_HPP
