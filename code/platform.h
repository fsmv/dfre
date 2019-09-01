#ifndef PLATFORM_H_

#include <stddef.h>
#include <stdint.h> // int32_t etc

// Must be extern "C" for for the *nix _start assembly code
extern "C" int main(int argc, char *argv[]);

// Write to stdout and return the number of characters written
uint32_t Write(const char *Str, size_t Len);
// Exit the current process with the given status code
void Exit(int Code);

// Minimum size of a memory block that can be committed or reserved.
// Blocks are aligned to addresses that are multiples of PAGE_SIZE.
extern const size_t PAGE_SIZE;
// Minimum size of a memory block that can be reserved.
// Reservations are aligned to address that are multiples of RESERVE_GRANULARITY
extern const size_t RESERVE_GRANULARITY;

// Reserve address space without requiring physical memory backing it.
void *Reserve(void *addr, size_t size);
// Commit to requiring physical memory to back this reserved address space.
// The memory is initialized to zero.
bool Commit(void *addr, size_t size);
// Deallocate any pages in the range. Uncommits and Unreserves.
// addr and size must be muliple of PAGE_SIZE
//
// Free must be called on the entire result of each Reserve call (even if a
// contiguous range was created from multiple Reserve calls). This is due to
// Windows having this restriction, even though POSIX can Free any arbitary
// range of pages.
void Free(void *addr, size_t size);

// Load the code bytes into an executable page and then mark it read-only
// Returns the location of the executable page to jump to
//
// If the code supports the C calling convention you can cast the pointer to an
// extern "C" function pointer and call it.
void *LoadCode(uint8_t *Code, size_t CodeWritten);

#define PLATFORM_H_
#endif
