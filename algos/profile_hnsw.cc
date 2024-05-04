#include "lib/hnswlib.h"
#include "utils.h"

#define MAX_ELEMENTS 10e6

int main(int argc, char **argv) {
    if (argc < 5) {
        std::cout << "usage: " << argv[0] << " [index (hnsw/brute/hnsw_recall)] " << "[dataset (siftsmall/sift/gist/bigann)] " << "[operation (index/query)]" << " [top_k]" << std::endl;
    }

    std::string index = argv[1];
    std::string dataset = argv[2];
    std::string operation = argv[3];
    int top_k = std::stoi(argv[4]);
    print_pid();

    std::cout << "[ARG] index: " << index << std::endl;
    std::cout << "[ARG] dataset: " << dataset << std::endl;
    std::cout << "[ARG] operation: " << operation << std::endl;
    std::cout << "[ARG] top_k: " << top_k << std::endl;

    int M = 2<<4;
    int ef_construction = 200;

    if (operation == "index") {
        size_t dim_learn, n_learn;
        float* data_learn;
        std::string dataset_path_learn = dataset + "/" + dataset + "_base.fvecs";
        read_dataset(dataset_path_learn.c_str(), data_learn, &dim_learn, &n_learn);
        std::cout << "[INFO] learn dataset shape: " << dim_learn << " x " << n_learn << std::endl;
        
        hnswlib::L2Space space(dim_learn);

        if (index == "hnsw" || index == "hnsw_recall") {
            std::cout << "[INFO] performing hnsw indexing" << std::endl;
            hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, n_learn, M, ef_construction);

            auto s = std::chrono::high_resolution_clock::now();
            #pragma omp parallel for
            for (int i = 0; i < n_learn; i++) {
                alg_hnsw->addPoint(data_learn + i * dim_learn, i);
            }
            auto e = std::chrono::high_resolution_clock::now();
            std::cout << "[TIME] hnsw_index: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms" << std::endl;

            std::string hnsw_path = "index." + dataset + ".hnswlib";
            alg_hnsw->saveIndex(hnsw_path);
            std::cout << "[FILESIZE] hnsw_index_size: " << alg_hnsw->indexFileSize() << " bytes" << std::endl;
            
            delete alg_hnsw;
        }

        if (index == "brute" || index == "hnsw_recall") {
            std::cout << "[INFO] performing bruteforce indexing" << std::endl;
            hnswlib::BruteforceSearch<float>* alg_brute = new hnswlib::BruteforceSearch<float>(&space, MAX_ELEMENTS);

            auto s = std::chrono::high_resolution_clock::now();
            #pragma omp parallel for
            for (int i = 0; i < n_learn; i++) {
                alg_brute->addPoint(data_learn + i * dim_learn, i);
            }
            auto e = std::chrono::high_resolution_clock::now();
            std::cout << "[TIME] brute_force_index: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms" << std::endl;

            std::string brute_path = "index." + dataset + ".bruteforce";
            alg_brute->saveIndex(brute_path);
            std::cout << "[FILESIZE] brute_force_index_size: " << filesize(brute_path.c_str()) << " bytes" << std::endl;

            delete alg_brute;
        }
        
        delete[] data_learn;
    }

    if (operation == "query") {
        size_t dim_query, n_query;
        float* data_query;
        std::string dataset_path_query = dataset + "/" + dataset + "_learn.fvecs";
        read_dataset(dataset_path_query.c_str(), data_query, &dim_query, &n_query);
        std::cout << "[INFO] query dataset shape: " << dim_query << " x " << n_query << std::endl;

        n_query = 1000;

        std::unordered_map<int, std::vector<int>> results_hnsw_map(n_query);
        std::unordered_map<int, std::vector<int>> results_brute_map(n_query);
        for (int i = 0; i < n_query; i++) {
            results_hnsw_map[i] = std::vector<int>(top_k, 0);
            results_brute_map[i] = std::vector<int>(top_k, 0);
        }

        hnswlib::L2Space space(dim_query);

        if (index == "hnsw" || index == "hnsw_recall") {
            std::string hnsw_path = "index." + dataset + ".hnswlib";
            hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, hnsw_path);

            auto s = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < n_query; i++) {
                std::priority_queue<std::pair<float, hnswlib::labeltype>> result_hnsw = alg_hnsw->searchKnn(data_query + i * dim_query, top_k);
                if (index == "hnsw_recall") {
                    std::cout << "[INFO] saving kNN result for recall calculation" << std::endl;
                    for (int j = 0; j < top_k; j++) {
                        results_hnsw_map[i][j] = result_hnsw.top().second;
                        result_hnsw.pop();
                    }
                    assert(results_hnsw_map[i].size() == top_k);
                }
            }
            auto e = std::chrono::high_resolution_clock::now();
            std::cout << "[TIME] query_hnsw: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms" << std::endl;

            delete alg_hnsw;
        }

        if (index == "brute" || index == "hnsw_recall") {
            std::string brute_path = "index." + dataset + ".bruteforce";
            hnswlib::BruteforceSearch<float>* alg_brute = new hnswlib::BruteforceSearch<float>(&space, brute_path);

            auto s = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < n_query; i++) {
                std::priority_queue<std::pair<float, hnswlib::labeltype>> result_brute = alg_brute->searchKnn(data_query + i * dim_query, top_k);
                if (index == "hnsw_recall") {
                    std::cout << "[INFO] saving kNN result for recall calculation" << std::endl;
                    for (int j = 0; j < top_k; j++) {
                        results_brute_map[i][j] = result_brute.top().second;
                        result_brute.pop();
                    }
                    assert(results_brute_map[i].size() == top_k);
                }
            }
            auto e = std::chrono::high_resolution_clock::now();
            std::cout << "[TIME] query_brute: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms" << std::endl;

            delete alg_brute;
        }

        delete[] data_query;

        if (index == "hnsw_recall") {
            std::cout << "[INFO] calculating recall@" << top_k << std::endl;
            std::vector<double> recalls(n_query);
            for (int i = 0; i < n_query; i++) {
                auto v1 = results_brute_map[i];
                auto v2 = results_hnsw_map[i];
                int correct = 0;            
                for (int j = 0; j < v2.size(); j++) {
                    if (std::find(v1.begin(), v1.end(), v2[j]) != v1.end()) {
                        correct++;
                    }
                }
                recalls[i] = (float)correct / top_k;         
            }
            assert(recalls.size() == n_query);
            std::cout << "[RECALL] mean recall@" << top_k << ": " << std::accumulate(recalls.begin(), recalls.end(), 0.0) / recalls.size() << std::endl;
        }
    }
    
    return 0;
}
