# HotDylib documentation
Implementation details, tips and tricks, reading resources.

## Reading resources
- [C++ as a scripting language](https://blog.molecular-matters.com/2014/05/10/using-runtime-compiled-c-code-as-a-scripting-language-under-the-hood/)
- [DLL hot reloading in theory and practice](https://ruby0x1.github.io/machinery_blog_archive/post/dll-hot-reloading-in-theory-and-practice/index.html)
- [Odin + Raylib game template, including hot reload](https://zylinski.se/posts/no-engine-gamedev-using-odin-and-raylib/)

## State transfer after hot reload
- Simply make your struct big enough (mostly Entity/Actor):
```C
struct Entity // add alignas if you wanna do full DoD
{
    // Fields

    // Reversed for hot reload
    uint8_t _reserved[<remain size>];
};

// Controlling your Entity size
static_cast(sizeof(Entity) == ENTITY_EXPECTED_SIZE, "Entity size is wrong!");
```
- Another problem, new fields is not inialized: just use ZII, when create entity, memset the memory.
- Another approach, doing serialization, and use rolled your own RTTI, declare the default values of fields.

## Mistake
- Because of function pointer, virtual methods:
    - The function's instructions memory will be store inside DLL
    - When DLL unload, the instructions will be unload
    - So we have pointer dangling
    - Next update will crash the host

- Make sure your data structure have no function pointer
- I have a experience, whill storing Odin's `map` in game state (actual using for storing resources), the next reload will crash the game

## System that need and helping hot reload better
- Hot reload assets
- Save system, checkpoint when playing, you can revert back to the state you want to check. Specially in boss combat.
- Replay system, or simply Player Input Tracker
- Crash Report
- Safe execution hot reload DLL functions.
    - In Windows, we have Structured Exception Handler and Vectored Exception Handler
    - In Unix, we can singal() to mimic the behaviours
- Debugger:
    - Visual Studio will locked PDB by default. Solution: https://blog.molecular-matters.com/2017/05/09/deleting-pdb-files-locked-by-visual-studio/
    - In my opinion, use RadDebugger is better approach, they are fast, can use for custom workflow.
- If you do all theses things, you will have a workflow like this (which is very inspired to me): https://www.youtube.com/watch?v=72y2EC5fkcE

## Sample code for safe invoke DLL functions
Acknowlegdes `pragmatic_hero` from [Handmade Network (scroll to bottom)](https://hero.handmade.network/forums/code-discussion/t/952-using_a_pure_c_compiler_instead_of_msvc)
```C
#define WIN32_EXCEPTION_LIST \
    FUN(EXCEPTION_ACCESS_VIOLATION) \
    FUN(EXCEPTION_ARRAY_BOUNDS_EXCEEDED) \
    FUN(EXCEPTION_BREAKPOINT) \
    FUN(EXCEPTION_DATATYPE_MISALIGNMENT) \
    FUN(EXCEPTION_FLT_DENORMAL_OPERAND) \
    FUN(EXCEPTION_FLT_DIVIDE_BY_ZERO) \
    FUN(EXCEPTION_FLT_INEXACT_RESULT) \
    FUN(EXCEPTION_FLT_INVALID_OPERATION) \
    FUN(EXCEPTION_FLT_OVERFLOW) \
    FUN(EXCEPTION_FLT_STACK_CHECK) \
    FUN(EXCEPTION_ILLEGAL_INSTRUCTION) \
    FUN(EXCEPTION_IN_PAGE_ERROR) \
    FUN(EXCEPTION_INT_DIVIDE_BY_ZERO) \
    FUN(EXCEPTION_INT_OVERFLOW) \
    FUN(EXCEPTION_INVALID_DISPOSITION) \
    FUN(EXCEPTION_NONCONTINUABLE_EXCEPTION) \
    FUN(EXCEPTION_PRIV_INSTRUCTION) \
    FUN(EXCEPTION_SINGLE_STEP) \
    FUN(EXCEPTION_STACK_OVERFLOW)

#include <setjmp.h>
static jmp_buf exception_jumper;

static LONG WINAPI
VectoredHandler__(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    PCONTEXT Context;
    Context = ExceptionInfo->ContextRecord;
    printf("\n---------------- EXCEPTION ---------------------\n");
    DWORD except_code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    #define FUN(E) if(except_code == E) {printf("Code 0x%x: %s \n", except_code, #E);} else
        WIN32_EXCEPTION_LIST {printf("UNKNOWN EXCEPTION CODE, wtf!\n");}
    #undef FUN
    printf("------------------------------------------------\n\n");
    longjmp(exception_jumper, 1);
    return EXCEPTION_CONTINUE_EXECUTION;
}

#define TRY_EXCEPT(TRY, EXCEPT) \
    {    \
        PVOID vectored_exhandler; \
        vectored_exhandler = AddVectoredExceptionHandler(0,VectoredHandler__); \
        if(!setjmp(exception_jumper)) { \
            TRY \
        } else { \
            EXCEPT \
        } \
        RemoveVectoredExceptionHandler(vectored_exhandler);    \
    }

#define DLL_INVOKE(FUN_NAME, ...) if(FUN_NAME) { \
    TRY_EXCEPT( \
        {FUN_NAME(__VA_ARGS__);}, \
        {FUN_NAME = NULL; \
        printf("Culprit: `%s` REMOVED AND TERMINATED! \n\n", #FUN_NAME);} \
    ); \
    }
```
