## 2. Lowering von rekursiven Datenstrukturen in CKSP

- Neue ASTNode Type Pointer, PointerRef, Nil, AccessChain
- Neuer Typ: Object

### Eingangs struct:

```cksp
struct List
    declare value: int
    declare next: List := nil

    function __init__(self, value: int, next: List)
        self.value := value
        self.next := next
    end function

    function get_next_next_value(self)
        return self.next.next.value
    end function
end struct
```

### 1. Erste Transformation (Desugaring):
- prüft, ob `__init__` oder `__repr__` Methode definiert sind, definiert sie ansonsten selbst
- Fügt self keyword hinzu, um die self referenzen einer DataStruct Node zuzuweisen
- Checkt, ob die Methoden alle als erstes das self keyword besitzen
- Checkt, ob Operator overload existiert
- Fügt zu allen membern den namen des struct als prefix hinzu, genauso wie zu den member referenzen, ersetzt hier das `self.` prefix. Dabei werden `::` benutzt, damit kein Variable Shadowing stattfinden kann, da `:` nicht supported ist im Tokenizer.

```cksp
declare const MAX_STRUCTS : int := 1000000
declare const MEM_WARNING : string := "Memory Error: No more free space available to allocate objects of type"
struct List
	declare self : List
	declare List::value : int
	declare List::next : List

	function List::__init__(value : int, next)
        List::value := value
        List::next := next
	end function

	function List::get_next_next_value(self : List)
	return (self.next.next.value)
	end function

	function List::__repr__(self : List)
		return ("<List> Object: " & self{List})
	end function

end struct
```

### 2. Type Inference
- Typen aller referenzen und ihrer Deklaration werden versucht zu inferieren, sofern sie nicht annotiert wurden.
- Typen der einzelnen Member einer Access Chain Node werden inferiert wie folgt:
    - Sofern der Typ des ersten Members inferiert wurde und dieser ein Object Type ist, wird im entsprechenden Struct der Name des darauffolgenden Access Chain Members mit denen der Struct Member verglichen und der Typ dieses Struct Members als nächster Type in der Access Chain verwendet.

```cksp
self{List}.next{List}.next{List}.value{int}
```

### 3. Zweite Transformation (PreLowering):
- Da in diesem Schritt durch die Syntax Analysis bereits alle Daten Typen erkannt wurden, werden die Struct Member nach verbotenen DatenTypen untersucht (UI Control variablen)
- Allgemeine für den Compiler wichtige Variablen werden pro Struct hinzugefügt: das allocation array, das stack array, die stack_top variable und die free_idx variable.
- Rekursive structs werden erkannt, indem alle Member typen (sofern sie von Typ Object sind) in eine map mit der Anzahl ihrer Vorkommnisse geschrieben werden. Dann wird rekursiv das struct eines jeden members untersucht und diese auch in die map aufgenommen. Sobald der Count eines Objekt typs in der map auf zwei anwächst oder keine member mehr übrig sind, wird die map im struct gespeichert.
- Die maximale anzahl des struct wird festgelegt durch Division der gesamten möglichen Structs und der Länge des größten Arrays im Struct.

### 4. Dritte Transformation (Lowering):
- Alle member und member referenzen werden um eine Dimension erhöht: Variablen werden zu Arrays, Arrays zu NDArrays usw.
- Als index bekommen sie eine referenz zu self und sofern sie im Constructor sind, eine referenz zu free_idx
- zum Constructor werden das allocation array und die variable free_idx hinzugefügt
- Pointer -> Variable, PointerRef -> VariableRef, Nil -> -1
- Alle Objekt Typen werden zu Integer Typen.
- Access Chains werden umgeformt, indem vom hintersten member der chain angefangen wird. Auch hier wird alles außer der erste Member um eine Dimension erhöht, sodass eine Array Kette entsteht, in deren innersten der zuvor am Anfang stehende Chain member steht.
    - Ist der letzte Member ein Method Call, wird der Rest der Chain als argument genutzt.
- Außerdem werden Methoden für Reference Counting angelegt.

