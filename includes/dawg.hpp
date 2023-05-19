#ifndef HEAVY_TREE_DAWG_DAWG_HPP
#define HEAVY_TREE_DAWG_DAWG_HPP

#include <vector>
#include <string>
#include <map>
#include <queue>
#include <cstring>

#include "full_text_index.hpp"
#include "sdsl/bp_support.hpp"
#include "map.hpp"
#include "vector.hpp"


using ULong = std::uint64_t;
// constexpr unsigned int WORD_SIZE = 64;
constexpr unsigned int CHAR_BITS = 8;
constexpr unsigned int ALPHA = 8; // WORD_SIZE / CHAR_BITS;

inline unsigned int get_lsb_pos(ULong val){
    if(val == 0){
        return ALPHA;
    }
    unsigned int ctz = __builtin_ctzll(val);
    return ctz / CHAR_BITS;
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
        DynamicHashMap<unsigned char, int> ch;
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
    Vector<MapType<unsigned char, int>, std::uint32_t> children;
public:
    explicit SimpleDAWG(const DAWGBase& base) {
        std::vector<MapType<unsigned char, int>> children_;
        for(auto& node : base.nodes){
            children_.emplace_back(node.ch);
        }
        children = children_;
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
        size += decltype(children)::offset_bytes;
        for(int i = 0; i < children.size(); ++i){
            size += children[i].num_bytes();
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
    Vector<MapType<unsigned char, int>, std::uint32_t> light_edges;
    Vector<int, std::uint32_t> poses;
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
        std::vector<int> poses_(n, 0);
        int sink = tps_order.back();
        // assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        poses_[sink] = text.size();
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
                    assert(poses_[y] != -1);
                    poses_[x] = poses_[y] - 1;
                    heavy_edge_label[x] = key;
                }
            }
            assert(1 <= path_cnt[x] && path_cnt[x] <= n);
        }
        std::vector<MapType<unsigned char, int>> light_edges_;
        for(int x = 0; x < n; ++x){
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [key, y] : base.nodes[x].ch.items()){
                if(heavy_edge_label[x] != key){
                    keys.emplace_back(key);
                    values.emplace_back(y);
                }
            }
            light_edges_.emplace_back(keys, values);
        }
        poses = poses_;
        light_edges = light_edges_;
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
        size += text.capacity() * sizeof(unsigned char) + 2 * sizeof(std::size_t);
        size += 2 * sizeof(std::size_t);
        size += heavy_edge_to.capacity() * sizeof(int) + 2 * sizeof(std::size_t);
        size += light_edges.offset_bytes;
        for(int i = 0; i < light_edges.size(); ++i){
            size += light_edges[i].num_bytes();
        }
        size += poses.num_bytes();
        return size;
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWGWithLABP : FullTextIndex {
protected:
    std::string text;
    std::string_view text_view;
    int source;
    Vector<MapType<unsigned char, int>, std::uint32_t> light_edges;
    Vector<int, std::uint32_t> poses;
    sdsl::bit_vector bp;
    sdsl::bp_support_sada<> rich_bp;
public:
    explicit HeavyTreeDAWGWithLABP(std::string_view text) : text(text), text_view(text) {
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
        std::vector<int> poses_(n, -1);
        int sink = tps_order.back();
        // assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        poses_[sink] = text.size();
        std::vector<unsigned char> heavy_edge_label(n, 0);
        std::vector<int> heavy_edge_to(n, -1);
        for(auto it = tps_order.rbegin(); it != tps_order.rend(); ++it){
            int x = *it;
            int path_cnt_max = 0;
            for(auto [key, y] : base.nodes[x].ch.items()){
                path_cnt[x] += path_cnt[y];
                if(path_cnt_max < path_cnt[y]){
                    path_cnt_max = path_cnt[y];
                    heavy_edge_to[x] = y;
                    assert(poses_[y] != -1);
                    poses_[x] = poses_[y] - 1;
                    heavy_edge_label[x] = key;
                }
            }
            assert(1 <= path_cnt[x] && path_cnt[x] <= n);
        }
        std::vector<MapType<unsigned char, int>> light_edges_;
        for(int x = 0; x < n; ++x){
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [key, y] : base.nodes[x].ch.items()){
                if(heavy_edge_label[x] != key){
                    keys.emplace_back(key);
                    values.emplace_back(y);
                }
            }
            light_edges_.emplace_back(keys, values);
        }
        light_edges = light_edges_;

        int root;
        std::vector<int> indexes(2 * n, -1);
        std::vector<std::vector<int>> tree(n);
        for(int i = 0; i < n; ++i){
            if(heavy_edge_to[i] == -1){
                root = i;
                continue;
            }
            assert(0 <= heavy_edge_to[i] && heavy_edge_to[i] < n);
            tree[heavy_edge_to[i]].emplace_back(i);
        }

        bp = sdsl::bit_vector (2 * n, 0);

        cnt = 0;
        int cnt2 = 0;
        std::vector<int> indexes_fl(n, 0);
        std::stack<std::pair<int, bool>> stack;
        stack.emplace(root, false);
        stack.emplace(root, true);
        while(!stack.empty()){
            auto [x, fl] = stack.top();
            stack.pop();
            if(fl){
                indexes[x] = cnt;
                indexes_fl[x] = cnt2;
                bp[cnt] = fl;
                ++cnt;
                ++cnt2;
                for(auto y : tree[x]){
                    stack.emplace(y, false);
                    stack.emplace(y, true);
                }
            }
            else{
                bp[cnt] = fl;
                ++cnt;
            }
        }
        rich_bp = sdsl::bp_support_sada<>(&bp);
        source = indexes[0];

        decltype(light_edges_) light_edges__(n);
        decltype(poses_) poses__(n);
        for(int i = 0; i < n; ++i){
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [k, v] : light_edges_[i].items()){
                keys.emplace_back(k);
                values.emplace_back(indexes[v]);
            }
            light_edges__[indexes_fl[i]] = MapType(keys, values);
            poses__[indexes_fl[i]] = poses_[i];
        }
        light_edges = light_edges__;
        poses = poses__;
    }

public:
    std::optional<int> get_node(std::string_view pattern) const override{
        unsigned int node = source;
        for(unsigned int i = 0; i < pattern.length();){
            int pos = poses[rich_bp.rank(node-1)];
            int lcp = get_lcp(text_view, pos, pattern, i, std::min(text.length() - pos, pattern.length() - i));
            node = rich_bp.level_anc(node, lcp);
            i += lcp;
            if(i == pattern.length()){
                break;
            }
            auto light_to = light_edges[rich_bp.rank(node-1)].find(pattern[i]);
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
        size += text.capacity() * sizeof(unsigned char) + 2 * sizeof(std::size_t);
        size += decltype(light_edges)::offset_bytes;
        for(int i = 0; i < light_edges.size(); ++i){
            size += light_edges[i].num_bytes();
        }
        size += poses.num_bytes();
        std::ofstream of("/dev/null");
        size += bp.serialize(of);
        size += rich_bp.serialize(of);
        return size;
    }
};


template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyPathDAWG : FullTextIndex {
    std::string hh_string;
    Vector<MapType<unsigned char, int>, std::uint32_t> light_edges;
    int source;
public:
    explicit HeavyPathDAWG(std::string_view text){
        DAWGBase base(text);
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
        std::vector<MapType<unsigned char, int>> light_edges_;
        int edge_cnt = 0;
        int hh_edge_cnt = 0;
        for(int i = 0; i < n; ++i){
            int x = path_nodes[i];
            std::vector<unsigned char> keys;
            std::vector<int> values;
            for(auto [key, y] : base.nodes[x].ch.items()){
                ++edge_cnt;
                hh_edge_cnt += (key == hh_string[i]);
                if(key != hh_string[i]){
                    keys.emplace_back(key);
                    values.emplace_back(path_nodes_inv[y]);
                }
            }
            light_edges_.emplace_back(keys, values);
        }
        source = path_nodes_inv[0];
        light_edges = light_edges_;
        int heavy_edge_cnt = n - 1;
        std::clog << "n   : " << text.size() << std::endl;
        std::clog << "|V| : " << n << std::endl;
        std::clog << "|E| : " << edge_cnt << std::endl;
        std::clog << "|H| : " << heavy_edge_cnt << std::endl;
        std::clog << "|HH|: " << hh_edge_cnt << std::endl;
        std::clog << "|HL|: " << heavy_edge_cnt - hh_edge_cnt << std::endl;
        std::clog << "|L| : " << edge_cnt - heavy_edge_cnt << std::endl;
        std::clog << std::endl;
    }
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
        size += sizeof(source);
        size += hh_string.capacity() * sizeof(unsigned char) + 2 * sizeof(std::uint64_t);
        size += decltype(light_edges)::offset_bytes;
        for(int i = 0; i < light_edges.size(); ++i){
            size += light_edges[i].num_bytes();
        }
        return size;
    }
};

#endif //HEAVY_TREE_DAWG_DAWG_HPP
