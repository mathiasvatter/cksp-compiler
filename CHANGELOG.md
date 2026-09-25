# Changelog

## [0.1.0-alpha.6]

> [!IMPORTANT]
> This alpha adds **property accessors** and **subscript overloads** to structs, and **signature help** to the language server. Porting from SublimeKSP gets further again: `iterate_post_macro` and `literate_post_macro` are now supported, and several constructs that used to stop a ported script now come with a quick fix.

## Language

### Added
- Added **property accessors** `__get__` and `__set__` to structs. Reading an object where a value is expected calls `__get__`, and assigning to it calls `__set__`:
  ```cksp
  struct Value
      number: int
      function __get__(self): int
          return self.number
      end function
      function __set__(self, value: int)
          self.number := value
      end function
  end struct

  on init
      declare v := new Value(5)
      message(v + 7)   // __get__
      v := 42          // __set__
  end on
  ```
- Added **subscript overloads** `__getitem__` and `__setitem__`. A subscript on a single object calls them with one parameter per index and, for the setter, the value last:
  ```cksp
  struct EnginePar
      par: int
      function __getitem__(self, g: int, s: int): int
          return get_engine_par(self.par, g, s, -1)
      end function
      function __setitem__(self, g: int, s: int, value: int)
          set_engine_par(self.par, value, g, s, -1)
      end function
  end struct

  on init
      declare volume := EnginePar(ENGINE_PAR_VOLUME)
      volume[0, -1] := 630000 // for group 0, slot -1
  end on
  ```
- Added **member access and assignment through a cast**: `(id as Item).value := 5`, `(id as Item).value += 2` and `(id as Item).bump()` now work.
- Added **`const` blocks inside namespaces** ([#128](https://github.com/mathiasvatter/cksp-compiler/issues/128)). They are reached as `Namespace.Block.Entry` and still count as constants, so they can size an array.
- Added 14 missing **Twin Delay engine parameters** (`ENGINE_PAR_TDL_*`) to the builtin constants.
- Added a warning for **`ctrl -> par` on a UI control par passed by value**. The function received the control's value instead of its ID. It now comes with the same *"Pass by reference"* quick fix as `get_ui_id(param)`.

### Improved
- **Operator overload signatures are checked where the method is defined.** A wrong parameter or return count used to show up as a type mismatch where the operator was used, or not at all.
- Assigning an object to a member whose type has a `__set__` now **explains that the accessor is the reason**, instead of naming the setter's parameter.
- A `struct` inside a namespace is now **resolved by the name it is written with**, also in type annotations and from sibling structs that come further down.
- The **non-constant array initializer** message is now a **hint**: it no longer shows up in the Problems panel or in the console. Arrays of `get_ui_id(...)` no longer trigger it at all, since a control's ID never changes.
- A **missing output folder** is reported as such, with a quick fix that creates it. A `./` output path now means the entry file's folder, just as it does for imports.

### Fixed
- Fixed [#132](https://github.com/mathiasvatter/cksp-compiler/issues/132): **persistent local variables** are now reported as an error instead of being accepted.
- Fixed [#131](https://github.com/mathiasvatter/cksp-compiler/issues/131): the **parameters of an auto-generated constructor** no longer inherit persistence from the struct members they initialize.
- Fixed the **obfuscator renaming persistent variables**, which lost their saved values.
- Fixed **list blocks** with rows of arrays, rows of different lengths, and string or real values. `list.SIZE` can now size an array.

## Migrating from SublimeKSP

### Added
- Added support for **`iterate_post_macro` and `literate_post_macro`**. They expand after every other macro, so their bounds, list and callee can be built from macro parameters.
- Added the **`else` fallback in `select` statements** as an alternative to `default` ([#119](https://github.com/mathiasvatter/cksp-compiler/issues/119)).
- Added **`(* ... *)` block comments**.
- Added **imports relative to the entry file**. SublimeKSP resolves every import against the main script, so a nested module importing a sibling of the entry file no longer fails. The path next to the importing file is still tried first, and the error names both folders.
- Added a rename for a **parameter named `ref`**, which cksp reads as the pass-by-reference keyword.
- Added quick fixes for the **SublimeKSP compile toggles** that have a cksp pragma: `optimize_code`, `combine_callbacks` and `compact_variables` become `#pragma optimize`, `combine_callbacks` and `obfuscate`.

### Improved
- A **`define` inside a macro body** now receives the macro's arguments, so `define MY_#name#` creates one define per expansion.
- The migration assistant now also fixes pass-by reference warnings. It is only rewritten when every call passes a variable, since `ref` would break a call that passes an expression.

## Language Server

### Added
- Added **signature help** for user-defined functions and methods.

### Improved
- **Member completion prefers a typed declaration**: inside `function remove(preset: Preset)`, `preset.` now completes the parameter even when a `const preset` block exists.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
