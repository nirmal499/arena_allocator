#include <iostream>
#include <memory>
#include <lua.hpp>
#include <string.h>
#include <array>
#include <assert.h>

constexpr int POOL_SIZE = 1024 * 4;
constexpr int MIN_BLOCK_SIZE = 8 * 8; // ALIGNMENT * 8 => 64
// We want out min block size to be allocated in 64B

struct alignas(8) Thing
{
    float x;
    float y;
};

struct GlobalAllocator
{
    void *Allocate(size_t sizeBytes)
    {
        printf("Allocated %d bytes by global Allocator\n", (int)sizeBytes);
        return ::operator new(sizeBytes, std::align_val_t(alignof(Thing)));
    }
    void DeAllocate(void *ptr, size_t /*osize*/)
    {
        // printf("DeAllocated by global allocator\n");
        ::operator delete(ptr);
    }
    void *ReAllocate(void *ptr, size_t osize, size_t nsize)
    {
        // printf("ReAllocated %d bytes by global allocator\n", (int)nsize);
        void *new_piece_of_memory = this->Allocate(nsize);

        // The reallocation can be also happen for a smaller block of memory,
        // i.e. nsize < osize, in which case you don't want to write past the
        // end of your new memory block.

        memcpy(new_piece_of_memory, ptr, std::min(osize, nsize));

        this->DeAllocate(ptr, osize);

        return new_piece_of_memory;
    }

    static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
    {
        GlobalAllocator *pool = static_cast<GlobalAllocator *>(ud);

        if (nsize == 0)
        {
            if (ptr != nullptr)
            {
                // Becoz we cant deallocate a nullptr
                pool->DeAllocate(ptr, osize);
            }
            return nullptr;
        }
        else
        {
            if (ptr == nullptr)
            {
                // If pointer passed for the old piece of memory is nullptr. Then must be an allocation
                return pool->Allocate(nsize);
            }
            else
            {
                // If pointer passed for the old piece of memory is not nullptr. Then must be an reallocation
                return pool->ReAllocate(ptr, osize, nsize);
            }
        }
    }
};

struct AlignedArenaAllocator
{

    void *m_begin;
    void *m_end;
    void *m_curr;
    size_t pool_size{};

    const int ALIGNMENT = 8;

    struct FreeList
    {
        FreeList *m_next;
    };

    FreeList *m_freeListHead{};
    GlobalAllocator m_globalAllocator;

    explicit AlignedArenaAllocator(void *begin, void *end) : m_begin{begin}, m_end{end}
    {
        pool_size = ((char *)m_end - (char *)m_begin) + 1;
        this->reset_pool();
    }

    void reset_pool()
    {
        m_curr = m_begin;
        m_freeListHead = nullptr;
    }

    size_t sizeToAllocate(size_t size)
    {
        size_t allocated_size = size;

        if (allocated_size < MIN_BLOCK_SIZE)
        {
            allocated_size = MIN_BLOCK_SIZE;
        }

        return allocated_size;
    }

