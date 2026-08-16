# Changelog

## [0.1.0-alpha.4]

> [!IMPORTANT]
> This alpha rounds out **structs**: members and methods can be `static`, constant blocks can live inside a struct, and object arrays can be queried by member. The **cksp Language Server** gains code completion. On the compiler side it is mostly about correctness — fixes to initializer lists, multidimensional members and the thread-safety analysis, several of which silently produced wrong values or invalid KSP.

## Language

### Added
- Added **`static` struct members and methods**. A `static` member holds one value shared by all instances instead of one per instance, and is reached through the struct itself without an instance by **type-qualified member access** such as `Foo.MAX`. A `static` method belongs to the struct, takes no `self` and is called as `Struct.method(...)`.
- Added **`static const` blocks inside structs**. The entries are scoped to the struct and reachable as `Struct.Block.Entry`. They stay compile-time constants and can be used as array dimensions.
  ```cksp
  struct Voice
      static const MAX_ALLOWED := 420
      static const State
          IDLE, ACTIVE
      end const
      state: int

      static function foo()
      ...
      end function
  end struct

  declare pool[Voice.MAX_ALLOWED]: Voice[]
  ```
- Added **`search_by(array, .field, value)`**, which returns an object in an array of objects whose member matches a value, or `nil`. The member path may reach through nested objects (`.child.id`). *Experimental.*
- Added **`Struct.storage(.member)`**, which returns a reference to the 'heap' array a struct member is stored in. *Experimental.*
- Added support for **user functions overriding builtin commands** of the same name.
- Added the new **Kontakt 8.4, 8.8 and 8.12** engine constants and engine functions.

### Improved
- Improved parser and preprocessor **diagnostics** including better error reporting on incorrect define substitutions ([#61](<https://github.com/mathiasvatter/cksp-compiler/issues/61>)).
- A `const` array is now rejected as the target of an array load instead of being written to silently.
- Improved compile times through a shared source-file table instead of per-token paths: large projects compile around **18% faster** than `v0.1.0-alpha.3` and use about **20%** less memory.
- Compiling the same sources twice now produces the **same output**, apart from the timestamp the compiler writes into it. Generated variable names and the order of the generated declarations used to shift from one compile to the next, which made a `diff` between two builds of a script unreadable.
- Improved compile times at `-O3` (`--optimize aggressive`): scalar vars to array optimization is now **1.5–3× faster**, which takes around **200 ms** off a large project.

### Fixed
- Fixed an array copy running past the end of the shorter of the two arrays. `declare dst[3] := src` (and `src` has size 4) made Kontakt report *"Array %dst[3] is out of range"*, and the other direction silently read `0` past the end. The copy is now bounded by whichever array declares fewer elements.
- Fixed a single non-constant value in an initializer list not spreading over the whole array. `declare chord[3]: Note[] := [n1]` wrote only `chord[0]` and left the rest at `0`, so the remaining elements pointed at whatever object lives in slot 0.
- Fixed a raised initializer list spreading its last value over the array. `declare a[4]: int[]` followed by `a[0] := 7` was raised to `declare %a[4] := (7)`, which made every element read `7`.
- Fixed an initializer list passed as an argument being copied into an array sized after the list rather than after the parameter, which left the callee reading past the end of a shorter list.
- Fixed a compiler crash on a multidimensional object member such as `declare grid[2,3]: Note[][]` inside a struct.
- Fixed `num_elements` returning the wrong dimension of a multidimensional member. A loop over a `[5,7]` member ran 7×7 instead of 5×7, straight into the storage of the next instance.
- Fixed a multidimensional array reference producing invalid KSP or a missing-declaration error during function inlining.
- Fixed a member initializer holding several values reaching only the first instance. KSP repeats the last value of an initializer list, which spreads over one instance but not over the storage of all of them — `row[3]: int[] := (7, 8, 9)` left the second instance reading `(9, 9, 9)`. Such a value now moves into the constructor, which runs per instance.
- Fixed several issues in the **thread-safety analysis** that let concurrent invocations of a callback share variables meant to be unique to each: variables were marked thread-safe too early, `return_flag` variables generated for returning functions failed to get marked, `on init` was mistakenly analysed. 
- Fixed: certain thread-safe variables would initialize across their entire callback dimension, wiping the slots of every other invocation still waiting.
- Fixed: the discarded result of an expression function called as an isolated statement being emitted as a bare statement, which is not valid KSP.

## Language Server

### Added
- Added **code completion**, in four parts:
  - **Qualifiers** — members offered after a `.` for namespaces, families, const blocks and the `static` members and methods of a struct.
  - **Instance members** — `inst.`, `audio.inst.`, `inst.child.`, `self.` and `zones[0].` resolve through declared types and offer the members of the struct the chain arrives at.
  - **Unqualified names** — globals, structs, functions, qualifier blocks and the locals of the enclosing function, each offered the way it has to be written at that position.
  - **Defines and macros** — including what a define actually expands to, since the preprocessor substitutes them away before parsing.
- Added a protocol test harness for the language server.

### Improved
- Improved macro support: declarations reached through a macro parameter are offered, and references and diagnostics point at the line the word was actually written on.
- Completion declarations are now scoped to the loop or branch they live in, and compiler-renamed declarations are kept out of the list.

### Fixed
- Fixed dangling pointers to structs handed out by the struct table, which crashed the compiler and the language server on everything lowered afterwards ([#122](https://github.com/mathiasvatter/cksp-compiler/issues/122)).
- Fixed a crash caused by the shared reference validator being used across threads.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