```cksp
struct List
	declare const List::MAX_STRUCTS : int := MAX_STRUCTS
	declare List::free_idx : int
	declare List::allocation[List::MAX_STRUCTS{int}] : int[]
	declare List::stack[List::MAX_STRUCTS{int}] : int[]
	declare List::stack_top : int
	declare List::value[List::MAX_STRUCTS{int}] : int[]
	declare List::next[List::MAX_STRUCTS{int}] : int[] := [- 1]

	function List::__init__(value : int, next : int)
		List::free_idx{int} := search(List::allocation{int[]}, 0)
		if(List::free_idx{int} = -1)
			message("Error: No more free space available to allocate objects of type 'List'")
		end if
		List::allocation[List::free_idx{int}]{int[]} := 1
		List::value[List::free_idx{int}]{int} := value{int}
		List::next[List::free_idx{int}]{int} := next{int}
		return (List::free_idx{int})
	end function

	function List::get_next_next_value(self : int)
		return (List::value[List::next[List::next[self{int}]{int}]{int}]{int})
	end function

	function List::__repr__(self : int)
		return ("<List> Object: " & self{int})
	end function
end struct
```

## 3. Reference Counting Algorithmus Implementation

- Das allocation array eines jeden struct zählt wie oft eine bestimmte struct instanz referenziert wird
- Es wird im Constructor auf 1 gesetzt und dann bei assignments und declarations reduziert bzw erhöht.
- Erhöhungen werden symbolisiert durch Retain Nodes
- Reduzierungen werden symbolisiert durch Delete Nodes

1. Wenn lokale Pointer oder Arrays of Pointers deklariert werden, werden sie in einer map gespeichert. Für jeden Pointer in dieser Map wird am Ende des Scopes/Blocks eine Delete Node eingefügt und die map bereinigt.
2. Bei Deklarationen von Pointern oder Array of Pointers, die ein Assignment haben welches ein anderer Pointer oder ein Array of Pointers ist, wird danach eine Retain Node eingefügt mit der Anzahl der assigned values, um den reference count zu erhöhen.
```cksp
// ------ initializer list of a single object
declare lists[10]: List[] := (ls)
retain(lists[0], 10) // increase ref count by num_elements of lists
```
3. Bei Assignments von Pointer bzw AoP zu anderen Pointern/AoP wird davor eine Delete Node für das l_value des Assignments eingefügt, danach eine Retain Node, da das vorherige value nicht mehr referenziert wird, der reference count des neu assignten values sich aber erhöht.
```cksp
    delete lists // delete complete array in for loop
    lists := (ls) // <-
    retain(lists, num_elements(lists))
```
4. Nach dem Lowering Prozess werden die Retain Nodes durch Function Calls zu den `__incr__` Methoden ersetzt und die Delete Nodes durch Function Calls zu den `__decr__` Methoden.

### Creating Reference Counting Methods for recursive Structures

- Folgende Methoden werden pro struct erstellt:
    - __incr__(ptr): Erhöht den Reference Count einer struct instanz.
    ```cksp
    function List::__incr__(self: List, num_refs: int)
        if self # nil
            List::allocation[self] := List::allocation[self] + num_refs
        end if
    end function
    ```
    - `__decr__`(ptr): Reduziert den Reference Count einer struct instanz, sobald der rc bei <=0 liegt, wird die Methode auch auf alle rekursiven member angewendet und der Pointer selbst auf nil gesetzt. Damit ist der Speicherplatz im allocation array wieder frei für eine neue struct instanz
    - `__del__`(ptr): Kümmert sich um das eigentliche löschen, setzt im Falle von rekursiven struct membern diese auf nil
    ```cksp
    function List::__del__(self: List)
        if(List::allocation[self] <= 0)
            // all object struct members       
            List::next[self] := nil
        end if 
    end function
    ```
- Da KSP keine rekursiven Funktionen erlaubt, muss `__decr__` mittels Iteration und stack implementiert werden.

Für die `__decr__` Methode gibt es drei unterschiedliche Implementationsmöglichkeiten, je nach Art der rekursion.

### 1. Methode 1 (Lineare direkte Rekursion)
    - Voraussetzungen: Struct hat maximal einen direkt rekursiven member
    - Die Methode kann auf einen stack verzichten und somit Speicherplatz sparen.
```cksp
struct List
    declare value: int
    declare next: List
end struct
function List::__decr__(self: List, num_refs: int)
    declare current: List := self
    // continue iterating as long as there are object members
    while current # nil

        // lower reference count
        List::allocation[current] := List::allocation[current] + num_refs
        // object still referenced, only delete this ptr
        if(List::allocation[current] > 0) 
            current := nil
        	continue
        end if

        // delete methods of other non-rekursive object members

        self := current
        if(List::next[current] # nil)
            current := List::next[current]
        else
            current := nil
        end if
        
        // actual delete
        List::__del__(self)
    end while
end function  
```

