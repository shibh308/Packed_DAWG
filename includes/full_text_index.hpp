//
// Created by shibh308 on 3/24/23.
//

#ifndef HEAVY_TREE_DAWG_FULL_TEXT_INDEX_HPP
#define HEAVY_TREE_DAWG_FULL_TEXT_INDEX_HPP

#include <vector>
#include <string_view>
#include <optional>

class FullTextIndex {
    virtual std::optional<int> get_node(std::string_view pattern) const = 0;
};

#endif //HEAVY_TREE_DAWG_FULL_TEXT_INDEX_HPP
