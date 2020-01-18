/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Memory context states
***********************************************************************************************************************************/
typedef enum {memContextStateFree = 0, memContextStateFreeing, memContextStateActive} MemContextState;

/***********************************************************************************************************************************
Contains information about a memory allocation
***********************************************************************************************************************************/
typedef struct MemContextAlloc
{
    bool active:1;                                                  // Is the allocation active?
    unsigned int size:32;                                           // Allocation size (4GB max)
    void *buffer;                                                   // Allocated buffer
} MemContextAlloc;

/***********************************************************************************************************************************
Contains information about the memory context
***********************************************************************************************************************************/
struct MemContext
{
    MemContextState state;                                          // Current state of the context
    const char *name;                                               // Indicates what the context is being used for

    MemContext *contextParent;                                      // All contexts have a parent except top
    unsigned int contextParentIdx;                                  // Index in the parent context list

    MemContext **contextChildList;                                  // List of contexts created in this context
    unsigned int contextChildListSize;                              // Size of child context list (not the actual count of contexts)
    unsigned int contextChildFreeIdx;                               // Index of first free space in the context list

    MemContextAlloc *allocList;                                     // List of memory allocations created in this context
    unsigned int allocListSize;                                     // Size of alloc list (not the actual count of allocations)
    unsigned int allocFreeIdx;                                      // Index of first free space in the alloc list

    void (*callbackFunction)(void *);                               // Function to call before the context is freed
    void *callbackArgument;                                         // Argument to pass to callback function
};

/***********************************************************************************************************************************
Top context

The top context always exists and can never be freed.  All other contexts are children of the top context. The top context is
generally used to allocate memory that exists for the life of the program.
***********************************************************************************************************************************/
MemContext contextTop = {.state = memContextStateActive, .name = "TOP"};

/***********************************************************************************************************************************
Mem context stack used to pop mem contexts and cleanup after an error
***********************************************************************************************************************************/
#define MEM_CONTEXT_STACK_MAX                                       128

static struct
{
    MemContext *memContext;
    bool new;
    unsigned int tryDepth;
} memContextStack[MEM_CONTEXT_STACK_MAX] = {{.memContext = &contextTop}};

static unsigned int memContextCurrentStackIdx = 0;
static unsigned int memContextMaxStackIdx = 0;

/***********************************************************************************************************************************
Wrapper around malloc()
***********************************************************************************************************************************/
static void *
memAllocInternal(size_t size, bool zero)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(BOOL, zero);
    FUNCTION_TEST_END();

    // Allocate memory
    void *buffer = malloc(size);

    // Error when malloc fails
    if (buffer == NULL)
        THROW_FMT(MemoryError, "unable to allocate %zu bytes", size);

    // Zero the memory when requested
    if (zero)
        memset(buffer, 0, size);

    // Return the buffer
    FUNCTION_TEST_RETURN(buffer);
}

/***********************************************************************************************************************************
Wrapper around realloc()
***********************************************************************************************************************************/
static void *
memReAllocInternal(void *bufferOld, size_t sizeOld, size_t sizeNew, bool zeroNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
        FUNCTION_TEST_PARAM(BOOL, zeroNew);
    FUNCTION_TEST_END();

    ASSERT(bufferOld != NULL);

    // Allocate memory
    void *bufferNew = realloc(bufferOld, sizeNew);

    // Error when realloc fails
    if (bufferNew == NULL)
        THROW_FMT(MemoryError, "unable to reallocate %zu bytes", sizeNew);

    // Zero the new memory when requested - old memory is left untouched else why bother with a realloc?
    if (zeroNew)
        memset((unsigned char *)bufferNew + sizeOld, 0, sizeNew - sizeOld);

    // Return the buffer
    FUNCTION_TEST_RETURN(bufferNew);
}

/***********************************************************************************************************************************
Wrapper around free()
***********************************************************************************************************************************/
static void
memFreeInternal(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    free(buffer);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Find space for a new mem context
***********************************************************************************************************************************/
static unsigned int
memContextNewIndex(MemContext *memContext, bool allowFree)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, memContext);
        FUNCTION_TEST_PARAM(BOOL, allowFree);
    FUNCTION_TEST_END();

    ASSERT(memContext != NULL);

    // Try to find space for the new context
    for (; memContext->contextChildFreeIdx < memContext->contextChildListSize; memContext->contextChildFreeIdx++)
    {
        if (!memContext->contextChildList[memContext->contextChildFreeIdx] ||
            (allowFree && memContext->contextChildList[memContext->contextChildFreeIdx]->state == memContextStateFree))
        {
            break;
        }
    }

    // If no space was found then allocate more
    if (memContext->contextChildFreeIdx == memContext->contextChildListSize)
    {
        // If no space has been allocated to the list
        if (memContext->contextChildListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memAllocInternal(sizeof(MemContext *) * MEM_CONTEXT_INITIAL_SIZE, true);

            // Set new list size
            memContext->contextChildListSize = MEM_CONTEXT_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int contextChildListSizeNew = memContext->contextChildListSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memReAllocInternal(
                memContext->contextChildList, sizeof(MemContext *) * memContext->contextChildListSize,
                sizeof(MemContext *) * contextChildListSizeNew, true);

            // Set new list size
            memContext->contextChildListSize = contextChildListSizeNew;
        }
    }

    FUNCTION_TEST_RETURN(memContext->contextChildFreeIdx);
}

