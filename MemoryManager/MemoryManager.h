#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <bitset>
using namespace std;

int ceil_div(u_int16_t a, u_int16_t b);

u_int8_t mirrorByte(u_int8_t byte);

std::vector<u_int8_t> convertToBinary(const std::vector<std::pair<int, int>>& ranges, u_int16_t totalBytes);

std::vector<uint8_t> mirrorBytes(const std::vector<uint8_t>& bytes);

uint16_t flipBytes(uint16_t number);

struct Hole{
    u_int16_t mStart = -1;
    u_int16_t mOffset = -1;

    Hole* mPrev = nullptr;
    Hole* mNext = nullptr;

    void clearDescendants();
};

struct HoleList{
    Hole* mHead = nullptr;
    u_int16_t mCount = 0;
    void clear();

    void* getList();
    int insert(u_int16_t pos, u_int16_t offset); // Insert, inserts item, therefore removes shrinks hole.
    int remove(u_int16_t pos, u_int16_t offset); // Remove, removes item, therefore inserts hole.
    void print();

    ~HoleList();
};

struct Alloc{
    int mStart = -1;
    int mOffset = -1;
};

struct MemoryManager {
        MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator);
        ~MemoryManager();
        void initialize(size_t sizeInWords);
        void shutdown();
        void *allocate(size_t sizeInBytes);
        void free(void *address);
        void setAllocator(std::function<int(int, void *)> allocator);
        int dumpMemoryMap(char *filename);
        void *getList();
        void *getBitmap();
        unsigned getWordSize();
        void *getMemoryStart();
        unsigned getMemoryLimit();

    private:
        // I set default values here.
        const unsigned mWordSizeInBytes; // Dont change this after construction.
        std::function<int(int, void *)> allocatorFunc = nullptr;
        u_int16_t mMemorySizeInWords = -1;
        u_int8_t* mMemoryList = nullptr;
        unordered_map<u_int8_t*, Alloc> mAllocatedBlockMap; // keeps track of allocated blocks to faciliatate removes.
        HoleList mHoleList; // The head of the linked list.
};

int bestFit(int sizeInWords, void *list);
int worstFit(int sizeInWords, void *list);