//
// Created by shibh308 on 4/8/23.
//

#ifndef HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP
#define HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP

#include "sdsl/bp_support.hpp"


class LevelAncestorByBP{
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
        for(int i = 1; i < 10; ++i){
            if(v[i] == 1){
                rich_bp.level_anc(i, 0);
                rich_bp.level_anc(i, 1);
            }
        }
    }
    std::optional<int> get_anc(int x, unsigned int k) const{
        int y = rich_bp.level_anc(indexes[x], k);
        return indexes_inv[y];
    }
};

#endif //HEAVY_TREE_DAWG_LEVEL_ANCESTOR_HPP
