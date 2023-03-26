#ifndef HEAVY_TREE_DAWG_DAWG_HPP
#define HEAVY_TREE_DAWG_DAWG_HPP

#include <vector>
#include <string>
#include <map>
#include <queue>
#include <cstring>

#include "full_text_index.hpp"
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

struct DAWGBase{
    struct Node{
        HashMap<unsigned char, int> ch;
        int slink, len;

        explicit Node(int len) : slink(-1), len(len){}
    };

    std::vector<Node> nodes;
    std::vector<int> node_ids;

    explicit DAWGBase(std::string_view text){
        nodes.emplace_back(0);
        for(int i = 0; i < text.size(); ++i){
            add_node(i, text[i]);
        }
        std::clog << "DAWG Base construct end" << std::endl;
    }

    void add_node(int i, unsigned char c){
        int new_node = nodes.size();
        int target_node = (nodes.size() == 1 ? 0 : node_ids.back());
        node_ids.emplace_back(new_node);
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
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWG : FullTextIndex {
protected:
    std::vector<int> heavy_edge_to;
    std::vector<MapType<unsigned char, int>> light_edges;
    std::vector<ULong> heads;
    int sink;
public:
    explicit HeavyTreeDAWG(const DAWGBase& base){
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
        sink = tps_order.back();
        assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        std::vector<unsigned char> heavy_edge_label(n, 0);
        heavy_edge_to.resize(n, 0);
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

        heads.resize(n);
        heads[sink] = 0;
        for(auto it = tps_order.rbegin(); it != tps_order.rend(); ++it){
            int x = *it;
            if(x != sink){
                int y = heavy_edge_to[x];
                heads[x] = heavy_edge_label[x] | (heads[y] << CHAR_BITS);
            }
        }
    }

    explicit HeavyTreeDAWG(std::string_view text) : HeavyTreeDAWG(DAWGBase(text)){}

public:
    std::optional<int> get_node(std::string_view pattern) const override{
        int node = 0;
        for(unsigned int i = 0; i < pattern.length();){
            ULong pattern_byte = get_head(pattern, i);
            ULong xor_result = heads[node] ^ pattern_byte;
            unsigned int lcp = std::min(get_lsb_pos(xor_result), static_cast<unsigned int>(pattern.length()) - i);
            if(lcp == ALPHA){
                node = get_anc_alpha(node);
                i += ALPHA;
            }
            else{
                if(lcp != 0){
                    node = get_anc(node, lcp);
                    i += lcp;
                }
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
        }
        return node;
    }
    virtual inline int get_anc(int node, int k) const = 0;
    virtual inline int get_anc_alpha(int node) const = 0;
};


template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWGWithNaiveAnc : HeavyTreeDAWG<MapType> {
public:
    explicit HeavyTreeDAWGWithNaiveAnc(const DAWGBase& base) : HeavyTreeDAWG<MapType>(base) {}
    explicit HeavyTreeDAWGWithNaiveAnc(std::string_view text) : HeavyTreeDAWGWithNaiveAnc<MapType>(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{ return HeavyTreeDAWG<MapType>::get_node(pattern); }
private:
    inline int get_anc(int node, int k) const override{
        for(int i = 0; i < k; ++i){
            node = this->heavy_edge_to[node];
        }
        return node;
    }
    inline int get_anc_alpha(int node) const override{
        for(int i = 0; i < ALPHA; ++i){
            node = this->heavy_edge_to[node];
        }
        return node;
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWGWithExpAnc : HeavyTreeDAWG<MapType> {
    std::vector<std::vector<unsigned int>> anc;
public:
    explicit HeavyTreeDAWGWithExpAnc(const DAWGBase& base) : HeavyTreeDAWG<MapType>(base) {
        int n = this->heads.size();
        anc.resize(LOG_ALPHA + 1, std::vector<unsigned int>(n));
        for(int i = 0; i < n; ++i){
            anc[0][i] = this->heavy_edge_to[i];
        }
        for(int k = 0; k < LOG_ALPHA; ++k){
            for(int i = 0; i < n; ++i){
                anc[k + 1][i] = anc[k][anc[k][i]];
            }
        }
    }
    explicit HeavyTreeDAWGWithExpAnc(std::string_view text) : HeavyTreeDAWGWithExpAnc<MapType>(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{ return HeavyTreeDAWG<MapType>::get_node(pattern); }
private:
    inline int get_anc(int node, int k) const override{
        for(unsigned int i = 0; i <= LOG_ALPHA && (1u << i) <= k; ++i){
            if(static_cast<unsigned int>(k) & (1u << i)){
                node = anc[i][node];
            }
        }
        return node;
    }
    inline int get_anc_alpha(int node) const override{
        return anc[LOG_ALPHA][node];
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWGWithMemoAnc : HeavyTreeDAWG<MapType> {
    int n;
    std::vector<unsigned int> anc;
public:
    explicit HeavyTreeDAWGWithMemoAnc(const DAWGBase& base) : HeavyTreeDAWG<MapType>(base) {
        n = this->heads.size();
        anc.resize((ALPHA + 1) * n);
        for(unsigned int i = 0; i < n; ++i){
            anc[i] = i;
        }
        for(int k = 0; k < ALPHA; ++k){
            for(unsigned int i = 0; i < n; ++i){
                anc[i + (k + 1) * n] = anc[this->heavy_edge_to[i] + k * n];
            }
        }
    }
    explicit HeavyTreeDAWGWithMemoAnc(std::string_view text) : HeavyTreeDAWGWithMemoAnc<MapType>(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{
        int node = 0;
        for(unsigned int i = 0; i < pattern.length();){
            ULong pattern_byte = get_head(pattern, i);
            ULong xor_result = this->heads[node] ^ pattern_byte;
            unsigned int lcp = std::min(get_lsb_pos(xor_result), static_cast<unsigned int>(pattern.length()) - i);
            if(lcp != 0){
                node = anc[node + lcp * n];
                i += lcp;
            }
            if(lcp != ALPHA){
                if(i == pattern.length()){
                    break;
                }
                auto light_to = this->light_edges[node].find(pattern[i]);
                if(light_to){
                    node = light_to.value();
                }else{
                    return std::nullopt;
                }
                ++i;
            }
        }
        return node;
    }
private:
    inline int get_anc(int node, int k) const override{
        return anc[node + k * n];
    }
    inline int get_anc_alpha(int node) const override{
        return anc[node + ALPHA * n];
    }
};


template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyTreeDAWGWithHPDAnc : HeavyTreeDAWG<MapType> {
    int source;
    std::vector<unsigned int> path_remain;
public:
    explicit HeavyTreeDAWGWithHPDAnc(const DAWGBase& base) : HeavyTreeDAWG<MapType>(base) {
        int n = this->heads.size();
        std::vector<std::vector<int>> heavy_tree(n);
        for(int x = 0; x < n; ++x){
            if(x != this->sink){
                heavy_tree[this->heavy_edge_to[x]].emplace_back(x);
            }
        }
        std::vector<int> order(n);
        int cnt = 0;
        std::queue<int> que;
        que.emplace(this->sink);
        while(!que.empty()){
            int x = que.front();
            order[cnt] = x;
            ++cnt;
            que.pop();
            for(auto y : heavy_tree[x]){
                que.push(y);
            }
        }
        std::vector<int> path_cnt(n, 0);
        // root (source) direction
        std::vector<int> hh_edge_source(n, -1);
        // leaf (sink) direction
        std::vector<int> hh_edge_sink(n, -1);
        for(auto it = order.rbegin(); it != order.rend(); ++it){
            int x = *it;
            if(heavy_tree[x].empty()){
                path_cnt[x] = 1;
            }
            else{
                int path_cnt_max = 0;
                for(auto y : heavy_tree[x]){
                    if(path_cnt_max < path_cnt[y]){
                        path_cnt_max = path_cnt[y];
                        hh_edge_source[x] = y;
                    }
                    path_cnt[x] += path_cnt[y];
                }
                hh_edge_sink[hh_edge_source[x]] = x;
            }
        }
        std::vector<int> path_nodes_inv(n);
        path_remain.resize(n);
        cnt = 0;
        for(int i = 0; i < n; ++i){
            if(hh_edge_source[i] == -1){
                // heavy path start
                std::vector<int> path;
                for(int x = i; x != -1; x = hh_edge_sink[x]){
                    path.emplace_back(x);
                }
                for(int j = 0; j < path.size(); ++j){
                    path_nodes_inv[path[j]] = cnt;
                    path_remain[cnt] = path.size() - j - 1;
                    ++cnt;
                }
            }
        }

        auto light_edges_tmp = this->light_edges;
        auto heads_tmp = this->heads;
        auto heavy_edge_to_tmp = this->heavy_edge_to;

        for(int x = 0; x < n; ++x){
            this->heads[path_nodes_inv[x]] = heads_tmp[x];
            for(auto &[key_, y] : light_edges_tmp[x].items()){
                y = path_nodes_inv[y];
            }
            this->light_edges[path_nodes_inv[x]] = std::move(light_edges_tmp[x]);
            this->heavy_edge_to[path_nodes_inv[x]] = path_nodes_inv[heavy_edge_to_tmp[x]];
        }
        source = path_nodes_inv[0];
        this->sink = path_nodes_inv[this->sink];
        assert(cnt == n);
    }
    explicit HeavyTreeDAWGWithHPDAnc(std::string_view text) : HeavyTreeDAWGWithHPDAnc<MapType>(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{
        unsigned int node = source;
        for(unsigned int i = 0; i < pattern.length();){
            ULong pattern_byte = get_head(pattern, i);
            ULong xor_result = this->heads[node] ^ pattern_byte;
            unsigned int lcp = std::min(get_lsb_pos(xor_result), static_cast<unsigned int>(pattern.length()) - i);
            if(lcp != 0){
                for(unsigned int k = lcp; k != 0;){
                    int skip = std::min(path_remain[node], k);
                    node += skip;
                    k -= skip;
                    if(k != 0){
                        node = this->heavy_edge_to[node];
                        --k;
                    }
                }
                i += lcp;
            }
            if(i == pattern.length()){
                break;
            }
            if(lcp != ALPHA){
                auto light_to = this->light_edges[node].find(pattern[i]);
                if(light_to){
                    node = light_to.value();
                }
                else{
                    return std::nullopt;
                }
                ++i;
            }
        }
        return node;
    }
private:
    inline int get_anc(int node, int k) const override{
        while(true){
            int skip = path_remain[node];
            if(k <= skip){
                return node + k;
            }
            else{
                node = this->heavy_edge_to[node + skip];
                k -= skip + 1;
            }
        }
    }
    inline int get_anc_alpha(int node) const override{
        int k = ALPHA;
        while(true){
            int skip = path_remain[node];
            if(k <= skip){
                return node + k;
            }
            else{
                node = this->heavy_edge_to[node + skip];
                k -= skip + 1;
            }
        }
    }
};

template <template <typename, typename> typename MapType> // requires std::is_base_of_v<Map, MapType>
class HeavyPathDAWG : FullTextIndex {
    std::string hh_string;
    std::vector<MapType<unsigned char, int>> light_edges;
    int source, sink;
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
        sink = tps_order.back();
        assert(sink == base.node_ids.back());
        path_cnt[sink] = 1;
        std::vector<int> heavy_edge_to(n, 0);
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
        que.emplace(this->sink);
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
    }
    explicit HeavyPathDAWG(std::string_view text) : HeavyPathDAWG(DAWGBase(text)){}
    std::optional<int> get_node(std::string_view pattern) const override{
        unsigned int node = source;
        for(unsigned int i = 0; i < pattern.length();){
            ULong pattern_byte = get_head(pattern, i);
            ULong hh_byte = get_head(hh_string, node);
            ULong xor_result = hh_byte ^ pattern_byte;
            unsigned int lcp = std::min(get_lsb_pos(xor_result), static_cast<unsigned int>(pattern.length()) - i);
            if(lcp == ALPHA){
                node += ALPHA;
                i += ALPHA;
            }
            else{
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
        }
        return node;
    }
};


#endif //HEAVY_TREE_DAWG_DAWG_HPP
