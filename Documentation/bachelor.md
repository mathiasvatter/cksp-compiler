
# Bachelor topics

## 1. Global Scope to Lexical Scope
- Checking of scopes and variable shadowing
- rename local variables to get lexical scoping
- turn local variables into global array

## 2. Data Structure declaration and dynamic allocation
- Allow declaration of structs by lowering them to arrays
- Allow dynamic allocation by tracking them in a global array
- Add nil to reference an uninitialized struct
- Add type 'object' to reference any struct
- Add methods to structs (__init__, __repr__, ...)

```c
struct List
    declare value: int
    declare next: List := nil

    function __init__(self, value: int, next: List)
        self.value := value
        self.next := next
    end function
end struct

declare this_list: List // := nil
this_list := List(42, nil)

declare that_list: List := List(39, List(40, List(41, this_list)))
// new type: object Pointer nil := -1
message(that_list.next.value)
```
Gets lowered to:
```c
declare const MAX_STRUCTS := 1000000
declare @object.warning := "Memory Error: No more free space available to allocate objects of type"

declare %List.allocation[MAX_STRUCTS] := (0)
declare %List.value[MAX_STRUCTS]             // Wert des Elements
declare %List.next[MAX_STRUCTS] := (-1)      // Verweis auf den Index des nächsten Elements
declare %List.free_idx := 0                  // Zeigt auf den ersten freien Platz im Array

declare $this_list := -1 // nil
$this_list := List.__init__(42, -1)

declare $that_list := List.__init__(39, List.__init__(40, List.__init__(41, this_list)))
message(%List.value[%List.next[$that_list]])

function List.__init__(value, next) -> result
    List.free_idx := search(List.allocation, 0)
    if List.free_idx = -1
        message(@object.warning & "'List'")        
    end if
    List.allocation[List.free_idx] := 1
    List.value[List.free_idx] := value
    List.next[List.free_idx] := next
    result := List.free_idx
end function

function nil() -> result
    result := -1  // -1 repräsentiert `nil` in dieser Implementierung
end function
```

## 3. Recursive Data Structures
- Allow recursive data structures by using pointers to the next index of the global struct array
- Allow recursive functions on these data structures by lowering them to loops

```c
function map(ls, func)
    if ls = nil()
        return nil()
    else
        new_next := map(ls.next, func)  // Rekursiver Aufruf
        return List(func(ls.value), new_next)
    end function
end function

function double(x)
    return x * 2
end function
```
Gets lowered to:
```c
function map(ls, func)
    current_pointer := ls  // Beginne mit that_list
    if(current_pointer = -1)
        return -1
    else
        new_next := -1
        while (current_pointer # -1)
            new_next := List.__init__(func(%List.value[current_pointer]), new_next)
            current_pointer := %List.next[current_pointer]  // Gehe zum nächsten Element -> recursive call
            // look for the return statement -> bind to recursive assignment -> 
            // new_next := List(func(ls.value), new_next)
        end while
        return new_next
    end if
end function
```