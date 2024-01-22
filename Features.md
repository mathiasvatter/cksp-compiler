# Features

The following describes added/retracted features to vanilla KSP Script that differ from `nojanath's` fork of `Nils 
Liberg's SublimeKSP plugin`.

## Single-line Declarations

Use a comma separated syntax to declare `variables`, `arrays` or `ui_controls` in one line and assign values to them.
When handling `read` or `pers` keywords one has to write them explicitly in front of every variable. The same logic 
applies to `ui_control` identifiers like `ui_button` or `ui_label`.
```c
declare var1, var2, var3
declare arr1[3], arr2[3], arr3[3]
declare var1, arr1[3]

declare var1, var2, var3 := 0, 1.1, "string"                            
declare arr1[3], arr2[3], arr3[3] := (0), (4,3,2), ("3","2","1")    // declares all arrays with right-hand values
declare var1, arr1[3] := 0                                          // declares var1 und arr1 mit values 0

declare read var1, pers var2, var3 := 0, 1, 2                       // declares var1 as read and var2 as persistent
declare read ui_button btn_a, ui_label lbl_b, ui_switch swi_c[3]    // declares btn_a as a read ui_button, lbl_b as 
                                                                    // ui_label and a ui_control array swi_c

```

## Single-line Assignments

Just as declaring multiple variables in one line – one can assign values of varying types to multiple variables at once.

```c
var1, var2, var3 := 0, 1.1, "string"                            
arr1[3], arr2[3], arr3[3] := (0), (4,3,2), ("3","2","1")
var1, arr1[3] := 0  
```

## Inlining Functions everywhere

Contrary to limitations in the `SublimeKSP Plugin`, `cksp` allows for (returning) functions of varying line 
counts to be inlined into `if-statement` conditions, `for-loop` conditions or other constructs.

```c
function long_search(array, value) -> return
    declare local i
    for i := 0 to num_elements(array)-1
        if array[i] = value
            return := i
        end if
    end for
end function

on init
    declare arr[2], val := (2, 1), 1
    if long_search(arr, val) = long_search(arr, 2)
        message("same values")
    end if
end on
```

## Local Variables on callback level



