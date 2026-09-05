# Changelog

## [0.1.0-alpha.5]

> [!IMPORTANT]
> This alpha adds **generic structs** and a new **type cast** syntax. Other than that, it is mostly about **porting from SublimeKSP**: the constructs cksp has no equivalent for are now recognised by name and named in a meaningful error/warning message or answered with a quick fix instead of an *"unknown construct"*.

## Language

### Added
- Added **generic structs**. A struct can declare type parameters, and each set of type arguments instantiates its own struct with its own storage:
  ```cksp
  struct Box<T>
      static const MAX := 42
      value: T
  end struct

  on init
      declare number: Box<int> := new Box<int>(1)
      declare text: Box<string> := new Box<string>("hello")
      message(Box<int>.MAX)
  end on
  ```
  Type arguments are written wherever the type is: in declarations, in constructor calls (`new List<int>(3, nil)`) and in type qualifiers. A parameterized type cannot be a type argument itself yet (`List<List<int>>`).
- Added **type casts** with `as`. `real(EVENT_NOTE) as int`, `x as real`, `x as bool` and `x as string` convert between the primitive types, and `id as Item` reads an object id back as an object of that struct — in a string context that goes through the struct's string representation rather than printing the raw id.

### Improved
- An **annotated return type is now enforced**. `function get(self): Box<T>` returning `1` is an error instead of being inferred away; `nil` stays accepted for a function returning an object.
- An **array initializer reading a non-constant variable** now warns. The initializer copies the value it sees at the declaration and does not follow the variable afterwards, which is what `declare arr[3] := [0, 1, variable]` suggests.
- **Reading a variable in its own declaration** (`declare x := f(x)`) is now diagnosed. The compiler used to accept it while the language server called the name undeclared.
- Improved **import diagnostics**: importing the same file twice under two different aliases is an error, and a circular import is reported as one.
- The **obfuscator** leaves the identifiers of `ui_control` variables alone, since KUI and KScript address them by name, and likewise the variables handed to the load/save array commands and the KSP log functions.
- A **pass-by-value warning names the parameter as it was written**, instead of offering to rewrite `ctrl0` for a parameter spelled `ctrl`.

### Fixed
- Fixed **`sh_left` and `sh_right` being pre-calculated differently than the Kontakt engine evaluates them**. The fold now takes only the low five bits of the count the way Kontakt does: `sh_left(1, 33)` is 2, `sh_right(1, -1)` is 0, `sh_left(1, 31)` is `INT32_MIN`.
- Fixed compiles failing at random with *"`<Variable>` has not been declared"* ([#124](https://github.com/mathiasvatter/cksp-compiler/issues/124)).
- Fixed a **`select` case with a single value being read past the end of its storage**, which any script using named constants as case labels runs into.
- Fixed an **array-returning function handing its result through a shared global copy**. Arrays are now returned by reference and land in the caller's variable directly.
- Fixed **`search`, `sort` and `num_elements` rejecting function calls returning arrays**, such as `Struct.storage(.member)`.
- Fixed a crash on a **type-qualified storage access** such as `List<int>.storage(.value)`.
- Fixed a **function whose return expression holds more than one function call**, such as `return self.a() > other.a()`, being miscompiled: it stayed in expression position and its arguments were then read out of bounds.
- Fixed **dead code elimination dropping a store whose value is read inside an expression**. `acc := table[0]` followed by `tmp := acc + 1` and `acc := table[1]` lost the first store, so `tmp` was built from a variable that was never written.

## Migrating from SublimeKSP

### Added
- Added a **quick fix for `taskfunc` and `tcm.*`**, which used to hit *"Found unknown construct"*. The block is parsed in full and rewritten by the fix: the keywords, `var`/`out` to `ref`, `tcm.wait` to `wait` and `tcm.init` to `#pragma max_callback_depth`.
- Added recognition of the **SublimeKSP property block**, which used to be read as a declaration of a variable called `property` and reported as whatever went wrong after it. It is now named wherever it can appear — a callback, the global scope, a struct body — together with what replaces it.
- Added a warning for **SublimeKSP pragmas cksp has no equivalent for**. `{#pragma save_compiled_source ...}` is a comment here, so the output silently went to the default path while the compile reported success; it now warns and offers `#pragma output_path("...")`. The line stays, so the same source keeps compiling under both compilers until the fix is applied.
- Added an **identifier-case migration**. SublimeKSP resolves names case-insensitively, so every declaration a ported script spells in a case of its own used to land on an undeclared-name error. A name that differs from exactly one declaration in nothing but case is now resolved to it and offered a rename — including a name assembled by a macro, which is corrected at the call site, and a raw ndarray reference such as `_env2.arr`.
- Added a **global initializer migration**.
- Added a rename for a **function result spelled `return`**, offered across every place the name stands.
- Added an answer for **`iterate_post_macro` and `literate_post_macro`**, naming them as SublimeKSP's and saying why the post variants differ: cksp evaluates `iterate_macro`/`literate_macro` during macro expansion, the post ones run after it. No fix is offered, since a rename only works when the bounds are known at expansion time.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