/***********************************************************************************************************************************
Create a new memory context
***********************************************************************************************************************************/
MemContext *
memContextNew(const char *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);

    // Check context name length
    ASSERT(name[0] != '\0');

    // Find space for the new context
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    unsigned int contextIdx = memContextNewIndex(contextCurrent, true);

    // If the context has not been allocated yet
    if (!contextCurrent->contextChildList[contextIdx])
        contextCurrent->contextChildList[contextIdx] = memAllocInternal(sizeof(MemContext), true);

    // Get the context
    MemContext *this = contextCurrent->contextChildList[contextIdx];

    // Create initial space for allocations
    this->allocList = memAllocInternal(sizeof(MemContextAlloc) * MEM_CONTEXT_ALLOC_INITIAL_SIZE, true);
    this->allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE;

    // Set the context name
    this->name = name;

    // Set new context active
    this->state = memContextStateActive;

    // Set current context as the parent
    this->contextParent = contextCurrent;
    this->contextParentIdx = contextIdx;

    // Possible free context must be in the next position
    contextCurrent->contextChildFreeIdx++;

    // Add to the mem context stack so it will be automatically freed on error if memContextKeep() has not been called
    memContextMaxStackIdx++;

    memContextStack[memContextMaxStackIdx].memContext = this;
    memContextStack[memContextMaxStackIdx].new = true;
    memContextStack[memContextMaxStackIdx].tryDepth = errorTryDepth();

    // Return context
    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Register a callback to be called just before the context is freed
***********************************************************************************************************************************/
void
memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *callbackArgument)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_TEST_PARAM_P(VOID, callbackArgument);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(callbackFunction != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot assign callback to inactive context");

    // Top context cannot have a callback
    if (this == &contextTop)
        THROW(AssertError, "top context may not have a callback");

    // Error if callback has already been set - there may be valid use cases for this but error until one is found
    if (this->callbackFunction)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);

    // Set callback function and argument
    this->callbackFunction = callbackFunction;
    this->callbackArgument = callbackArgument;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Clear the mem context callback.  This is usually done in the object free method after resources have been freed but before
memContextFree() is called.  The goal is to prevent the object free method from being called more than once.
***********************************************************************************************************************************/
void
memContextCallbackClear(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Error if context is not active or freeing
    ASSERT(this->state == memContextStateActive || this->state == memContextStateFreeing);

    // Top context cannot have a callback
    ASSERT(this != &contextTop);

    // Clear callback function and argument
    this->callbackFunction = NULL;
    this->callbackArgument = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Allocate memory in the memory context and optionally zero it.
***********************************************************************************************************************************/
static void *
memContextAlloc(size_t size, bool zero)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(BOOL, zero);
    FUNCTION_TEST_END();

    // Find space for the new allocation
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;

    for (; contextCurrent->allocFreeIdx < contextCurrent->allocListSize; contextCurrent->allocFreeIdx++)
        if (!contextCurrent->allocList[contextCurrent->allocFreeIdx].active)
            break;

    // If no space was found then allocate more
    if (contextCurrent->allocFreeIdx == contextCurrent->allocListSize)
    {
        // Only the top context will not have initial space for allocations
        if (contextCurrent->allocListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            contextCurrent->allocList = memAllocInternal(sizeof(MemContextAlloc) * MEM_CONTEXT_ALLOC_INITIAL_SIZE, true);

            // Set new size
            contextCurrent->allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int allocListSizeNew = contextCurrent->allocListSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            contextCurrent->allocList = memReAllocInternal(
                contextCurrent->allocList, sizeof(MemContextAlloc) * contextCurrent->allocListSize,
                sizeof(MemContextAlloc) * allocListSizeNew, true);

            // Set new size
            contextCurrent->allocListSize = allocListSizeNew;
        }
    }

    // Allocate the memory
    contextCurrent->allocList[contextCurrent->allocFreeIdx].active = true;
    contextCurrent->allocList[contextCurrent->allocFreeIdx].size = (unsigned int)size;
    contextCurrent->allocList[contextCurrent->allocFreeIdx].buffer = memAllocInternal(size, zero);
    contextCurrent->allocFreeIdx++;

    // Return buffer
    FUNCTION_TEST_RETURN(contextCurrent->allocList[contextCurrent->allocFreeIdx - 1].buffer);
}

