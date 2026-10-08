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
