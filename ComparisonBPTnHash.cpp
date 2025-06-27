#include <iostream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace std;
using namespace chrono;

const int ORDER = 4;

struct BPTreeNode {
    bool isLeaf;
    vector<int> keys;
    vector<BPTreeNode*> children;
    vector<int> values;
    BPTreeNode* next;

    BPTreeNode(bool leaf) : isLeaf(leaf), next(nullptr) {}
};

class BPTree {
private:
    BPTreeNode* root;
    void insertInternal(int key, int value, BPTreeNode* node, BPTreeNode* child);
    void splitLeaf(BPTreeNode* node, int key, int value);
    void splitInternal(BPTreeNode* node, int key, BPTreeNode* child);
    void deleteKey(BPTreeNode* node, int key);

public:
    BPTree() : root(new BPTreeNode(true)) {}
    void insert(int key, int value);
    int search(int key);
    void remove(int key);
    vector<pair<int, int>> searchRange(int start_key, int end_key);
    void traverseOrdered();
};

void BPTree::insert(int key, int value) {
    BPTreeNode* node = root;
    if (node->keys.empty()) {
        node->keys.push_back(key);
        node->values.push_back(value);
        return;
    }
    while (!node->isLeaf) {
        int i = 0;
        while (i < node->keys.size() && key > node->keys[i]) i++;
        node = node->children[i];
    }

    auto it = lower_bound(node->keys.begin(), node->keys.end(), key);
    int pos = it - node->keys.begin();
    node->keys.insert(it, key);
    node->values.insert(node->values.begin() + pos, value);

    if (node->keys.size() > ORDER - 1) {
        splitLeaf(node, key, value);
    }
}

void BPTree::splitLeaf(BPTreeNode* node, int key, int value) {
    BPTreeNode* newLeaf = new BPTreeNode(true);
    int mid = (ORDER + 1) / 2;
    newLeaf->keys.assign(node->keys.begin() + mid, node->keys.end());
    newLeaf->values.assign(node->values.begin() + mid, node->values.end());
    node->keys.resize(mid);
    node->values.resize(mid);

    newLeaf->next = node->next;
    node->next = newLeaf;

    if (node == root) {
        BPTreeNode* newRoot = new BPTreeNode(false);
        newRoot->keys.push_back(newLeaf->keys[0]);
        newRoot->children.push_back(node);
        newRoot->children.push_back(newLeaf);
        root = newRoot;
    } else {
        insertInternal(newLeaf->keys[0], 0, root, newLeaf);
    }
}

void BPTree::insertInternal(int key, int value, BPTreeNode* node, BPTreeNode* child) {
    if (node->isLeaf) return;
    if (node->children[0]->isLeaf) return; 
    int i = 0;
    while (i < node->keys.size() && key > node->keys[i]) i++;
    insertInternal(key, value, node->children[i], child);
    if (node->keys.size() > ORDER - 1) {
        splitInternal(node, key, child);
    }
}

void BPTree::splitInternal(BPTreeNode* node, int key, BPTreeNode* child) {
    BPTreeNode* newInternal = new BPTreeNode(false);
    int mid = node->keys.size() / 2;
    int upKey = node->keys[mid];

    newInternal->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    newInternal->children.assign(node->children.begin() + mid + 1, node->children.end());

    node->keys.resize(mid);
    node->children.resize(mid + 1);

    if (node == root) {
        BPTreeNode* newRoot = new BPTreeNode(false);
        newRoot->keys.push_back(upKey);
        newRoot->children.push_back(node);
        newRoot->children.push_back(newInternal);
        root = newRoot;
    }
}

int BPTree::search(int key) {
    BPTreeNode* node = root;
    while (!node->isLeaf) {
        int i = 0;
        while (i < node->keys.size() && key >= node->keys[i]) i++;
        node = node->children[i];
    }
    for (int i = 0; i < node->keys.size(); ++i) {
        if (node->keys[i] == key) return node->values[i];
    }
    return -1;
}

void BPTree::remove(int key) {
    deleteKey(root, key);
}

void BPTree::deleteKey(BPTreeNode* node, int key) {
    if (!node->isLeaf) return;
    for (size_t i = 0; i < node->keys.size(); ++i) {
        if (node->keys[i] == key) {
            node->keys.erase(node->keys.begin() + i);
            node->values.erase(node->values.begin() + i);
            return;
        }
    }
}

vector<pair<int, int>> BPTree::searchRange(int start_key, int end_key) {
    vector<pair<int, int>> result;
    BPTreeNode* node = root;

    while (!node->isLeaf) {
        int i = 0;
        while (i < node->keys.size() && start_key >= node->keys[i]) i++;
        node = node->children[i];
    }

    while (node != nullptr) {
        for (int i = 0; i < node->keys.size(); ++i) {
            if (node->keys[i] >= start_key && node->keys[i] <= end_key) {
                result.push_back({node->keys[i], node->values[i]});
            }
            if (node->keys[i] > end_key) {
                return result;
            }
        }
        node = node->next;
    }
    return result;
}

void BPTree::traverseOrdered() {
    BPTreeNode* node = root;
    if (root == nullptr || root->keys.empty()) return;

    while (!node->isLeaf) {
        node = node->children[0];
    }

    while (node != nullptr) {
        for (int i = 0; i < node->keys.size(); ++i) {
            volatile int key = node->keys[i];
            volatile int val = node->values[i];
        }
        node = node->next;
    }
}

long getMemoryUsageKB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memInfo;
    GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo));
    return memInfo.PeakWorkingSetSize / 1024;
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
#endif
}