/***********************************************************************************************************************************
Find a memory allocation
***********************************************************************************************************************************/
static unsigned int
memFind(const void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find memory allocation
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    unsigned int allocIdx;

    for (allocIdx = 0; allocIdx < contextCurrent->allocListSize; allocIdx++)
        if (contextCurrent->allocList[allocIdx].buffer == buffer && contextCurrent->allocList[allocIdx].active)
            break;

    // Error if the buffer was not found
    if (allocIdx == contextCurrent->allocListSize)
        THROW(AssertError, "unable to find allocation");

    FUNCTION_TEST_RETURN(allocIdx);
}

/***********************************************************************************************************************************
Allocate zeroed memory in the memory context
***********************************************************************************************************************************/
void *
memNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(memContextAlloc(size, true));
}

/***********************************************************************************************************************************
Grow allocated memory without initializing the new portion
***********************************************************************************************************************************/
void *
memGrowRaw(const void *buffer, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find the allocation
    MemContextAlloc *alloc = &(memContextStack[memContextCurrentStackIdx].memContext->allocList[memFind(buffer)]);

    // Grow the buffer
    alloc->buffer = memReAllocInternal(alloc->buffer, alloc->size, size, false);
    alloc->size = (unsigned int)size;

    FUNCTION_TEST_RETURN(alloc->buffer);
}

/***********************************************************************************************************************************
Allocate memory in the memory context without initializing it
***********************************************************************************************************************************/
void *
memNewRaw(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(memContextAlloc(size, false));
}

