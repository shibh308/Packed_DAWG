#include <iostream>
#include <fstream>
#include <cassert>
#include <chrono>
#include <string>
#include <type_traits>
#include <random>
#include <vector>
#include <cxxabi.h>


#include "includes/full_text_index.hpp"
#include "includes/dawg.hpp"


template <typename T> std::string type_name(){
    std::string name = typeid(T).name();
    char* demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, nullptr);
    name = demangled;
    std::free(demangled);
    return name;
}

template<typename Index> requires std::is_base_of_v<FullTextIndex, Index>
struct Benchmark {

    std::string file_name;
    std::string_view text_view;
    Index index;
    int text_length, seed;
    std::ofstream& out_file;

    void benchmark_text(const std::vector<int>& pattern_poses, int pattern_length){
        std::clog << "matching..." << std::endl;
        // pattern matching
        auto elapsed = std::chrono::duration<int64_t, std::nano>::zero();
        for(auto l : pattern_poses){
            std::string_view pattern = text_view.substr(l, pattern_length);
            auto start = std::chrono::high_resolution_clock::now();
            auto result = index.get_node(pattern);
            auto end = std::chrono::high_resolution_clock::now();
            elapsed += end - start;
            assert(result.has_value());
        }

        // TODO: check correctness
        double time_sec = elapsed.count() / 1'000'000'000.0;
        std::clog << "elapsed time: " << time_sec << "[sec]" << std::endl;
        out_file << type_name<Index>() << "," << file_name << "," << text_length << "," << pattern_poses.size() << "," << pattern_length << "," << elapsed.count() << std::endl;
        std::clog << std::endl;
    }

    // template<typename DAWGBasedIndex> requires std::is_base_of_v<FullTextIndex, DAWGBasedIndex> && std::is_constructible_v<DAWGBasedIndex, const DAWGBase&>

public:
    explicit Benchmark(std::string_view text_view, std::string file_name, std::ofstream& out_file, int seed = 0) : file_name(std::move(file_name)), seed(seed), text_view(text_view), out_file(out_file), index(text_view), text_length(text_view.length()){
        std::clog << type_name<Index>() << " construct end" << std::endl;
        assert(out_file.is_open());
    }

    void run(int num_queries, int pattern_length){
        // generate patterns
        std::mt19937 gen(seed);
        assert(pattern_length <= text_length);
        std::uniform_int_distribution<int> dist(0, text_length - pattern_length);

        std::clog << "text_length   : " << text_length << std::endl;
        std::clog << "num_queries   : " << num_queries << std::endl;
        std::clog << "pattern_length: " << pattern_length << std::endl;

        std::vector<int> pattern_poses(num_queries);
        for(int i = 0; i < num_queries; ++i){
            pattern_poses[i] = dist(gen);
        }

        benchmark_text(pattern_poses, pattern_length);
    }
};


template<typename Index> requires std::is_base_of_v<FullTextIndex, Index>
void _bench(std::string data_path, std::ofstream& out_file){
    std::clog << "loading: " << data_path << std::endl;
    std::ifstream file(data_path);
    assert(file.is_open());
    std::string text = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::clog << "constructing..." << std::endl;
    Benchmark<Index> bench(text, data_path.substr(data_path.rfind('/') + 1), out_file);

    constexpr int num_queries = 10000;
    bench.run(num_queries, 10000);

    // for(int pattern_length = 10; pattern_length <= 10'000; pattern_length *= 10){
        // bench.run(num_queries, pattern_length);
    // }
}

template<typename... Indexes> requires (std::is_base_of_v<FullTextIndex, Indexes> && ...)
void bench(std::string data_path, std::ofstream& out_file){
    (_bench<Indexes>(data_path, out_file), ...);
}

template<typename K, typename V>
using MapType = BinarySearchMap<K, V>;

int main(){
    std::string out_file_path = "./data/output.txt";
    for(auto data_path : {
        "./data/english",
        "./data/sources",
        "./data/dna",
    }){
        std::ofstream out_file(out_file_path);
        bench<
                SimpleDAWG<MapType>,
                HeavyTreeDAWG<MapType>,
                HeavyTreeDAWGWithLA<MapType, LevelAncestorByLadder>,
                HeavyTreeDAWGWithLA<MapType, LevelAncestorByBP>,
                HeavyPathDAWG<MapType>
        >(data_path, out_file);
    }
    return 0;
}
