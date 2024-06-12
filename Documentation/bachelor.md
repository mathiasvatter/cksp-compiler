
# Bachelor topics

## 1. Lexical Scope to Global Scope
- Checking of scopes and variable shadowing
- rename local variables to get global scoping
- reuse variables depending on their dynamic extend
- do parameter promotion (lambda lifting?) in functions
- (turn local variables into global array?)

### Vorgehen:
#### In Funktionsdefinitionen und Callbacks:

1. Variablen und Scopes tracken
    - Nehme einen Stack aus Maps an Variablen, pro Scope wird eine neue Map eingesetzt, die mit Variablen/Arrays gefüllt wird
    - jede Variable Reference erhält einen Pointer auf ihr deklaration in der Map

#### Nur in Funktionsdefinitionen:

2. Dynamischen Extend von Variablen/Arrays innerhalb verschiedener Scopes in den Funktionen tracken und mit typisierten Variablen austauschen, 
deren dynamic extend bereits abgelaufen ist. Deklarationen werden durch Assignments mit neutralen Elementen ersetzt.
    - Erstelle eine Map aus "passive variables" mit Hashwerten aus Variablen Typen (und Array sizes). Sobald ein Scope verlassen wird, werden die Variablen (deren dynamic extend nun abgelaufen ist)
    in die Map eingetragen.
    - Bei jeder neuen Deklaration wird geprüft, ob eine Variable mit gleichem Typ bereits in der Map ist. Falls ja, wird die Variable durch eine Referenz auf die Map ersetzt.
    - Ersetzte Deklarationen werden durch Assignments (mit neutralen Elementen) ersetzt.
3. Alle lokalen Variablen mit Gensym umbenennen.
    - Tracke alle Variablen und Referenzen in Listen und benenne sie mit Gensym um, damit kein Variable Capturing stattfindet,
   wenn eine der freien "passiven Variablen" gleich heißt wie eine Variable im Scope.
     
#### Beispiel:

__Pre Function Definition:__
```c
function scoped_func2()
    declare i: int
    message(i)
    if(i = 1)
        declare arr[5] := (1,2,3,4) // <- turns into passive var as soon as scope is left
        declare j: int // <- turns into passive var as soon as scope is left
        declare k: int // <- turns into passive var as soon as scope is left
        message(j & k & arr[3])
    end if
    if(i = 2)
        declare h: int // <- will be replaced by passive var
        declare g: int // <- will be replaced by passive var
        message(h & g)
    end if
end function
```

__Post Function Definition:__
```c
function scoped_func2(loc_0 : int, loc_1 : int[], loc_2 : int, loc_3 : int)
    loc_0 := 0
    message(loc_0)
    if(loc_0 = 1)
        loc_1 := (1, 2, 3, 4) // <- so direkt in KSP nicht möglich
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
#### Nur in Callbacks:
   4. Parameter Promotion/(Lambda Lifting?) mit allen Funktionen durchführen, bis alle Variablen in Callbacks sind.
      - Führe die ersten drei Schritte zunächst nur mit den Funktionsdefinitionen durch.
      - Besuche nun jeden Function Call bis zum letzten nested call, füge die lokalen Deklarationen als neue Parameter und in eine Map mit pointer auf die nächst höhere Funktionsdefinitionen ein.
      - Wiederhole dieses Vorgehen bis der Function Call Stack leer ist und füge die Deklarationen in den Callback ein.
      
#### Beispiel:

__Pre Parameter Promotion:__
```c
on persistence_changed // <- callback
    declare array[3] := (1,2,5)
    scoped_func2()
end on
```

__Post Parameter Promotion:__
```c
on persistence_changed
    declare array[3] := (1,2,5)
    declare loc_1: int
    declare loc_2: int[]
    declare loc_3: int
    scoped_func2(loc_1, loc_2, loc_3)
end on
```
   5. Punkt 2 und 3 mit allen Callbacks ausführen, ohne die Funktionen zu besuchen.
      - Führe die Schritte durch und ersetze die Deklarationen in den Callbacks durch passive Variablen.
      - Füge alle Deklarationen in den init Callback ein.
      - Umbenennung mit Gensym.


### Herausforderungen:
- Array sizes sind nicht unbedingt bekannt, die Wiederverwendung von Arrays hängt derzeit nur an Typ und Dimension.
    * __Lösung:__ Füge zum Hash Wert in der Map der passiven Variablen nicht nur den Typ, sondern auch Größe und 
      andere Faktoren hinzu, die die Wiederverwendung von Arrays beeinflussen (persistence, const).
    * __Problem:__ Größe kann auch Expression sein oder Konstante (womöglich vorher constant propagation machen?)
- __TO-DO__: Lambda Lifting bringt Overhead an Funktionsparametern mit sich -> Funktionen die vorher gecallt werden konnten 
(also keine Parameter hatten) und nicht inlined werden mussten, müssen jetzt inlined werden, was wiederum zu mehr Overhead und längerem Code führt.
   * __Lösung?:__ Globaler Stack, der alle Funktionsparameter vorher übergibt und nachher wieder ausgibt.
- Arrays müssen bei Wiederverwendung neu initialisiert werden, was aber in KSP nicht direkt supported ist
  * __Lösung:__ Mache Array Assignment Lowering. Erstelle Funktion (bereits parameter promoted), die dort eingesetzt 
    wird, wo Arrays initialisiert werden, mit dem Array Typ im Namen. Mache diesen Prozess bereits vor der Umwandlung in global Scope, damit der iterator des while loops direkt durch passive Variablen ersetzt werden kann.
  
__Pre Array Assignment Lowering:__
```c
arr := (0)
```
__Post Array Assignment Lowering:__
```c
// call
declare local _iter : int
array.init.Integer(arr, _iter, 0)
// function definition
function array.init.Integer(array : int, _iter : int, value : int)
	while(_iter < num_elements(array)) 
		array[_iter] := value
		inc(_iter)
	end while
end function
```



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
declare object.warning: string := "Memory Error: No more free space available to allocate objects of type"

declare List.allocation[MAX_STRUCTS]: int[] := (0)
declare List.value[MAX_STRUCTS]: int[]             // Wert des Elements
declare List.next[MAX_STRUCTS]: int[] := (-1 )     // Verweis auf den Index des nächsten Elements
declare List.free_idx: int[] := 0                  // Zeigt auf den ersten freien Platz im Array

declare this_list: int := -1 // nil
this_list := List.__init__(42, -1)

declare that_list: int := List.__init__(39, List.__init__(40, List.__init__(41, this_list)))
message(List.value[List.next[that_list]])

function List.__init__(value: int, next: List) -> result
    List.free_idx := search(List.allocation, 0)
    if List.free_idx = -1
        message(object.warning & "'List'")        
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
