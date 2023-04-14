//
// Created by shibh308 on 4/8/23.
//

#ifndef HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP
#define HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP

#include "sdsl/bp_support.hpp"

class LevelAncestor{
    virtual inline int get_anc(int x, unsigned int k) const = 0;
    virtual std::uint64_t num_bytes() const = 0;
};

class LevelAncestorByLadder : LevelAncestor{
private:
    int root;
    std::vector<std::vector<int>> heavy_pathes;
    std::vector<int> heavy_path_idx;
    std::vector<int> pos_in_heavy_path;
public:
    explicit LevelAncestorByLadder(const std::vector<int>& parent_vec) {
        int n = parent_vec.size();
        std::vector<std::vector<int>> tree(n);
        for(int i = 0; i < n; ++i){
            if(parent_vec[i] == -1){
                root = i;
                continue;
            }
            assert(0 <= parent_vec[i] && parent_vec[i] < n);
            tree[parent_vec[i]].emplace_back(i);
        }
        std::vector<int> depth(n);
        std::vector<int> bfs_order(n, -1);
        std::queue<int> que;
        depth[root] = 0;
        bfs_order[0] = root;
        que.emplace(root);
        int cnt = 1;
        while(!que.empty()){
            int x = que.front();
            que.pop();
            assert(0 <= x && x < n);
            for(auto y : tree[x]){
                assert(0 <= y && y < n);
                depth[y] = depth[x] + 1;
                bfs_order[cnt] = y;
                ++cnt;
                que.push(y);
            }
        }
        assert(cnt == n);
        std::vector<int> heavy_in(n, -1);
        for(int x = 0; x < n; ++x){
            int heavy = -1;
            for(auto y : tree[x]){
                if(heavy == -1 || depth[heavy] < depth[y]){
                    heavy = y;
                }
            }
            if(heavy != -1){
                heavy_in[heavy] = x;
            }
        }
        heavy_path_idx.resize(n, -1);
        pos_in_heavy_path.resize(n);
        for(int i = n - 1; i >= 0; --i){
            int x = bfs_order[i];
            assert(0 <= x && x < n);
            if(heavy_path_idx[x] == -1){
                pos_in_heavy_path[x] = 0;
                heavy_path_idx[x] = heavy_pathes.size();
                heavy_pathes.emplace_back(1, x);
            }
            int y = heavy_in[x];
            if(y != -1){
                assert(0 <= y && y < n);
                int heavy_idx = heavy_path_idx[x];
                assert(0 <= heavy_idx && heavy_idx < heavy_pathes.size());
                auto& heavy_path = heavy_pathes[heavy_idx];
                assert(heavy_path.back() == x);
                heavy_path_idx[y] = heavy_idx;
                pos_in_heavy_path[y] = heavy_path.size();
                heavy_path.emplace_back(y);
            }
        }
        for(auto& heavy_path : heavy_pathes){
            int length = heavy_path.size();
            while(heavy_path.size() < length * 2){
                if(heavy_path.back() == root){
                    break;
                }
                heavy_path.emplace_back(parent_vec[heavy_path.back()]);
            }
        }
        heavy_pathes.shrink_to_fit();
        for(auto& path : heavy_pathes){
            path.shrink_to_fit();
        }
        heavy_path_idx.shrink_to_fit();
        pos_in_heavy_path.shrink_to_fit();
    }
    inline int get_anc(int x, unsigned int k) const override{
        while(k != 0 && x != root){
            int path_idx = heavy_path_idx[x];
            int pos_in_path = pos_in_heavy_path[x];
            auto& heavy_path = heavy_pathes[path_idx];
            if(pos_in_path + k < heavy_path.size()){
                return heavy_path[pos_in_path + k];
            }
            else{
                k -= heavy_path.size() - pos_in_path - 1;
                x = heavy_path.back();
            }
        }
        assert(k == 0);
        return x;
    }
    virtual std::uint64_t num_bytes() const{
        // about 4|V| words
        std::uint64_t size = sizeof(root);
        size += sizeof(std::size_t) * 3;
        for(auto& path : heavy_pathes){
            size += (path.capacity() * sizeof(int) + sizeof(std::size_t) * 2);
        }
        size += (heavy_path_idx.capacity() * sizeof(int) + sizeof(std::size_t) * 2);
        size += (pos_in_heavy_path.capacity() * sizeof(int) + sizeof(std::size_t) * 2);
        return size;
    }
};


class LevelAncestorByBP : LevelAncestor{
private:
public:
    sdsl::bit_vector v;
    sdsl::bp_support_sada<> rich_bp;
    std::vector<int> indexes, indexes_inv;
    explicit LevelAncestorByBP(const std::vector<int> &parent_vec){
        int n = parent_vec.size();
        int root;
        indexes.resize(2 * n);
        indexes_inv.resize(2 * n);
        std::vector<std::vector<int>> tree(n);
        for(int i = 0; i < n; ++i){
            if(parent_vec[i] == -1){
                root = i;
                continue;
            }
            assert(0 <= parent_vec[i] && parent_vec[i] < n);
            tree[parent_vec[i]].emplace_back(i);
        }

        v = sdsl::bit_vector (2 * n, 0);

        int cnt = 0;
        std::stack<std::pair<int, bool>> stack;
        stack.emplace(root, false);
        stack.emplace(root, true);
        while(!stack.empty()){
            auto [x, fl] = stack.top();
            stack.pop();
            if(fl){
                indexes[x] = cnt;
                indexes_inv[cnt] = x;
                v[cnt] = fl;
                ++cnt;
                for(auto y : tree[x]){
                    stack.emplace(y, false);
                    stack.emplace(y, true);
                }
            }
            else{
                v[cnt] = fl;
                ++cnt;
            }
        }
        rich_bp = sdsl::bp_support_sada<>(&v);
        indexes.shrink_to_fit();
        indexes_inv.shrink_to_fit();
    }
    inline int get_anc(int x, unsigned int k) const override{
        return indexes_inv[rich_bp.level_anc(indexes[x], k)];
    }
    virtual std::uint64_t num_bytes() const{
        // about 2|V| words + 2|V| + o(n) bits
        std::uint64_t size;
        // size += (indexes.capacity() * sizeof(int) + sizeof(std::size_t) * 2);
        // size += (indexes_inv.capacity() * sizeof(int) + sizeof(std::size_t) * 2);
        std::ofstream out("/dev/null");
        size += v.serialize(out);
        size += rich_bp.serialize(out);
        return size;
    }
};

#endif //HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP
