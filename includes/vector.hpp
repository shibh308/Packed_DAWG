//
// Created by shibh308 on 4/13/23.
//

#ifndef PACKED_DAWG_VECTOR_HPP
#define PACKED_DAWG_VECTOR_HPP

#include <cstdint>

template<typename value_type, typename size_type>
class Vector{
public:
    static constexpr std::uint64_t offset_width = sizeof(std::size_t) + sizeof(size_type);
    static std::uint64_t vec_size(unsigned int vec_size){
        return offset_width + sizeof(value_type) * vec_size;
    }
};


#endif //PACKED_DAWG_VECTOR_HPP
