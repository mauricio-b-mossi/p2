#include "MemoryManager.h"
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>    
#include <sys/types.h>
#include <sys/stat.h> 
#include <cstring>    
#include <sstream>
/*
Memory tracking will be done with Hole Linked List. 
Allocations will be tracked with a map(ptr, Alloc)
Memory will be added to the chunk accordingly.

On allocate, check if there is space in chunk (in bytes). Just a wrapper for bestFit and worstFit.

Just mutate, memoryList -> nullptr, memorySizeInWords -> c, AllocatedBlockMap, and head.
*/

void Hole::clearDescendants(){
    if (this->mNext != nullptr) {
      this->mNext->clearDescendants(); 
      delete this->mNext;
      this->mNext = nullptr;
    }
}

void HoleList::print(){
  void *l = this->getList();
  if (l == nullptr) {
    std::cout << "empty, getList -> nullptr" << std::endl;
    return;
  }
  u_int16_t *nl = (u_int16_t *)l;
  u_int16_t count = nl[0];
  std::cout << "[" << count << ", ";
  for (int i = 1; i < count * 2 + 1; i++) {
    if (i < count * 2) {
      std::cout << nl[i] << ", ";
    } else {
      std::cout << nl[i] << "]" << std::endl;
    }
  }
  delete[] nl;
}

// HoleList counts and produces list, insert if allocate is successful.
void HoleList::clear(){
    if(mHead == nullptr){
        return;
    }
    mHead->clearDescendants();
    delete mHead;
    mHead = nullptr;
}

HoleList::~HoleList(){
    if(mHead == nullptr){
        return;
    }
    this->clear();
}

// Both are valid by default. Since they are just repr of what other funcs do.
int HoleList::insert(u_int16_t pos, u_int16_t offset){
  auto curr = this->mHead;
  if (curr == nullptr) {
    return -1; // There must be atleast 1 node. This is an invariant.
  }
  while (curr->mStart + curr->mOffset < pos) {
    curr = curr->mNext;
  }
  // Error will be thrown if not found, should not happen, since best fit finds.
  // Due to nullptr deref

  if (curr->mStart == pos) {
    // Case where hole == allocated amount, decrement mCount, deallocate block,
    // set others accordingly.
    if (curr->mOffset == offset) {
      this->mCount--;
      if (curr == this->mHead) { // In case curr is head. alt prev == nullptr
        this->mHead = curr->mNext;
        if (this->mHead != nullptr) {
          this->mHead->mPrev = nullptr;
        }
      } else {
        curr->mPrev->mNext =
            curr->mNext; // If not head, just remove, by setting prev and next
        if (curr->mNext != nullptr) {
          curr->mNext->mPrev = curr->mPrev;
        }
      }
      delete curr;
      return pos;
    }

    // Not end at same place, just shift hole right. No count ++ since no hole
    // was created. And no del since no hole was romeved.
    curr->mStart = pos + offset; // Remember these represent holes.
    curr->mOffset = curr->mOffset - offset;
    return pos;
  }

  // Case where alloc in between the block. Create new block. If offsets do not
  // match.
  u_int16_t off = curr->mOffset;
  curr->mOffset = pos - curr->mStart;
  if (off - curr->mOffset > offset) {
    curr->mNext =
        new Hole{(u_int16_t)(pos + offset),
                 (u_int16_t)(off - curr->mOffset - offset), curr, curr->mNext};
    this->mCount++;
  }
  return pos;

}

int HoleList::remove(u_int16_t pos, u_int16_t offset) {
  std::cout << pos << " " << offset << std::endl;
  if (this->mHead == nullptr || this->mCount == 0) {
    this->mHead = new Hole{pos, offset};
    this->mCount++;
    return pos;
  }
  // I know it is not null.
  auto curr = this->mHead;
  bool last = false;
  while (curr->mStart < pos && !last) {
    if (curr->mNext == nullptr) {
      last = true;
    } else {
      curr = curr->mNext;
    }
  }
  /* Either last or not.
   * If last can merge (start + off = pos)
   * else can create new.
   */
  if (last) {
    std::cout << "Last" << std::endl;
    if (curr->mStart + curr->mOffset == pos) {
      curr->mOffset = curr->mOffset + offset;
      return curr->mStart;
    }

    curr->mNext = new Hole{pos, offset, curr};
    this->mCount++;
    return pos;
  }

  // Not last cases.

  // Case where first is Head.
  if (curr == this->mHead) {         // Before head
  std::cout << "Head" << std::endl;
    if (curr->mStart == pos + offset) { // Merging case.
      curr->mStart = pos;
      curr->mOffset = curr->mOffset + offset;
      return pos;
    }

    // not merging case.
    std::cout << "not merging head" << std::endl;
    curr->mPrev = new Hole{pos, offset, nullptr, curr};
    this->mHead = curr->mPrev;
    this->mCount++;
    return pos;
  }

  // Middle cases 4
  
  // Same start, merge left.
  if(curr->mPrev->mStart + curr->mPrev->mOffset == pos){
      if(curr->mStart == pos + offset){ // Merging cases
        std::cout << "Merge left case" << std::endl;
        curr->mPrev->mOffset = curr->mPrev->mOffset + offset + curr->mOffset;
        curr->mPrev->mNext = curr->mNext;
        u_int16_t s = curr->mPrev->mStart;
        delete curr;
        this->mCount--;
        return s;
      }

      std::cout << "left append" << std::endl;
      curr->mPrev->mOffset = curr->mPrev->mOffset + offset;
      return curr->mPrev->mStart;
  }

  // Same end, merge right
  if(curr->mStart == pos + offset){
    std::cout << "Merge rigth case" << std::endl;
      curr->mStart = pos;
      curr->mOffset = curr->mOffset + offset;
      return curr->mStart;
  }

  std::cout << "Middle case" << std::endl;
  // Case in the middle, no adjacent.
  curr->mPrev = new Hole{pos, offset, curr->mPrev, curr};
  curr->mPrev->mPrev->mNext = curr->mPrev;
  this->mCount++;
  return pos; 
}

