#ifndef HEAVY_TREE_DAWG_DAWG_HPP
#define HEAVY_TREE_DAWG_DAWG_HPP

#include <vector>
#include <string>
#include <map>

#include "full_text_index.hpp"
#include "ordered_map.hpp"

struct DAWGBase{
    struct Node{
        std::map<unsigned char, int> ch;
        int slink, len;
        unsigned char c;

        Node(int len, unsigned char c) : slink(-1), len(len), c(c){}
    };

    std::vector<Node> nodes;
    std::vector<int> node_ids;

    explicit DAWGBase(std::string_view text){
        nodes.emplace_back(0, 0);
        for(int i = 0; i < text.size(); ++i){
            add_node(i, text[i]);
        }
    }

    void add_node(int i, unsigned char c){
        int new_node = nodes.size();
        int target_node = (nodes.size() == 1 ? 0 : node_ids.back());
        node_ids.emplace_back(new_node);
        nodes.emplace_back(i + 1, c);

        for(; target_node != -1 &&
              nodes[target_node].ch.find(c) == nodes[target_node].ch.end(); target_node = nodes[target_node].slink){
            nodes[target_node].ch[c] = new_node;
        }
        if(target_node == -1){
            nodes[new_node].slink = 0;
        }else{
            int sp_node = nodes[target_node].ch[c];
            if(nodes[target_node].len + 1 == nodes[sp_node].len){
                nodes[new_node].slink = sp_node;
            }else{
                int clone_node = nodes.size();
                nodes.emplace_back(nodes[target_node].len + 1, nodes[sp_node].c);
                nodes[clone_node].ch = nodes[sp_node].ch;
                nodes[clone_node].slink = nodes[sp_node].slink;
                for(; target_node != -1 && nodes[target_node].ch.find(c) != nodes[target_node].ch.end() &&
                      nodes[target_node].ch[c] == sp_node; target_node = nodes[target_node].slink){
                    nodes[target_node].ch[c] = clone_node;
                }
                nodes[sp_node].slink = nodes[new_node].slink = clone_node;
            }
        }
    }
};


class SimpleDAWG : FullTextIndex {
    std::vector<BinarySearchMap<unsigned char, int>> children;
public:
    explicit SimpleDAWG(std::string_view text) {
        DAWGBase base(text);
        for(auto& node : base.nodes){
            children.emplace_back(node.ch);
        }
    }
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


#endif //HEAVY_TREE_DAWG_DAWG_HPP