    void *Allocate(size_t sizeBytes)
    {

        size_t allocateBytes = sizeToAllocate(sizeBytes);
        // Now this will make sure that all our allocation is atleast 64B

        if (allocateBytes == MIN_BLOCK_SIZE && m_freeListHead)
        {

            // printf(" --- Allocated from the FreeList ---\n");

            // Here m_freeListHead is not nullptr. It means in out FreeList(LinkedList) we have free blocks available of 64B

            // Let's get free blocks of memory
            void *ptr = m_freeListHead;

            // Since we allocated the one free block of memory. Make the head of the FreeList point to the next block of free memory
            m_freeListHead = m_freeListHead->m_next;

            return ptr;
        }
        else
        {
            // printf("Pool size %d",(int)pool_size);

            // We are not changing the pool_size as the pool is getting allocated. So, the buffer is never too small
            // as a result align will never give nullptr
            if (std::align(ALIGNMENT, sizeof(Thing), m_curr, pool_size))
            {

                /*  Given a pointer ptr to a buffer of size space, returns a pointer aligned by the
                 *   specified alignment for size number of bytes and decreases space argument by the
                 *   number of bytes used for alignment. The first aligned address is returned.
                 *   The function modifies the pointer only if it would be possible to fit the wanted number
                 *   of bytes aligned by the given alignment into the buffer. If the buffer is too small,
                 *   the function does nothing and returns nullptr.
                 */

                void *ptr = m_curr;
                m_curr = (char *)m_curr + allocateBytes;

                // Program will abort if the condition specified inside the assert function is false.
                // assert(m_curr <= m_end); //  We have run out of memory

                if (m_curr <= m_end)
                {

                    printf("Allocated %d bytes\n", (int)allocateBytes);
                    return ptr;
                }
                else
                {
                    // Falling back to the GlobalAllocator
                    return m_globalAllocator.Allocate(sizeBytes);
                }
            }
            else
            {
                printf("Aligned function did not worked\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    void DeAllocate(void *ptr, size_t osize)
    {

        if (ptr >= m_begin && ptr <= m_end)
        {
            // ptr is the memory address of the memory allocated from our memory pool

            size_t allocatedBytes = sizeToAllocate(osize);
            // printf("DeAllocated %d bytes\n", (int)allocatedBytes);

            if (allocatedBytes == MIN_BLOCK_SIZE)
            {

                // printf(" --- DeAllocated to the FreeList ---\n");

                // Lets put the 64B freed block to our FreeList
                // [insert this freed block of memory in the beginning of our FreeList[Linked List]]

                FreeList *newHead = static_cast<FreeList *>(ptr);
                newHead->m_next = m_freeListHead;
                m_freeListHead = newHead;
            }
            else
            {
                // we are just burning through the memory.It means that a used memory
                // can never be used again
            }
        }
        else
        {
            // ptr is the memory address of the memory allocated by the global allocator
            m_globalAllocator.DeAllocate(ptr, osize);
        }
    }

    // Here nsize represents new size and the osize represents old size
    // It returns the starting address of the allocated memory
    void *ReAllocate(void *ptr, size_t osize, size_t nsize)
    {

        // printf("ReAllocated %d bytes\n", (int)nsize);
        void *new_piece_of_memory = this->Allocate(nsize);

        // The reallocation can be also happen for a smaller block of memory,
        // i.e. nsize < osize, in which case you don't want to write past the
        // end of your new memory block.

        memcpy(new_piece_of_memory, ptr, std::min(osize, nsize));

        this->DeAllocate(ptr, osize);

        return new_piece_of_memory;
    }

    static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
    {
        AlignedArenaAllocator *pool = static_cast<AlignedArenaAllocator *>(ud);
        (void)osize;
        if (nsize == 0)
        {
            if (ptr != nullptr)
            {
                // Becoz we cant deallocate a nullptr
                pool->DeAllocate(ptr, osize);
            }
            return nullptr;
        }
        else
        {
            if (ptr == nullptr)
            {
                // If pointer passed for the old piece of memory is nullptr. Then must be an allocation
                return pool->Allocate(nsize);
            }
            else
            {
                // If pointer passed for the old piece of memory is not nullptr. Then must be an reallocation
                return pool->ReAllocate(ptr, osize, nsize);
            }
        }
    }
};

int main()
{
    constexpr const char *MY_LUA_FILE = R"(

        function my_func1(a,b)
            local result = (a*a) + (b*b)
            return result,a,b
        end

    )";

    char memory[POOL_SIZE]; // It is representing 20000 bytes

    AlignedArenaAllocator pool1(memory, &memory[POOL_SIZE - 1]);
    /**
     * Now the memory will be allocated from the pool only
     * And for every memory allocation our l_alloc will be called.
     */
    lua_State *L = lua_newstate(AlignedArenaAllocator::l_alloc, &pool1);

    if (L == nullptr)
    {
        printf("okay\n");
        exit(EXIT_FAILURE);
    }

    Thing *t = (Thing *)lua_newuserdata(L, sizeof(Thing));
    assert((uintptr_t)t % alignof(Thing) == 0);

    luaL_dostring(L, MY_LUA_FILE);
    lua_getglobal(L, "my_func1"); // we pushed the my_func1 into our stack

    if (lua_isfunction(L, -1))
    {
        //  NUM_ARGS -> represents no.of arguments function takes
        //  NUM_RETURNS -> represents no.of values function returns
        //  0 -> is the error handler(LOOK FOR YOURSELF)

        // These two will be used as arguments for the function my_func1
        lua_pushnumber(L, 2);
        lua_pushnumber(L, 3);

        constexpr int NUM_ARGS = 2;
        constexpr int NUM_RETURNS = 3;

        lua_pcall(L, NUM_ARGS, NUM_RETURNS, 0);
        // This function expects that TOS is a function
        // This will call the function present in TOS and stores its value in TOS

        // Since we are returning three arguments from the function, all three returned arguments will be pushed in the stack
        lua_Number a = lua_tonumber(L, -1);
        printf("returned a is = %lf\n", a);

        lua_Number b = lua_tonumber(L, -2);
        printf("returned b is = %lf\n", b);

        lua_Number returned_result = lua_tonumber(L, -3);
        printf("returned result is = %lf\n", returned_result);
    }

    lua_close(L);

    return 0;
}
