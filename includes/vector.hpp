//
// Created by shibh308 on 4/13/23.
//

#ifndef PACKED_DAWG_VECTOR_HPP
#define PACKED_DAWG_VECTOR_HPP

#include <memory>
#include <cstdint>

template<typename value_type, typename size_type>
class Vector{
    std::unique_ptr<value_type[]> pointer;
    size_type _size;
public:
    explicit Vector(){
        _size = 0;
        pointer = std::make_unique<value_type[]>(0);
    }
    Vector(const std::vector<value_type>& vector){
        _size = vector.size();
        pointer = std::make_unique<value_type[]>(_size);
        for(int i = 0; i < _size; ++i){
            pointer[i] = vector[i];
        }
    }
    Vector(const Vector& other){
        _size = other._size;
        pointer = std::make_unique<value_type[]>(_size);
        for(int i = 0; i < _size; ++i){
            pointer[i] = other.pointer[i];
        }
    }
    ~Vector() = default;
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            Vector tmp(other);
            std::swap(tmp._size, _size);
            std::swap(tmp.pointer, pointer);
        }
        return *this;
    }
    value_type& operator[](std::size_t index){
        return pointer[index];
    }
    const value_type& operator[](std::size_t index) const{
        return pointer[index];
    }
    static constexpr std::uint64_t offset_bytes = sizeof(value_type*) + sizeof(size_type);
    size_type size() const{
        return _size;
    }
    std::uint64_t num_bytes() const{
        return offset_bytes + sizeof(value_type) * _size;
    }
};


#endif //PACKED_DAWG_VECTOR_HPP
