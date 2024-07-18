
# Bachelor Compilerbau

## 1. Lexical Scope to Global Scope
- Checking of scopes and variable shadowing
- rename local variables to get global scoping
- reuse variables depending on their dynamic extend
- do parameter promotion (lambda lifting?) in functions
- (turn local variables into global array?)

### Control Flow in vanilla KSP

- nicht viele offizielle Quellen. Inoffizielle, aber gängige Quelle: Task Module von 'Big Bob'.
- KSP besteht aus verschiedenen Callbacks, die in beliebiger Reihenfolge (beeinflusst du User Input) auftreten können (außer der 'on init' Callback, dieser wird ganz zu Anfang ausgeführt und enthält alle Variablen Deklarationen).
- Ein Callback wird immer komplett durchlaufen, bevor der nächste ausgeführt wird, Variablen, die in mehreren Callbacks referenziert werden, behalten also ihre Werte während eines Callback Durchlaufs, müssten bei Mehrfachverwendung also nur zu Beginn initialisiert werden.
- Es gibt Ausnahmen: wird die `wait` Funktion, oder eine andere asynchrone Builtin Funktion in einer Funktion oder Callback ausgeführt, kann währenddessen ein anderer Callback ausgeführt werden -> es kann zu race conditions zwischen mehrmals verwendeten Variablen kommen.

```
// callback
on init // gets evaluated first
    /* 
     * control flow statements and expressions
     */
end on

// callback
on persistence_changed
    /* 
     * control flow statements and expressions
     */
end on
```

### Vorgehen:

#### 1. Variablen und Scopes tracken (Callbacks und Funktionsdefinitionen)
- Nehme einen Stack aus Maps an Variablen, pro Scope wird eine neue Map in den Stack eingesetzt, die in Deklarationen mit Variablen/Arrays gefüllt wird.
- Wird ein Scope verlassen, wird die oberste Map gelöscht.
- jede Variable Reference erhält einen Pointer auf ihre Deklaration in der Map

#### 2. Wiederverwendung von Variablen/Arrays sobald Scope abgelaufen ist. Deklarationen werden durch Assignments ersetzt. (Funktionen)
- Erstelle eine Map aus "**passive variables**" mit Hashwerten aus Variablen Typen (und Array sizes). Sobald ein Scope verlassen wird, werden die Variablen (deren dynamic extend nun abgelaufen ist) in die Map eingetragen.
- Bei jeder neuen Deklaration wird geprüft, ob eine Variable mit gleichem Typ bereits in der Map aus passiven Variablen ist. Falls ja, wird die Variable durch eine Referenz auf die Variable aus der Map ersetzt.
- Ersetzte Deklarationen werden durch Assignments (mit neutralen Elementen oder den bereits zugewiesenen values) ersetzt.

#### 3. Alle lokalen Variablen mit Gensym umbenennen. (Funktionen)
- Tracke alle Variablen und Referenzen in Listen und benenne sie mit Gensym um, damit kein Variable Capturing stattfindet, falls eine der freien "passiven Variablen" gleich heißt wie eine Variable im Scope.
     
#### Beispiel:
__Pre Register Reuse:__
```
function scoped_func2()
    declare i: int // <- ist das ganze scope über sichtbar, wird nie zu passiver Variable
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
__Post Register Reuse:__
```
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
#### 4. Parameter Promotion/(Lambda Lifting?) mit allen Funktionen durchführen, bis alle Variablen in Callbacks sind.
- Führe die ersten drei Schritte zunächst nur mit den Funktionsdefinitionen durch.
- Besuche nun jeden Function Call bis zum letzten nested call, füge die lokalen Deklarationen als neue Parameter und in eine Map mit pointer auf die nächst höhere Funktionsdefinitionen ein.
- Wiederhole dieses Vorgehen bis der Function Call Stack leer ist und füge die Deklarationen in den entsprechenden Callback ein. Nach Einfügen der Deklarationen in den Callback wird die entsprechende Map geleert, sodass bei mehrmaligem Aufrufen der gleichen Funktion nicht die gleichen Variablen mehrfach deklariert werden
      
#### Beispiel:
__Pre Parameter Promotion:__
```
on persistence_changed // <- callback
    declare array[3] := (1,2,5)
    scoped_func2()
    scoped_func2()
end on
```
__Post Parameter Promotion:__
```
on persistence_changed
    declare array[3] := (1,2,5)
    declare loc_1: int
    declare loc_2: int[]
    declare loc_3: int
    scoped_func2(loc_1, loc_2, loc_3)
    scoped_func2(loc_1, loc_2, loc_3)
end on
```
#### 5. Wiederhole Punkt 2 und 3 ausschließlich mit Callbacks.
- Führe die Schritte durch und ersetze die Deklarationen in den Callbacks durch passive Variablen.
- Speichere die restlichen Deklarationen in einer Liste
- Füge alle Deklarationen in den init Callback ein.
- Umbenennung mit Gensym.


