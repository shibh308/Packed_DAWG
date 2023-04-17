#include <iostream>
#include <cassert>
#include <chrono>
#include <string>
#include <type_traits>
#include <random>
#include <vector>
#include <cxxabi.h>

#include <cstdio>
#include <cstdlib>


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
    text.shrink_to_fit();
    std::clog << "constructing...: " << text.size() << std::endl;
    Benchmark<Index> bench(text, data_path.substr(data_path.rfind('/') + 1), out_file);

    // constexpr int num_queries = 10000;
    // bench.run(num_queries, 10000);
    constexpr int num_queries = 100'000;
    // bench.run(num_queries, 100000);

    std::vector<int> pattern_lengthes = {
            1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 1000000
    };
    for(auto pattern_length : pattern_lengthes){
        bench.run(num_queries, pattern_length);
    }
}

template<typename... Indexes> requires (std::is_base_of_v<FullTextIndex, Indexes> && ...)
void bench(std::string data_path, std::ofstream& out_file){
    (_bench<Indexes>(data_path, out_file), ...);
}

template<typename Index> requires std::is_base_of_v<FullTextIndex, Index>
std::pair<Index, int> get_index(std::string data_path, int length_limit){
    std::clog << "loading: " << data_path << std::endl;
    std::ifstream file(data_path);
    assert(file.is_open());
    std::string text = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if(length_limit != -1){
        assert(length_limit <= text.length());
        text.resize(length_limit);
    }
    text.shrink_to_fit();
    Index index(text);
    return {std::move(index), text.length()};
    // return Index("text");
}

/*
long long process_size() {
    FILE* fp = std::fopen("/proc/self/statm", "r");
    long long tmp, vm;
    fscanf(fp, "%lld %lld ", &tmp, &vm);
    return vm * getpagesize();
}
 */

template<typename Index> requires std::is_base_of_v<FullTextIndex, Index>
void _bench_memory(std::string data_path, std::ofstream& out_file, int length_limit){
    std::cout << type_name<Index>() << std::endl;
    auto [index, text_length] = get_index<Index>(data_path, length_limit);
    std::string file_name = data_path.substr(data_path.rfind('/') + 1);
    out_file << type_name<Index>() << "," << file_name << "," << text_length << "," << index.num_bytes() << std::endl;
    std::clog << "length: " << text_length << std::endl;
    std::clog << "memory: " << index.num_bytes() / (1024.0 * 1024.0) << " [MiB]" << std::endl;
}

template<typename... Indexes> requires (std::is_base_of_v<FullTextIndex, Indexes> && ...)
void bench_memory(std::string data_path, std::ofstream& out_file, int length_limit){
    (_bench_memory<Indexes>(data_path, out_file, length_limit), ...);
}

template<typename K, typename V>
using MapType = BinarySearchMap<K, V>;

int main(int argc, char** argv){
    if(argc == 1){
        std::string out_file_path = "./data/output.txt";
        std::ofstream out_file(out_file_path);
        for(auto data_path : {
            "./data/english.10MiB",
            "./data/dna.10MiB",
            "./data/sources.10MiB",
        }){
            bench<
                    SimpleDAWG<MapType>,
                    HeavyTreeDAWGWithLABP<MapType>,
                    HeavyTreeDAWG<MapType>,
                    HeavyPathDAWG<MapType>
            >(data_path, out_file);
        }
    }
    else{
        std::string out_file_path = "./data/output_memory.txt";
        std::ofstream out_file(out_file_path, std::ios_base::app);
        assert(argc >= 3);
        std::string data_path;
        if(strcmp(argv[1], "english") == 0){
            data_path = "./data/english.10MiB";
        }
        else if(strcmp(argv[1], "dna") == 0){
            data_path = "./data/dna.10MiB";
        }
        else if(strcmp(argv[1], "sources") == 0){
            data_path = "./data/sources.10MiB";
        }
        assert(!data_path.empty());

        int length_limit = -1;
        if(argc >= 4){
            length_limit = atoi(argv[3]);
        }

        if(strcmp(argv[2], "Simple") == 0){
            bench_memory<SimpleDAWG<MapType>>(data_path, out_file, length_limit);
        }
        else if(strcmp(argv[2], "HeavyTree") == 0){
            bench_memory<HeavyTreeDAWG<MapType>>(data_path, out_file, length_limit);
        }
        else if(strcmp(argv[2], "HeavyTreeBP") == 0){
            bench_memory<HeavyTreeDAWGWithLABP<MapType>>(data_path, out_file, length_limit);
        }
        else if(strcmp(argv[2], "HeavyPath") == 0){
            bench_memory<HeavyPathDAWG<MapType>>(data_path, out_file, length_limit);
        }
    }
    return 0;
}
