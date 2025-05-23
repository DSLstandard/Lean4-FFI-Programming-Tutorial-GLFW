# Notes on doing C FFI in Lean4
There seems to be no complete tutorial on creating C FFI bindings in Lean4. So I am writing notes here. I am putting the keyword "tutorial" in the GitHub project name just so people who search "Lean4 FFI tutorial" will land on this and hopefully get some guidances, but by no means does this markdown serve as a complete, 100% correct tutorial. I don't even know what is going on half of the time when reading the sources:
- `src/include/lean/lean.h` in https://github.com/leanprover/lean4
- https://lean-lang.org/lean4/doc/dev/ffi.html (it seems to be outdated as it uses a keyword called `constant`, which does not actually exist in Lean4).
- https://github.com/leanprover/lean4/tree/master/src/lake/examples/ffi


## Introduction

(This section might need you to have some understanding in writing C FFIs in Haskell, or writing language bindings in general. Otherwise you might have no idea what is going on or why are things the way they are.)

Say you would like to write some C FFI bindings in Lean4 to [`libglfw.so`](https://www.glfw.org), a popular C library for creating windows and OpenGL contexts. Let's also say you plan to create Lean4 bindings to `glfwInit`, `glfwTerminate`, `glfwPollEvents`, `glfwCreateWindow` and `glfwWindowShouldClose`. In fact, I picked these examples because:
  - `glfwInit`, `glfwTerminate`, and `glfwPollEvents` are simple functions, great for an introduction to writing Lean4 bindings.
  - `glfwCreateWindow` involves passing an opaque object from C to Lean4. I will also demonstrate how to pass `String`s from Lean4 to C.
  - `glfwWindowShouldClose` involves passing an opaque object from Lean4 to C.
  - They provide the minimal set of functionalities to create a working GLFW window.

In short, you will basically have to: (You can skip this paragraph if you want and move on to the steps to creating FFIs for GLFW right now)
1. Write a `.c` source file providing the right functions and function signatures that compiled Lean4 programs know how to talk to.
2. Modify your `lakefile.lean` to...
    1. compile the `.c` file to produce a static lib,
    2. and add the necessary compilation/linking flags to link `libglfw.so` and the static lib of the `.c` to your Lean4 application executable whenever you do `$ lake build`.
3. Write `opaque` Lean4 definitions in a `.lean` module in your Lean4 project that point to the certain functions written in that `.c` file. You might also have to define some opaque structures if the FFI functions return pointers to, say, a C handle object.

## A brief introduction on GLFW

For reference, here are the functions you would like to bind to:
```c
// Initializes the GLFW library.
// `GLFW_TRUE` if successful, or `GLFW_FALSE` if an error occurred.
int glfwInit(void);

// Terminates the GLFW library.
void glfwTerminate(void);

// Creates a GLFW window,
// and returns an abstract handle to it.
GLFWwindow * glfwCreateWindow(
  int width,
  int height,
  const char *title,     // window title
  GLFWmonitor *monitor,  // you may ignore this argument
  GLFWwindow *share      // you may ignore this argument
);

// See: https://www.glfw.org/docs/latest/group__window.html#ga37bd57223967b4211d60ca1a0bf3c832
void glfwPollEvents(void);

// Did the user intend to close `window`? (e.g., pressing the [X] button)
int glfwWindowShouldClose(GLFWwindow *window);
```

Here is a basic GLFW example that creates a blank GLFW window and ends when the user closes the GLFW window:
```c
#include <GLFW/glfw3.h>
#include <stdlib.h>

int main() {
  if (!glfwInit()) {
    perror("Cannot initialize GLFW!!\n");
    return EXIT_FAILURE;
  }

  GLFWwindow *win = glfwCreateWindow(800, 600, "My GLFW window's title", NULL, NULL);

  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
  }

  glfwTerminate();
  printf("Goodbye.\n");
}
```
Our goal will be to rewrite this program in "pure Lean4".

# Writing the FFI

## Setup

Suppose in the beginning, you did `$ lake init` and get the following:

```
.
├── lakefile.lean
├── lake-manifest.json
├── lean-toolchain
├── Main.lean
├── <Your main library directory>/
├── <Your main library name>.lean
└── ... possibly more files
```

First, define the following `opaque` functions in a `.lean` file in your Lean4 project (I will explain the details later). For simplicity, I will write them in `./Main.lean`.

```
-- There are no need for more additional imports.

@[extern "lean_glfwInit"]
opaque glfwInit : IO Bool

@[extern "lean_glfwTerminate"]
opaque glfwTerminate : IO Unit

@[extern "lean_glfwPollEvents"]
opaque glfwPollEvents : IO Unit

-- An opaque type to represent `GLFWwindow *`
opaque WindowP : NonemptyType
def Window := WindowP.type

@[extern "lean_glfwCreateWindow"]
opaque glfwCreateWindow (width : UInt32) (height : UInt32) (title : String) : IO Window
-- We will ignore `monitor` and `share`.

@[extern "lean_glfwWindowShouldClose"]
opaque glfwWindowShouldClose (win : Window) : IO Bool

def main : IO Unit := do
  IO.println "Hello world!"
```

Next, create a file called `./native.c` (the name is arbitrary) and put the following:

```c
#include <lean/lean.h> // IMPORTANT!!

lean_obj_res lean_glfwInit(lean_obj_arg world) {
  // TODO:
}

lean_obj_res lean_glfwTerminate(lean_obj_arg world) {
  // TODO:
}

lean_obj_res lean_glfwCreateWindow(uint32_t width, uint32_t height, b_lean_obj_arg title, lean_obj_arg world) {
  // TODO:
}

lean_obj_res lean_glfwWindowShouldClose(lean_obj_arg winp, lean_obj_arg world) {
  // TODO:
}

lean_obj_res lean_glfwPollEvents(lean_obj_arg world) {
  // TODO:
}
```

After that, modify your `./lakefile.lean` to automatically compile `./native.c` and link the generated static lib and `libglfw.so` to your Lean4 application executable for future `$ lake build` calls. Here is an example:

```
import Lake
open Lake DSL

package «YourProjectName» where
  -- add package configuration options here

@[default_target]
lean_exe «glfw_test_exe» where
  root := `Main
  moreLinkArgs := #["-lglfw", "-Wl,--allow-shlib-undefined"] -- To link `libglfw.so` to your Lean4 app exe.

-- Create target named `native.o` by compile `./native.c`
-- Honestly, this is kind of like writing Makefiles
target native.o (pkg : NPackage _package.name) : FilePath := do
  let native_src := "native.c"
  let native_c := pkg.dir / native_src
  let native_o := pkg.buildDir / "native.o"
  buildFileAfterDep native_o (<- inputFile native_c) fun native_src => do
    let lean_dir := (<- getLeanIncludeDir).toString
    compileO
      "My native.o compilation"
      native_o
      native_src #["-I", lean_dir, "-fPIC"]

-- Create a static lib from `native.o`
extern_lib native (pkg : NPackage _package.name) := do
  let name := nameToStaticLib "native"
  let native_o <- fetch <| pkg.target ``native.o
  buildStaticLib
    (pkg.buildDir / "lib" / name)
    #[native_o]
```

## On `./native.c` and `./Main.lean`
What is going on with `./native.c` and those `@[extern "XXX"]` opaque definitions in our `./Main.lean`?

In `./Main.lean`:
  1. Each `@[extern <func_name>]` marked opaque function in `./Main.lean` corresponds to a function with the name `<func_name>` defined in `./native.c`.
  2. The `WindowP` and `Window` part in `./Main.lean` will become apparent later when we start passing `GLFWwindow *` objects around between Lean4 and C. I don't have an explanation as to why it is written like this, so just copy it and change the names for other opaque C types.

Now, let's take a look at `./native.c`. We will first inspect `lean_glfwInit`:

```c
// Lean4's counterpart in ./Main.lean:
//   @[extern "lean_glfwInit"]
//   opaque glfwInit : IO Bool
lean_obj_res lean_glfwInit(lean_obj_arg world) {
  // TODO: we will call glfwInit() here and return the initialization status code back as a Bool
}
```
`lean_glfwInit` corresponds to `opaque glfwInit : IO Bool` in our `./Main.lean` as indicated by us marking the Lean4 function `@[extern "lean_glfwInit"]`. Whenever we call `opaque glfwInit : IO Bool` in our Lean4 program, `lean_glfwInit` from `./native.c` will be called.

In `lean_glfwInit`'s function signature, you can see that the function takes in one `lean_obj_arg` and returns a `lean_obj_res`. These are Lean4 objects, the typedefs come from `#include <lean/lean.h>`.
  - Side note: When `lake` compiles `./native.c`, `#include <lean/lean.h>` will point to the file `src/include/lean/lean.h` as defined in https://github.com/leanprover/lean4/blob/master/src/include/lean/lean.h.

In fact, `lean_obj_arg` and `lean_obj_res` are just aliases to the type `lean_object *`. Here are their definitions ripped out from `lean/lean.h`:

```c
/* The following typedef's are used to document the calling convention for the primitives. */
typedef lean_object * lean_obj_arg;   /* Standard object argument. */
typedef lean_object * b_lean_obj_arg; /* Borrowed object argument. */
typedef lean_object * u_lean_obj_arg; /* Unique (aka non shared) object argument. */
typedef lean_object * lean_obj_res;   /* Standard object result. */
typedef lean_object * b_lean_obj_res; /* Borrowed object result. */
```

and here is `lean_object`'s definition if you are curious:
```c
typedef struct {
    int      m_rc;
    unsigned m_cs_sz:16;
    unsigned m_other:8;
    unsigned m_tag:8;
} lean_object;
```

Do be informed that Lean4's usages of `lean_object` and `lean_object *` are non-trivial. There are some tricks like `lean_object *` pointer tagging for constants and other black magics in play for optimization. I will NOT touch on them. However, you can find all the disgusting details in the comments written on https://github.com/leanprover/lean4/blob/master/src/include/lean/lean.h (The whole header only has around 2000 lines).

Before explaining more on `lean_glfwInit`'s actual function signature, observe the following translations between Lean4 function types and C function types:

A function that takes in **no arguments** and returns `Unit`.
```c
// @[extern "foo1"]
// opaque foo1 : Unit
lean_obj_res foo1();
// [!] Returning a `Unit` requires returning a `lean_obj_res`.
```

A function that takes in a `Unit` and returns `Unit`.
```c
// @[extern "foo2"]
// opaque foo2 : (a : Unit) -> Unit
lean_obj_res foo2(lean_obj_arg a);
// [!] Taking in 1 argument.
```

...and now 3 `Unit`s as input.
```c
// @[extern "foo3"]
// opaque foo3 : (a : Unit) -> (b : Unit) -> (c : Unit) -> Unit
lean_obj_res foo3(lean_obj_arg a, lean_obj_arg b, lean_obj_arg c);
// [!] Taking in 3 arguments.
```

Here is a function with "special Lean4 types" (`uint8_t`, `uint32_t`).
```c
// @[extern "foo4"]
// opaque foo4 : (a : UInt8) -> (b : UInt32) -> (c : Unit) -> Unit
lean_obj_res foo4(uint8_t a, uint32_t b, lean_obj_arg c);
// [!] Certain Lean4 types have special translations
//     Please see https://lean-lang.org/lean4/doc/dev/ffi.html#translating-types-from-lean-to-c for a complete(?) list.
//     Here, UInt8 maps to uint8_t, and UInt32 maps to uint32_t.
```

..and here is one that takes in a `Bool`, which is mapped to `uint8_t`. It is also a "special Lean4 type".
```c
// @[extern "foo5"]
// opaque foo5 : (mybool : Bool) -> Unit
lean_obj_res foo5(uint8_t mybool);
// [!] Bool maps to uint8_t, where 0 is false and 1 is true.
//     See https://lean-lang.org/lean4/doc/dev/ffi.html#translating-types-from-lean-to-c.
```

Here is a function that takes in `uint8_t` (a special Lean4 type) and returns `uint16_t` (another special Lean4 type). Notice that we are not returning a `lean_obj_res`, but a `uint16_t`. (TODO: cite an official reference on this behaviour, I learnt this through experimenting with FFIs and see what segfaults.)
```c
// @[extern "foo6"]
// opaque foo6 : (mybool : Bool) -> UInt16
uint16_t foo6(uint8_t mybool);
// [!] Returning a uint16_t instead of the usual `lean_obj_res` here because UInt16 translates to uint16_t.
```

**`foo7` is the most important example. It does I/O.**

```c
// @[extern "foo7"]
// opaque foo7 : (mybool : Bool) -> IO Unit
lean_obj_res foo7(uint8_t mybool, lean_obj_arg world);
// [!] Functions with the return type `IO` are quite interesting,
//     Lean4 expects them to take in an extra lean_obj_arg,
//       and they *MUST* return a lean_obj_res.
// =======================================================================================
// I have not been able to find an official explanation for this seemingly strange function signature.
// But here is my guess:
//
//   We have:
//     opaque foo7 : (mybool : Bool) -> IO Unit
//
//   Consider that, in Lean4:
//     abbrev IO : Type → Type := EIO Error
//     def EIO (ε : Type) : Type → Type := EStateM ε IO.RealWorld
//     def IO.RealWorld : Type := Unit
//     def EStateM (ε σ α : Type u) := σ → Result ε σ α
//     inductive Result (ε σ α : Type u) where
//       | ok    : α → σ → Result ε σ α
//       | error : ε → σ → Result ε σ α
//
//   This means that:
//       IO a
//     = EIO Error a
//     = EStateM Error IO.RealWorld a
//     = IO.RealWorld -> Result Error IO.RealWorld a
//     = Unit -> Result Error Unit a (IO.RealWorld is just Unit)
//
//   Therefore,
//       foo7 : (mybool : Bool) -> IO Unit
//     = foo7 : (mybool : Bool) -> IO.RealWorld -> Result Error IO.RealWorld Unit
//     = foo7 : (mybool : Bool) -> (world : Unit) -> Result Error Unit Unit
//              ^^^^^^^^^^^^^^^    ^^^^^^^^^^^^^^
//              first arg          second arg
//
//   This matches what we see in foo7's type signature:
//     lean_obj_res foo7(uint8_t mybool, lean_obj_arg world);
//                       ^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^
//                       first arg       second arg
//
//   This also implies that the returned `lean_obj_res` will in fact be a `Result` object,
//     and indeed we will later see that we will use functions like `lean_io_result_mk_ok`
//     to construct `Result` Lean4 objects and return them back to our Lean4 program.
```

Here is another IO function, it returns a `UInt32` (a special type), but in fact we return a `lean_obj_res` unlike `foo6` because IO functions return a `Result` object.
```c
// @[extern "foo8"]
// opaque foo8 : (mybool : Bool) -> IO UInt32
lean_obj_res foo8(uint8_t mybool, lean_obj_arg world);
// [!] IMPORTANT! Even if we are technically returning a UInt32, we still return a
//       `lean_obj_res` because we are suppose return a `Result` Lean4 object, of which
//       will contain our uint32_t result.
```

Now, hopefully you can see now why `lean_glfwInit` has this type signature:
```c
// @[extern "lean_glfwInit"]
// opaque glfwInit : IO Bool
lean_obj_res lean_glfwInit(lean_obj_arg world);
```

## Implementing `lean_glfwInit()`

***DISCLAIMER: THE FOLLOWING DETAILS COULD BE VERY, VERY WRONG,*** but the final code does produce a working GLFW program.

Let's implement `lean_glfwInit`:
```c
lean_obj_res lean_glfwInit(lean_obj_arg world) {
  // `world` is unused.
  int result = glfwInit();
  return lean_io_result_mk_ok(lean_box(result));
}
```

Here, we call `glfwInit()`, and call some functions provided by `lean/lean.h` to build a `lean_obj_res` as a Lean4 `Result` object as required by `IO` functions, where...
```c
// Construct a `lean_object *` representing `Result.ok r ()`
lean_obj_res lean_io_result_mk_ok(lean_obj_arg r);

// Construct a `lean_object *` representing the signed integer n
lean_object * lean_box(size_t n);
```

Hopefully this is enough to explain why we are writing this to build our `lean_obj_res`:
```c
return lean_io_result_mk_ok(lean_box(result)); // ~ Result IO.Error IO.RealWorld Bool
```

You can now try using `opaque glfwInit : IO Bool` in `./Main.lean`:
```
@[extern "lean_glfwInit"]
opaque glfwInit : IO Bool

-- ...

main : IO Unit
main = do
  IO.println "Initializing GLFW"
  let init_ok <- glfwInit
  unless init_ok do
    IO.println "Cannot initialize GLFW!!" 
    return // end program
  
  IO.println "Initialized GLFW!!"
```

## Implementing `lean_glfwTerminate()` and `lean_glfwPollEvents()`
Let's move on to `glfwTerminate` and `glfwPollEvents`, although they are not so interesting:

```c
// @[extern "lean_glfwTerminate"]
// opaque glfwTerminate : IO Unit
lean_obj_res lean_glfwTerminate(lean_obj_arg world) {
  glfwTerminate();
  // [!] lean_box(0) means 0 or an Unit object.
  return lean_io_result_mk_ok(lean_box(0)); 
}

// @[extern "lean_glfwPollEvents"]
// opaque glfwPollEvents : IO Unit
lean_obj_res lean_glfwPollEvents(lean_obj_arg world) {
  // In fact, `world` is also just lean_box(0)... I think.
  glfwPollEvents();
  return lean_io_result_mk_ok(lean_box(0));
}
```

## Implementing `lean_glfwCreateWindow()` + opaque `GLFWwindow *`

Let's get to `glfwCreateWindow`. Here, things get more complicated because we have to handle the opaque type `GLFWwindow *`:

```c
static void noop_foreach(void *data, b_lean_obj_arg arg) {
  // NOTHING
}

static void glfw_window_finalizer(void *ptr) {
  glfwDestroyWindow((GLFWwindow *) ptr);
}

// opaque WindowP : NonemptyType
// def Window := WindowP.type
static lean_external_class *get_glfw_window_class() {
  static lean_external_class *g_glfw_window_class = NULL;
  if (!g_glfw_window_class) {
    g_glfw_window_class = lean_register_external_class(
      &glfw_window_finalizer,
      &noop_foreach);
  }
  return g_glfw_window_class;
}

// @[extern "lean_glfwCreateWindow"]
// opaque glfwCreateWindow (width : UInt32) (height : UInt32) (title : String) : IO Window
lean_obj_res lean_glfwCreateWindow(uint32_t width, uint32_t height, b_lean_obj_arg title, lean_obj_arg world) {
  printf("lean_glfwCreateWindow %dx%d\n", width, height); // Here for debugging
  char const *title_cstr = lean_string_cstr(title); // Read below for what `lean_string_cstr` does
  GLFWwindow *win = glfwCreateWindow(width, height, title, NULL, NULL); // Returns (GLFWwindow *) -- an opaque type!
  return lean_io_result_mk_ok(lean_alloc_external(get_glfw_window_class(), win));
}

// where...

// Suppose o is a `String` Lean4 object, get the pointer pointing to the UTF8 bytes of the String.
char const * lean_string_cstr(b_lean_obj_arg o);
```

Here, we see new functions related to constructing `lean_object *` from custom pointers (e.g., `GLFWwindow *`):
- `lean_alloc_external`
- `lean_external_class`
- `lean_register_external_class`

You already know what `lean_io_result_mk_ok` does.

### On `lean_alloc_external()` and `lean_external_class`

Here is the function signature of `lean_alloc_external` from `lean/lean.h`:

```c
// Constructs a `lean_object *` representing an externally-defined C object
//   cls:  Lean4's representation of the "type" of the externally-defined C object
//   data: the pointer to that externally-defined C object
lean_object * lean_alloc_external(lean_external_class *cls, void *data);
```

`lean_external_class` is used by Lean4 internally to understand more about the `void *data` you pass in. It *apparently* has two purposes:
1. to tell how to deallocate/finalize `data` when the `lean_object *` goes out of scope in your Lean4 program,
2. (My guess) to tell how to iterate over `data`. (Like how you can have `char *c` and do `c + 3` to get the address of the 3rd `char` by displacement.)

Here is the definition of `lean_external_class` if that is of any help:
```c
typedef void (*lean_external_finalize_proc)(void *data);
typedef void (*lean_external_foreach_proc)(void *data, b_lean_obj_arg* idk_what_this_arg_is_for);

typedef struct {
    lean_external_finalize_proc m_finalize;
    lean_external_foreach_proc  m_foreach;
} lean_external_class;
```

It is okay if you don't understand what `lean_external_class` actually does, because I also don't.

### On `lean_register_external_class()` and `get_glfw_window_class()`

Anyway, recall that to create a `lean_object *` out of `GLFWwindow *win`, we need to do `lean_alloc_external(get_glfw_window_class(), win)`, which then will be wrapped by `lean_lean_io_result_mk_ok`. Having already explained what `lean_alloc_external()` is, I will now explain the `get_glfw_window_class()` part.

Here is the function `get_glfw_window_class` again:
```c
// opaque WindowP : NonemptyType
// def Window := WindowP.type
static lean_external_class *get_glfw_window_class() {
  static lean_external_class *g_glfw_window_class = NULL;
  if (!g_glfw_window_class) {
    g_glfw_window_class = lean_register_external_class(
      &glfw_window_finalizer,
      &noop_foreach);
  }
  return g_glfw_window_class;
}

// where...
typedef void (*lean_external_finalize_proc)(void *);
typedef void (*lean_external_foreach_proc)(void *, b_lean_obj_arg);

// Creates a (lean_external_class *), for use in lean_alloc_external()
lean_external_class * lean_register_external_class(lean_external_finalize_proc, lean_external_foreach_proc);
```

In `get_glfw_window_class`, essentially, we are calling `lean_register_external_class` to tell Lean4 to create a `lean_external_class` representing `GLFWwindow *` with `glfw_window_finalizer` as its finalizer and `noop_foreach` as its "foreach" part,

where `glfw_window_finalizer` is defined as...
```c
static void glfw_window_finalizer(void *ptr) {
  // Here, I defined it to destroy the GLFWwindow object as an example.
  // In reality, I would put nothing here, and instead write a new opaque Lean4 function + C function for this destroying windows explicitly.
  glfwDestroyWindow((GLFWwindow *) ptr);
}
```

...and `noop_foreach` is defined as:
```c
static void noop_foreach(void *data, b_lean_obj_arg arg) {
  // NOTHING
}
```

I am using `noop_foreach` for the `lean_external_class` representing `GLFWwindow *` here because `GLFWwindow *` points to an handle object instead of the start of an array of something. There is no need for a "foreach" functionality.

`get_glfw_window_class` is also written in such a way that the first call to `get_glfw_window_class` will register the `lean_external_class` that represents `GLFWwindow *` and return it, and subsequent classes will return the already created `lean_external_class`. I am making use of a function-scoped `static` variable here to do this trick.

Now, this should clarify what `lean_glfwCreateWindow` does.

### On `Window`, `WindowP` and the `GLFWwindow *` opaque type

As for the opaque `Window`/`WindowP` definition in our `./Main.lean`, we as programmers will have to make sure that whenever a `GLFWwindow *` is the return type or a argument type in our functions, we must use `Window` in our Lean4 code.

```
opaque WindowP : NonemptyType
def Window := WindowP.type

@[extern "lean_glfwCreateWindow"]
opaque glfwCreateWindow (width : UInt32) (height : UInt32) (title : String) : IO Window // [!] Returns `Window`
// lean_obj_res lean_glfwCreateWindow(uint32_t width, uint32_t height, b_lean_obj_arg title, lean_obj_arg world);

@[extern "lean_glfwWindowShouldClose"]
opaque glfwWindowShouldClose (win : Window) : IO Bool // [!] Takes in `Window`
// lean_obj_res lean_glfwWindowShouldClose(lean_obj_arg winp, lean_obj_arg world);
```

## Implementing `lean_glfwWindowShouldClose()`

Finally, let's implement `lean_glfwWindowShouldClose`:
```C
// @[extern "lean_glfwWindowShouldClose"]
// opaque glfwWindowShouldClose (win : Window) : IO Bool // [!] Takes in `Window`
lean_obj_res lean_glfwWindowShouldClose(lean_obj_arg winp, lean_obj_arg world) {
  assert(lean_is_external(winp)); // For debugging

  // This is how we extract the `void *data` from a `lean_obj_arg` if the arg is indeed representing some external data.
  GLFWwindow * win = (GLFWwindow *) lean_get_external_data(winp);
  bool status = glfwWindowShouldClose(win);
  return lean_io_result_mk_ok(lean_box(status));
}

// where...
void * lean_get_external_data(lean_object * o);
```

Hopefully you can understand the definition `lean_glfwWindowShouldClose` without any explanations now.

And that is it!

## GLFW in Lean4

We can now implement the following C code:
```c
#include <GLFW/glfw3.h>
#include <stdlib.h>

int main() {
  if (!glfwInit()) {
    perror("Cannot initialize GLFW!!\n");
    return EXIT_FAILURE;
  }

  GLFWwindow *win = glfwCreateWindow(800, 600, "My GLFW window's title", NULL, NULL);

  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
  }

  glfwTerminate();
  printf("Goodbye.\n");
}
```

...in Lean4:
```
def main : IO Unit := do
  unless (<- glfwInit) do
    (<- IO.getStderr).putStrLn "Cannot initialize GLFW!!"
    return -- TODO: how to exit with EXIT_FAILURE?

  let win <- glfwCreateWindow 800 600 "My GLFW window's title"

  while (not (<- glfwWindowShouldClose win)) do
    glfwPollEvents

  glfwTerminate
  (<- IO.getStdout).putStrLn "Goodbye."
```

All the code is in this repository. Do `$ ./run.sh` to build and run the final GLFW example app.

# TODOs
- [ ] Investigate on constructing `lean_obj_res *` objects of Lean4 inductive types in C.
- [ ] Understand what borrowing is and how it affects programming an FFI.