### 2. Methode 2 (Nicht-lineare direkte Rekursion)
    - Voraussetzungen: Struct hat mehrere direkt rekursive member
    - Die Methode setzt einen stack und eine stack_top variable voraus
```cksp
struct Node
    declare value: int
    declare left: Node := nil
    declare right: Node := nil
end struct
declare Node::stack[MAX_STRUCTS]: Node[]
declare Node::stack_top: int := 0
function Node::__decr__(self: Node, num_refs: int)
    Node::stack[Node::stack_top] := self
    inc(Node::stack_top)

    // Solange der Stack nicht leer ist
    while Node::stack_top > 0
        dec(Node::stack_top)
        self := Node::stack[Node::stack_top]

        if self = nil
            continue
        end if

        // lower reference count
        Node::allocation[self] := Node::allocation[self] + num_refs
        // object still referenced
        if(Node::allocation[self] > 0) 
        	continue
        end if

        // füge left dem stack hinzu
        if Node::left[self] # nil
            inc(Node::stack_top)
            Node::stack[Node::stack_top] := Node::left[self]
        end if

        // füge right dem stack hinzu
        if Node::right[self] # nil
            inc(Node::stack_top)
            Node::stack[Node::stack_top] := Node::right[self]
        end if

        // wenn self nicht mehr referenziert wird, lösche alle member
        Node::__del__(self)

    end while
end function
function Node::__del__(self: Node)
    if(Node::allocation[self] <= 0)
        Node::left[self] := nil
        Node::right[self] := nil
    end if
end function
```

### 3. Methode 3 (nicht-lineare indirekte Rekursion)
    - Voraussetzungen: Struct hat ein oder mehrere indirekt rekursive Member, bspw. Member, die den Typ eines anderen struct haben, welches aber wiederum Member hat, die den Typ des ersten struct haben.
```cksp
struct List
    declare value: int
    declare next: Node
end struct
struct Node
    declare value: int
    declare left: List
    declare right: List
end struct
function List::__decr__(self: List, num_refs: int)
    // Füge das initiale Objekt dem entsprechenden Stack hinzu
    List::stack[List::stack_top] := self
    inc(List::stack_top)

    // Hauptschleife zur Verarbeitung der Stacks
    while List::stack_top > 0 or Node::stack_top > 0
        // Verarbeite List-Stack
        while List::stack_top > 0
            dec(List::stack_top)
            self := List::stack[List::stack_top]

            if self = nil
                continue
            end if

            // Schritt 1: Referenzzähler reduzieren/erhöhen (je nach num_refs)
            List::allocation[self] := List::allocation[self] + num_refs

            // Wenn das Objekt noch referenziert wird, überspringe die Löschung
            if List::allocation[self] > 0
                continue
            end if

            // Hier kommen Delete-Methoden von anderen Membern hin, die nicht vom Typ Node sind

            // Füge rekursive Member hinzu
            if List::next[self] # nil
                // Füge das Node-Objekt dem Node-Stack hinzu
                inc(Node::stack_top)
                Node::stack[Node::stack_top] := List::next[self]
            end if

            List::__del__(self)
        end while

        // Verarbeite Node-Stack
        while Node::stack_top > 0
            dec(Node::stack_top)
            self := Node::stack[Node::stack_top]

            if self = nil
                continue
            end if

            // Schritt 1: Referenzzähler reduzieren
            dec(Node::allocation[self])

            // Wenn das Objekt noch referenziert wird, überspringe die Löschung
            if Node::allocation[self] > 0
                continue
            end if

            // Hier kommen Delete-Methoden von anderen Membern hin, die nicht vom Typ List sind

            // Füge rekursive Member hinzu
            if self.left # nil
                // Füge das List-Objekt dem List-Stack hinzu
                inc(List::stack_top)
                List::stack[List::stack_top] := self.left
            end if

            if Node::right[self] # nil
                inc(List::stack_top)
                List::stack[List::stack_top] := Node::right[self]
            end if

            Node::__del__(self)
        end while
    end while
end function
```