void *HoleList::getList(){
    if(this->mCount == 0){
        return nullptr;
    }
    u_int16_t *list = new u_int16_t[mCount * 2 + 1];
    list[0] = this->mCount;
    auto curr = this->mHead;
    for(int i = 1; i < mCount + 1; i++){
        list[2* i - 1] = curr->mStart;
        list[2 * i] = curr->mOffset;
        curr = curr->mNext;
    }
    return list;
}

MemoryManager::MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator) : mWordSizeInBytes{wordSize}, allocatorFunc{allocator}{
}

MemoryManager::~MemoryManager(){
    this->shutdown();
}

void MemoryManager::initialize(size_t sizeInWords){
    if(sizeInWords > 65535){ // Weird spec says 65536 but UINT16_MAX is 65535.
        return; // Fail silently...
    }
    this->shutdown();
    this->mMemorySizeInWords = sizeInWords;
    this->mMemoryList = new u_int8_t[this->mMemorySizeInWords * this->mWordSizeInBytes];
    this->mHoleList.mHead = new Hole{0, (u_int16_t)(this->mMemorySizeInWords)}; // hole list only stores blocks and words not bytes.
    this->mHoleList.mCount++; // This think.
    this->mHoleList.print();
}

void MemoryManager::shutdown(){
    if(this->mMemoryList == nullptr){
        return; // Fail silently...
    }
    delete[] mMemoryList; 
    mMemoryList = nullptr;
    this->mMemorySizeInWords = -1;
    // TODO clean entry map!
    this->mAllocatedBlockMap.clear();
    this->mHoleList.clear();
}

void *MemoryManager::allocate(size_t sizeInBytes){
    if(this->mMemoryList == nullptr){
        return nullptr;
    }
    u_int16_t wordsLen = ceil_div(sizeInBytes, this->mWordSizeInBytes);

    if(wordsLen > this->mMemorySizeInWords){
        return nullptr;
    }

    void* l = this->getList();
    if(l == nullptr){
        return nullptr; // This means no space left.
    }
    u_int16_t* list = (u_int16_t*)l;
    int wordOffset = allocatorFunc(wordsLen, list);
    delete[] list;
    l = nullptr;
    list = nullptr;
    if(wordOffset == -1){
        return nullptr;
    }
    // Insert block
    this->mHoleList.insert(wordOffset, wordsLen);
    this->mHoleList.print();
    u_int8_t* startAddr = this->mMemoryList + wordOffset * this->mWordSizeInBytes;
    this->mAllocatedBlockMap.emplace(startAddr, Alloc{wordOffset, wordsLen}); //

    return startAddr;
}

void MemoryManager::free(void* address){
  auto it = this->mAllocatedBlockMap.find((u_int8_t*)address);
  if(it == this->mAllocatedBlockMap.end()){
    return; // error silently ...
  }
  this->mHoleList.remove(it->second.mStart, it->second.mOffset);
  this->mHoleList.print();
  this->mAllocatedBlockMap.erase((u_int8_t*)address);
}

void MemoryManager::setAllocator(std::function<int(int, void *)> allocator){
    this->allocatorFunc = allocator;
}

void *MemoryManager::getList(){
    if(this->mMemoryList == nullptr){
        return nullptr;
    }
    return this->mHoleList.getList();
}


int MemoryManager::dumpMemoryMap(char* filename){
 int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) {
        return -1;
    }

    // Convert the list of holes to a string
    std::ostringstream oss;

    void*l = this->getList();
    if(l == nullptr){
      return -1;
    }

    u_int16_t *nl = (u_int16_t *)l;
    u_int16_t count = nl[0];

    oss << "[" << count << ", ";
    for(int i = 1; i < count * 2 + 1; i++){
      if (i < count * 2) {
        std::cout << nl[i] << ", ";
      } else {
        std::cout << nl[i] << "]";
      }
    }

    delete[] nl;
    nl = nullptr;
    l = nullptr;

    std::string data = oss.str();

    ssize_t written = write(fd, data.c_str(), data.size());
    if (written == -1) {
      return -1;
    }

    if (close(fd) == -1) {
      return -1;
    }

    return 0;
};

