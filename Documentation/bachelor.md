
# Bachelor topics

## 1. Lexical Scope to Global Scope
- Checking of scopes and variable shadowing
- rename local variables to get global scoping
- reuse variables depending on their dynamic extend
- do lambda lifting in functions
- (turn local variables into global array)

### Vorgehen:
1. Variablen und Scopes tracken
2. Dynamischen Extend von Variablen/Arrays innerhalb verschiedener Scopes in den Funktionen tracken und mit typisierten Variablen austauschen, deren dynamic extend bereits abgelaufen ist.
3. Alle lokalen Variablen mit Gensym umbenennen.
4. Lambda Lifting mit allen Funktionen durchführen.
5. Punkt 2 und 3 mit allen Callbacks ausführen.

### Beispiel:
#### Vorher:
```c
function scoped_func2()
	declare i: int
	message(i)
	if(i = 1)
		declare loc_array[5] := (1,2,3,4)
		declare j: int, k: int
		message(j & k & loc_array[3])
	end if
	if(i = 2)
		declare h: int, g: int
		message(h & g)
	end if
end function
```
####Nachher:
```c
function scoped_func2(loc_0 : int, loc_1 : int[], loc_2 : int, loc_3 : int)
	loc_0 := 0
	message(loc_0)
	if(loc_0 = 1)
		loc_1 := (1, 2, 3, 4)
		loc_2 := 0
		loc_3 := 0
		message(loc_2 & loc_3 & loc_1[3])
	end if
	if(loc_0 = 2)
		loc_2 := 0
		loc_3 := 0
		message(loc_2 & loc_3)
	end if
end function
```

### Herausforderungen:
- Lambda Lifting bringt Overhead an Funktionsparametern mit sich -> Funktionen die vorher gecallt wurden 
(komplett ohne Parameter) müssen jetzt inlined werden, was wiederum zu mehr Overhead und längerem Code führt.
__Lösung?:__ Globaler Stack, der alle Funktionsparameter vorher übergibt und nachher wieder ausgibt.
- Durch Lambda Lifting und das mehrfache Auslagern von Declarations (in Callbacks und dann in den init Callback) kommt
es zu vielen unnötigen Assignments.
__Lösung?:__ Optimierung der Assignments durch Code-Flow Analyse und Dead Code Elimination.


## 2. Rekursive Data Structure declaration and dynamic allocation
- Allow declaration of structs by lowering them to arrays
- Allow dynamic allocation by tracking them in a global array
- Allow recursive data structures by using pointers to the next index of the global struct array
- Add nil to reference an uninitialized struct
- Add type 'object' to reference any struct
- Add methods to structs (`__init__`, `__repr__`, ...)

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

## 3. Allow recursive Functions (Defunctionalize the Continuation)
- Transform recursive Functions into continuation passing style
- Defunctionalize by turning functions into data structures
- Allow recursive functions on these data structures by lowering them to loops
- <https://www.pathsensitive.com/2019/07/the-best-refactoring-youve-never-heard.html>
