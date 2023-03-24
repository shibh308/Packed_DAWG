#include <iostream>
#include <fstream>
#include <cassert>
#include <chrono>
#include <string>
#include <random>
#include <vector>
#include <optional>


#include "includes/full_text_index.hpp"
#include "includes/dawg.hpp"



template<typename Index> requires std::is_base_of_v<FullTextIndex, Index>
void benchmark_text(const Index& index, std::string_view text){
    int text_length = text.length();

    // generate patterns
    constexpr int seed = 0;
    constexpr int num_queries = 10000;
    constexpr int pattern_length = 10000;
    std::mt19937 gen(seed);
    assert(pattern_length <= text_length);
    std::uniform_int_distribution<int> dist(0, text_length - pattern_length);

    std::vector<std::string_view> patterns(num_queries);
    for(int i = 0; i < num_queries; ++i){
        int left = dist(gen);
        patterns[i] = text.substr(left, pattern_length);
    }

    std::clog << "matching..." << std::endl;
    // pattern matching
    auto elapsed = std::chrono::duration<int64_t, std::nano>::zero();
    for(int i = 0; i < num_queries; ++i){
        auto start = std::chrono::high_resolution_clock::now();
        auto result = index.get_node(patterns[i]);
        auto end = std::chrono::high_resolution_clock::now();
        elapsed += end - start;
        assert(result.has_value());
    }
    // TODO: check correctness
    double time_sec = elapsed.count() / 1'000'000'000.0;
    std::clog << "elapsed time: " << time_sec << "[sec]" << std::endl;
}

void benchmark(const std::string& file_path){
    std::clog << "loading: " << file_path << std::endl;
    std::ifstream file(file_path);
    assert(file.is_open());
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::clog << "file length: " << text.length() << std::endl;
    std::clog << "constructing..." << std::endl;
    SimpleDAWG index(text);
    benchmark_text<SimpleDAWG>(index, text);
}


int main(){
    benchmark("./data/dna.1MB");
    return 0;
}