/***********************************************************************************************************************************
Free a memory allocation in the context
***********************************************************************************************************************************/
void
memFree(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find the allocation
    unsigned int allocIdx = memFind(buffer);
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    MemContextAlloc *alloc = &(contextCurrent->allocList[allocIdx]);

    // Free the buffer
    memFreeInternal(alloc->buffer);
    alloc->active = false;

    // If this allocation is before the current free allocation then make it the current free allocation
    if (allocIdx < contextCurrent->allocFreeIdx)
        contextCurrent->allocFreeIdx = allocIdx;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Move a context to a new parent context

This is generally used to move objects to a new context once they have been successfully created.
***********************************************************************************************************************************/
void
memContextMove(MemContext *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    // Only move if a valid mem context is provided and the old and new parents are not the same
    if (this != NULL && this->contextParent != parentNew)
    {
        // Find context in the old parent and NULL it out
        MemContext *parentOld = this->contextParent;
        unsigned int contextIdx;

        for (contextIdx = 0; contextIdx < parentOld->contextChildListSize; contextIdx++)
        {
            if (parentOld->contextChildList[contextIdx] == this)
            {
                parentOld->contextChildList[contextIdx] = NULL;
                break;
            }
        }

        // The memory must be found
        if (contextIdx == parentOld->contextChildListSize)
            THROW(AssertError, "unable to find mem context in old parent");

        // Find a place in the new parent context and assign it. The child list may be moved while finding a new index so store the
        // index and use it with (what might be) the new pointer.
        contextIdx = memContextNewIndex(parentNew, false);
        ASSERT(parentNew->contextChildList[contextIdx] == NULL);
        parentNew->contextChildList[contextIdx] = this;

        // Assign new parent
        this->contextParent = parentNew;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Switch to the specified context
***********************************************************************************************************************************/
void
memContextPush(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot switch to inactive context");

    ASSERT(memContextCurrentStackIdx < MEM_CONTEXT_STACK_MAX - 1);

    memContextMaxStackIdx++;
    memContextCurrentStackIdx = memContextMaxStackIdx;

    memContextStack[memContextCurrentStackIdx].memContext = this;
    memContextStack[memContextCurrentStackIdx].new = false;
    memContextStack[memContextCurrentStackIdx].tryDepth = errorTryDepth();

    FUNCTION_TEST_RETURN_VOID();
}

void memContextPop(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextCurrentStackIdx > 0);
    ASSERT(!memContextStack[memContextMaxStackIdx].new);
    ASSERT(memContextCurrentStackIdx == memContextMaxStackIdx);

    memContextMaxStackIdx--;
    memContextCurrentStackIdx--;

    // Keep going until we find a mem context that is not new to be the current context
    while (memContextStack[memContextCurrentStackIdx].new)
        memContextCurrentStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

void memContextKeep(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextStack[memContextMaxStackIdx].new);

    memContextMaxStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

void memContextDiscard(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextStack[memContextMaxStackIdx].new);

    memContextFree(memContextStack[memContextMaxStackIdx].memContext);
    memContextMaxStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

MemContext *
memContextPrior(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextCurrentStackIdx > 0);

    unsigned int priorIdx = 1;

    while (memContextStack[memContextCurrentStackIdx - priorIdx].new)
        priorIdx++;

    FUNCTION_TEST_RETURN(memContextStack[memContextCurrentStackIdx - priorIdx].memContext);
}

/***********************************************************************************************************************************
Return the top context
***********************************************************************************************************************************/
MemContext *
memContextTop(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(&contextTop);
}

/***********************************************************************************************************************************
Return the current context
***********************************************************************************************************************************/
MemContext *
memContextCurrent(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(memContextStack[memContextCurrentStackIdx].memContext);
}

/***********************************************************************************************************************************
Return the context name
***********************************************************************************************************************************/
const char *
memContextName(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot get name for inactive context");

    FUNCTION_TEST_RETURN(this->name);
}

/**********************************************************************************************************************************/
void memContextClean(unsigned int tryDepth)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, tryDepth);
    FUNCTION_TEST_END();

    ASSERT(tryDepth > 0);

    while (memContextStack[memContextMaxStackIdx].tryDepth >= tryDepth)
    {
        if (memContextStack[memContextMaxStackIdx].new)
            memContextFree(memContextStack[memContextMaxStackIdx].memContext);
        else
        {
            memContextCurrentStackIdx--;

            while (memContextStack[memContextCurrentStackIdx].new)
                memContextCurrentStackIdx--;
        }

        memContextMaxStackIdx--;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
memContextFree - free all memory used by the context and all child contexts
***********************************************************************************************************************************/
void
memContextFree(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If context is already freeing then return if memContextFree() is called again - this can happen in callbacks
    if (this->state != memContextStateFreeing)
    {
        // Current context cannot be freed unless it is top (top is never really freed, just the stuff under it)
        if (this == memContextStack[memContextCurrentStackIdx].memContext && this != &contextTop)
            THROW_FMT(AssertError, "cannot free current context '%s'", this->name);

        // Error if context is not active
        if (this->state != memContextStateActive)
            THROW(AssertError, "cannot free inactive context");

        // Free child contexts
        if (this->contextChildListSize > 0)
            for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
                if (this->contextChildList[contextIdx] && this->contextChildList[contextIdx]->state == memContextStateActive)
                    memContextFree(this->contextChildList[contextIdx]);

        // Set state to freeing now that there are no child contexts.  Child contexts might need to interact with their parent while
        // freeing so the parent needs to remain active until they are all gone.
        this->state = memContextStateFreeing;

        // Execute callback if defined
        bool rethrow = false;

        if (this->callbackFunction)
        {
            TRY_BEGIN()
            {
                this->callbackFunction(this->callbackArgument);
            }
            CATCH_ANY()
            {
                rethrow = true;
            }
            TRY_END();
        }

        // Free child context allocations
        if (this->contextChildListSize > 0)
        {
            for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
                if (this->contextChildList[contextIdx])
                    memFreeInternal(this->contextChildList[contextIdx]);

            memFreeInternal(this->contextChildList);
            this->contextChildListSize = 0;
        }

        // Free memory allocations
        if (this->allocListSize > 0)
        {
            for (unsigned int allocIdx = 0; allocIdx < this->allocListSize; allocIdx++)
            {
                MemContextAlloc *alloc = &(this->allocList[allocIdx]);

                if (alloc->active)
                    memFreeInternal(alloc->buffer);
            }

            memFreeInternal(this->allocList);
            this->allocListSize = 0;
        }

        // If the context index is lower than the current free index in the parent then replace it
        if (this->contextParent != NULL && this->contextParentIdx < this->contextParent->contextChildFreeIdx)
            this->contextParent->contextChildFreeIdx = this->contextParentIdx;

        // Make top context active again
        if (this == &contextTop)
            this->state = memContextStateActive;
        // Else reset the memory context so it can be reused
        else
            memset(this, 0, sizeof(MemContext));

        // Rethrow the error that was caught in the callback
        if (rethrow)
            RETHROW();
    }

    FUNCTION_TEST_RETURN_VOID();
}