### Herausforderungen:
- Array sizes sind nicht unbedingt bekannt, die Wiederverwendung von Arrays hängt derzeit nur an Typ und Dimension -> können nicht sehr effizient reused werden.
    * __Lösung:__ Füge zum Hash Wert in der Map der passiven Variablen nicht nur den Typ, sondern auch Größe und 
      andere Faktoren hinzu, die die Wiederverwendung von Arrays beeinflussen (persistence, const).
    * __Problem:__ Größe kann auch Expression sein oder Konstante (womöglich vorher constant propagation machen?)
- __TO-DO__: Lambda Lifting bringt Overhead an Funktionsparametern mit sich -> Funktionen die vorher gecallt werden konnten 
(also keine Parameter hatten) und nicht inlined werden mussten, müssen jetzt inlined werden, was wiederum zu mehr Overhead und längerem Code führt.
   * __Lösung?:__ Globaler Stack, der alle Funktionsparameter vorher übergibt und nachher wieder ausgibt.
- Wenn ein asynchroner Befehl in einem Callback (außer 'on init') oder einer Funktion vorkommt (und damit auch im Callback), ist dieser nicht mehr 'threadsafe' und die gesamte beschriebene register usage Optimierung kann nicht mehr vorgenommen werden.
    * __Lösung:__ Wie bereits bei den Funktionsparameter, könnte ein globaler Stack helfen, der bei jeder asynchronen instruction eine Kopie des derzeitigen Environments macht.
- Nach Parameter Promotion könnten die parameter mit den lokalen variablen der übergeordneten Funktion verglichen werden, um noch mehr Reuse zu ermöglichen.
- Arrays müssen bei Wiederverwendung neu initialisiert werden, was aber in KSP nicht direkt supported ist
  * __Lösung:__ Mache Array Assignment Lowering. Erstelle Funktion (bereits parameter promoted), die dort eingesetzt 
    wird, wo Arrays initialisiert werden, mit dem Array Typ im Namen. Mache diesen Prozess bereits vor der Umwandlung in global Scope, damit der iterator des while loops direkt durch passive Variablen ersetzt werden kann.
  
__Pre Array Assignment Lowering:__
```
arr := (0)
```
__Post Array Assignment Lowering:__
```
// function call
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

```
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
```

declare const MAX_STRUCTS := 1000000
declare object.warning: string := "Memory Error: No more free space available to allocate objects of type"

declare const List.MAX_STRUCTS := MAX_STRUCTS // can alternate if mutlidimensional array is needed
declare List.allocation[MAX_STRUCTS]: int[] := (0)
declare List.value[MAX_STRUCTS]: int[]             // Wert des Elements
declare List.next[MAX_STRUCTS]: List[] := (nil)      // Verweis auf den Index des nächsten Elements
declare List.free_idx: int := 0                  // Zeigt auf den ersten freien Platz im Array

declare $this_list := nil // nil
$this_list := List.__init__(42, nil)

declare $that_list := List.__init__(39, List.__init__(40, List.__init__(41, this_list)))
message(%List.value[%List.next[$that_list]])

function List.__init__(value, next) -> result
    List.free_idx := search(List.allocation, 0)
    if List.free_idx = -1
        message(object.warning & "'List'")        
    end if
    List.allocation[List.free_idx] := 1
    List.value[List.free_idx] := value
    List.next[List.free_idx] := next
    result := List.free_idx
end function
```

### Vorgehen:
1. Tokenizer erweitern. Reserviere folgende keywords: struct, end struct, new, delete, nil. Tokenizer erweitern im Umgang mit "." (`token::DOT`), gefolgt von `token::KEYWORD` für method chaining.
2. Parser erweitern. Erstelle neue ASTDataStructures Subclass ASTStruct. Erstelle parsing rules für structs. Erstelle ASTDataStructure Pointer, sowie ASTReference PointerRef und AccessChain mit vektor für chained methods und members, inklusive parsing rules.
3. Erstelle Parsing rules für NodeNil
4. Erst desugaring und "namespace apply", damit Referenz und Deklaration Zuweisung innerhalb der Struct Hierarchie funktioniert.

__Pre-desugaring__:
```
struct List
    declare value: int
    declare next: List := nil

    function __init__(self, value: int, next: List)
        self.value := value
        self.next := next
    end function
end struct
```
__Post-desugaring__:
```
struct List
	declare List.value: int
	declare List.next: List := nil

	function List.__init__(value: int, next: List)
		List.value := value
		List.next := next
	end function
end struct
```
5. Lowering der Structs in Arrays. Erstelle für jede struct Deklaration ein Array, das die Werte der Structs enthält. Erstelle für jede struct Deklaration ein Array, das die Verweise auf die nächsten Elemente enthält.
Wenn das struct bereits ein oder mehrere Arrays enthält, wird ein multidimensionales array erstellt, dessen neue Dimension die maximale Größe aus den bisherigen Arrays ist.
6. Erweitere Typsystem mit dynamischen Typen für Objekte.

## 3. Allow recursive Functions (Defunctionalize the Continuation)
- Transform recursive Functions into continuation passing style
- Defunctionalize by turning functions into data structures
- Allow recursive functions on these data structures by lowering them to loops
- <https://www.pathsensitive.com/2019/07/the-best-refactoring-youve-never-heard.html>
