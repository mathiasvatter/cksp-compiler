# Changelog

## [0.1.0-alpha.4]

> [!IMPORTANT]
> This alpha rounds out **structs**: members and methods can be `static`, constant blocks can live inside a struct as the language's stand-in for enums, and object arrays can be queried by member. The **cksp Language Server** gains full code completion — qualifiers, instance members through access chains, unqualified names, defines and macros. On the compiler side this release is mostly about correctness: a long list of fixes to initializer lists, multidimensional members and the thread-safety analysis, several of which silently produced wrong values or invalid KSP.

## Language

### Added
- Added **`static` struct members**. A `static` member holds one value shared by all instances instead of one per instance, and is reached through the struct itself.
- Added **`static` methods**. A method declared `static` belongs to the struct, takes no `self` and is called as `Struct.method(...)`.
- Added **type-qualified member access** such as `Foo.MAX`, which reaches a struct's shared members without an instance.
- Added **`static const` blocks inside structs**. The entries are scoped to the struct and reachable as `Struct.Block.Entry`. Since they stay compile-time constants, they fold into their use sites and can be used as array dimensions.
  ```cksp
  struct Voice
      static const State
          IDLE
          ACTIVE
      end const
  end struct

  declare pool[Voice.State.ACTIVE]
  ```
- Added **`search_by(array, .field, value)`**, which finds the first object in an array of objects whose member matches a value and returns it, or `nil`. The member is named by a member path such as `.id` and may reach through nested objects (`.child.id`). The lookup runs over the member's own storage instead of scanning the array. *Experimental.*
- Added **`Struct.storage(.member)`**, which hands out the array a struct member is stored in — the values of that member for every instance, by reference rather than as a copy. *Experimental.*
- Added support for **user functions overriding builtin commands** of the same name.
- Added the new **Kontakt 8.4, 8.8 and 8.12** engine constants and engine functions.

### Improved
- Improved parser and preprocessor **diagnostics**. A declaration that borrows the name of a `define` is now reported on the declaration rather than inside the substituted body ([#61](<https://github.com/mathiasvatter/cksp-compiler/issues/61>)).
- A `const` array is now rejected as the target of an array load instead of being written to silently.
- Improved compile times through a shared source-file table instead of per-token paths. A file path is no longer copied into every token and into every clone one takes part in, which leaves large projects around **18% faster** than `v0.1.0-alpha.3` and cuts peak memory by about **20%**.

### Fixed
- Fixed an array copy running past the end of the shorter of the two arrays. `declare dst[3]` assigned from a `src[4]` made Kontakt report *"Array %dst[3] is out of range"*, and the other direction silently read `0` past the end — a valid object index for an array of objects. The copy is now bounded by whichever array declares fewer elements.
- Fixed a single value in an initializer list not spreading over the whole array when Kontakt forced the declaration to be split. `declare chord[3]: Note[] := [n1]` wrote only `chord[0]` and left the rest at `0`, so the remaining elements pointed at whatever object lives in slot 0.
- Fixed a raised initializer list spreading its last value over the array. `declare a[4]: int[]` followed by `a[0] := 7` was raised to `declare %a[4] := (7)`, which made every element read `7`.
- Fixed an initializer list passed as an argument being copied into an array sized after the list. A shorter list left the callee copying past the end of it, and for an array of objects the elements read there were references to object 0 that reference counting then followed. The argument array is now a copy of the parameter it is passed to, so shape and size come along.
- Fixed a compiler crash on a multidimensional object member such as `declare grid[2,3]: Note[][]` inside a struct.
- Fixed `num_elements` returning the wrong dimension depending on the order in which nodes happened to be lowered. A loop over a `[5,7]` member ran 7×7 instead of 5×7 that way, straight into the block of the next instance.
- Fixed a whole reference to a multidimensional array being rejected for every member of rank greater than one, which surfaced through the `self.arr := arr` a generated constructor writes.
- Fixed a multidimensional array reference lowered to a raw array losing its declaration, which produced invalid KSP or a missing-declaration report during function inlining.
- Fixed `static const` blocks inside a namespaced struct being hoisted with the wrong prefix, which made `audio.Voice.State.IDLE` fail with *"Member State does not exist in audio.Voice"*.
- Fixed a member initializer holding several values reaching only the first instance. KSP repeats the last value of an initializer list over the remaining indices, which spreads correctly over one instance but not over the storage of all of them — `row[3]: int[] := (7, 8, 9)` left the second instance reading `(9, 9, 9)`. Such a value is now moved into a hand-written constructor, which runs per instance.
- Fixed several issues in the **thread-safety analysis**: variables were marked thread-safe before all enclosing loops were finished, a lifetime ending inside a thread-unsafe range counted as shareable even with asynchronous commands in between, variables generated for prematurely returning functions were not marked thread-unsafe at all, and `on init` was analysed as if it could run concurrently with itself. Expanded variables no longer initialize the whole callback dimension, which used to wipe the slots of every other invocation still waiting.
- Fixed the initialization of a declaration that gets one slot per callback invocation being dropped. A local array read back whatever the invocation that used its row last left behind, the values such an array was declared with were thrown away, and the early-return flag of an inlined function body was never reset — every copy of that body shares one flag, so the copy behind the first one that returned never ran. In a mixer preset switch only the first of 25 inlined dispatches took effect.
- Fixed a struct being destroyed while the struct tables still handed out pointers to it, which crashed the compiler and the language server on everything lowered afterwards.
- Fixed a monomorphized function being registered under its generic name, so it stayed reachable under a name that no longer belonged to it.
- Fixed a method call binding to a builtin command of the same name instead of to its receiver.
- Fixed the discarded result of a function returning an array being emitted as a bare statement, which is not valid KSP.
- Fixed a rejected `sort` reporting `<search>`, a command the source never called.
- Fixed the preprocessor aborting on a `define` body that ends in the middle of an expression.

## Language Server

### Added
- Added **code completion**, in four parts:
  - **Qualifiers** — members offered after a `.` for namespaces, families, const blocks and the `static` members and methods of a struct.
  - **Instance members** — `inst.`, `audio.inst.`, `inst.child.`, `self.` and `zones[0].` resolve through declared types and offer the members of the struct the chain arrives at.
  - **Unqualified names** — globals, structs, functions, qualifier blocks and the locals of the enclosing function, each offered the way it has to be written at that position.
  - **Defines and macros** — including what a define actually expands to, since the preprocessor substitutes them away before parsing.
- Added a protocol test harness for the language server.

### Improved
- Completion items are now labelled as the construct they were declared as — `member`, `method`, `static const`, `static function` — instead of being derived from the LSP kind, which cannot tell a `family` member from a struct member.
- Improved handling of names that merely contain dots, such as `macro nks.init()` and `function nks.update_labels()`. They are now reachable through their dots, offered unqualified, and shown with their dots in the signature.
- Improved macro support: every declaration a macro parameter leads to is offered, a reference is resolved through the word a substitution assembled it from, and a diagnostic points at the line a substituted token was written on.
- Completion declarations are now scoped to the loop or branch they live in, and compiler-renamed declarations are kept out of the list.

### Fixed
- Fixed a crash when a diagnostic aborted the analysis after struct lowering.
- Fixed a crash caused by the shared reference validator being used across threads.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