void runExperiment(int dataSize, ofstream& csv) {
    vector<int> keys;
    for (int i = 0; i < dataSize; ++i) {
        keys.push_back(rand() % (dataSize * 2));
    }

    unordered_map<int, int> hashmap;
    auto startHashInsert = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        hashmap[keys[i]] = i;
    }
    auto endHashInsert = high_resolution_clock::now();

    auto startHashSearch = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        int val = hashmap[keys[i]];
    }
    auto endHashSearch = high_resolution_clock::now();

    BPTree bptree;
    auto startBPTInsert = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        bptree.insert(keys[i], i);
    }
    auto endBPTInsert = high_resolution_clock::now();

    auto startBPTSearch = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        int val = bptree.search(keys[i]);
    }
    auto endBPTSearch = high_resolution_clock::now();
    
    int start_range = dataSize / 2;
    int end_range = dataSize * 1.5;

    auto startHashRange = high_resolution_clock::now();
    vector<pair<int, int>> hash_range_results;
    for(const auto& pair : hashmap) {
        if (pair.first >= start_range && pair.first <= end_range) {
            hash_range_results.push_back(pair);
        }
    }
    auto endHashRange = high_resolution_clock::now();

    auto startBPTRange = high_resolution_clock::now();
    vector<pair<int, int>> bpt_range_results = bptree.searchRange(start_range, end_range);
    auto endBPTRange = high_resolution_clock::now();

    auto startHashOrdered = high_resolution_clock::now();
    vector<int> sorted_keys;
    sorted_keys.reserve(hashmap.size());
    for(const auto& pair : hashmap) {
        sorted_keys.push_back(pair.first);
    }
    sort(sorted_keys.begin(), sorted_keys.end());
    for(const int key : sorted_keys) {
        volatile int val = hashmap.at(key);
    }
    auto endHashOrdered = high_resolution_clock::now();

    auto startBPTOrdered = high_resolution_clock::now();
    bptree.traverseOrdered();
    auto endBPTOrdered = high_resolution_clock::now();

    auto startHashDelete = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        hashmap.erase(keys[i]);
    }
    auto endHashDelete = high_resolution_clock::now();
    
    auto startBPTDelete = high_resolution_clock::now();
    for (int i = 0; i < dataSize; ++i) {
        bptree.remove(keys[i]);
    }
    auto endBPTDelete = high_resolution_clock::now();

    auto hashInsertTime = duration_cast<microseconds>(endHashInsert - startHashInsert).count();
    auto hashSearchTime = duration_cast<microseconds>(endHashSearch - startHashSearch).count();
    auto hashDeleteTime = duration_cast<microseconds>(endHashDelete - startHashDelete).count();
    auto hashRangeTime = duration_cast<microseconds>(endHashRange - startHashRange).count();
    auto hashOrderedTime = duration_cast<microseconds>(endHashOrdered - startHashOrdered).count();
    
    auto bptInsertTime = duration_cast<microseconds>(endBPTInsert - startBPTInsert).count();
    auto bptSearchTime = duration_cast<microseconds>(endBPTSearch - startBPTSearch).count();
    auto bptDeleteTime = duration_cast<microseconds>(endBPTDelete - startBPTDelete).count();
    auto bptRangeTime = duration_cast<microseconds>(endBPTRange - startBPTRange).count();
    auto bptOrderedTime = duration_cast<microseconds>(endBPTOrdered - startBPTOrdered).count();

    long hashMemory = getMemoryUsageKB();
    long bptMemory = getMemoryUsageKB();

    cout << "Data Size: " << dataSize << endl;
    cout << "HashMap Insert Time: " << hashInsertTime << " us\n";
    cout << "HashMap Search Time: " << hashSearchTime << " us\n";
    cout << "HashMap Delete Time: " << hashDeleteTime << " us\n";
    cout << "HashMap Range Query Time: " << hashRangeTime << " us\n";
    cout << "HashMap Ordered Traversal Time: " << hashOrderedTime << " us\n";
    cout << "HashMap Memory: " << hashMemory << " KB\n";
    cout << "-------------------------------------------\n";
    cout << "B+ Tree Insert Time: " << bptInsertTime << " us\n";
    cout << "B+ Tree Search Time: " << bptSearchTime << " us\n";
    cout << "B+ Tree Delete Time: " << bptDeleteTime << " us\n";
    cout << "B+ Tree Range Query Time: " << bptRangeTime << " us\n";
    cout << "B+ Tree Ordered Traversal Time: " << bptOrderedTime << " us\n";
    cout << "B+ Tree Memory: " << bptMemory << " KB\n";
    cout << "===========================================\n\n";

    csv << dataSize << ","
        << hashInsertTime << "," << hashSearchTime << "," << hashDeleteTime << "," << hashRangeTime << "," << hashOrderedTime << "," << hashMemory << ","
        << bptInsertTime << "," << bptSearchTime << "," << bptDeleteTime << "," << bptRangeTime << "," << bptOrderedTime << "," << bptMemory << "\n";
}

int main() {
    srand(time(nullptr));
    ofstream csv("hasil_perbandingan.csv");
    csv << "DataSize,"
        << "HashMap_Insert(us),HashMap_Search(us),HashMap_Delete(us),HashMap_RangeQuery(us),HashMap_OrderedTraversal(us),HashMap_Memory(KB),"
        << "BPTree_Insert(us),BPTree_Search(us),BPTree_Delete(us),BPTree_RangeQuery(us),BPTree_OrderedTraversal(us),BPTree_Memory(KB)\n";
    
    runExperiment(100, csv);
    runExperiment(500, csv);
    runExperiment(1000, csv);
    
    csv.close();
    return 0;
}