void *MemoryManager::getBitmap(){
  void*l = this->getList(); // CLEAN
  if(l == nullptr && this->mMemoryList == nullptr){ // Handle full case.
    return nullptr;
  }
  std::vector<std::pair<int, int>> ranges ={};
  if(l != nullptr){
    u_int16_t *nl = (u_int16_t *)l;
    u_int16_t count = nl[0];
    for(int i = 1; i < count * 2 + 1; i++){
      ranges.push_back({nl[i], nl[i+1]});
    }
    delete[] nl;
  }
  l = nullptr;
  std::vector<u_int8_t> res = convertToBinary(ranges, this->mMemorySizeInWords);
  std::vector<u_int8_t> mirrRes = mirrorBytes(res);

  uint8_t lowerByte = this->mMemorySizeInWords & 0x00FF; // mask lower byte

  uint8_t upperByte = (this->mMemorySizeInWords >> 8) & 0x00FF; // shift right to get the upper byte

  u_int8_t* result = new u_int8_t[this->mMemorySizeInWords + 2];

  result[0] = lowerByte;
  result[1] = upperByte;
  for(int i = 2; i < this->mMemorySizeInWords + 2; i++){
    result[i] = mirrRes[i - 2];
  }

  return result;
}

unsigned MemoryManager::getWordSize(){
    return this->mWordSizeInBytes;
}

void *MemoryManager::getMemoryStart(){
    return this->mMemoryList;
}

unsigned MemoryManager::getMemoryLimit(){
    return this->mWordSizeInBytes * this->mMemorySizeInWords;
}

int bestFit(int sizeInWords, void *list){ // Error code is -1
    if (list == nullptr) {
        return -1;
    }

    u_int16_t* holeList = (u_int16_t*)list;
    u_int16_t numHoles = holeList[0];
    int offset = -1; // Default error state.
    int minSpaceRem = UINT16_MAX;

    for (int i = 1; i < numHoles * 2; i += 2) {
        auto currSpaceLeft = holeList[i + 1] - sizeInWords;
        if (currSpaceLeft >= 0 && currSpaceLeft < minSpaceRem) {
            minSpaceRem = currSpaceLeft;
            offset = holeList[i];
        }
    }

    //delete[] holeList; double free
    return offset;
};

int worstFit(int sizeInWords, void *list){
    if (list == nullptr) {
        return -1;
    }

    u_int16_t* holeList = (u_int16_t*)list;
    u_int16_t numHoles = holeList[0];
    int offset = -1; // Default error state.
    int maxSpaceRem = UINT16_MAX;

    for (int i = 1; i < numHoles * 2; i += 2) {
        auto currSpaceLeft = holeList[i + 1] - sizeInWords;
        if (currSpaceLeft >= 0 && currSpaceLeft < maxSpaceRem) {
            maxSpaceRem = currSpaceLeft;
            offset = holeList[i];
        }
    }

    //delete[] holeList; Double free...
    return offset;
};

// Utils.
int ceil_div(u_int16_t a, u_int16_t b) {
    return (a + b - 1) / b;
}

uint8_t mirrorByte(uint8_t byte) {
    uint8_t mirrored = 0; // initialize mirrored byte
    for (int i = 0; i < 8; ++i) {
        mirrored |= ((byte >> i) & 1) << (7 - i); // shift bits from original byte into mirrored byte
    }
    return mirrored;
}

std::vector<uint8_t> convertToBinary(const std::vector<std::pair<int, int>>& ranges, u_int16_t totalBytes) {
    const int totalBits = totalBytes * 8; // total bits based on number of bytes
    std::vector<uint8_t> bytes(totalBytes, 0xFF); // initialize all bytes to 1 (0xFF)

    for (const auto& range : ranges) {
        int start = range.first; // starting index
        int length = range.second; // number of bits to set to 0

        for (int i = start; i < start + length; i++) {
            if (i < totalBits) { // ensure we dont exceed the total number of bits
                int byteIndex = i / 8; // determine which byte
                int bitIndex = 7 - (i % 8); // determine which bit in that byte 0 7
                bytes[byteIndex] &= ~(1 << bitIndex); // set the bit to 0
            }
        }
    }

    return bytes;
}

std::vector<uint8_t> mirrorBytes(const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> mirroredBytes;
    for (uint8_t byte : bytes) {
        mirroredBytes.push_back(mirrorByte(byte)); // mirror each byte
    }
    return mirroredBytes;
}

uint16_t flipBytes(uint16_t number) {
    uint8_t lowerByte = number & 0x00FF; // mask lower byte

    uint8_t upperByte = (number >> 8) & 0x00FF; // shift right to get the upper byte

    return (lowerByte << 8) | upperByte; // combine
}