# Features

The following describes features to `vanilla KSP Script` that differ from `nojanath's` fork of `Nils 
Liberg's SublimeKSP plugin`.

## Pragma Directive

`cksp` currently holds the option to specify the desired output path via `#pragma output_path("path/to/file.txt")`. The output file has to be of type `*.txt`.
The pragma directives are project wide, meaning a `#pragma` later on in the imported files could potentially overwrite an earlier directive.
The output path specified with the `-o` option in the command line always has precedence over any `#pragma output_path` directive.

## Global Preprocessor Conditions

Preprocessor directives `SET_CONDITION` and `RESET_CONDITION` are project wide and not on a per file basis. A condition `DEV_STATE` set in one file can be used to edit code blocks with `USE_CODE_IF` or `USE_CODE_IF_NOT` in all imported `*.ksp` files.

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
var1, arr1[3] := 0                                                  // assigns 0 to var1 and to every element of arr1
```

## Inlining Functions everywhere

Contrary to limitations in the `SublimeKSP Plugin`, `cksp` allows for (returning) functions of varying line counts to be
inlined into `if-statement` conditions, `for-loop` conditions or other constructs.

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

## Scoped Local Variables

Local variables can be declared in every scope. They retain their value while inside the scope and can not be accessed 
anymore when outside the scope.
They are thread safe since every callback gets its own section in the internal local variable array.
_Note: local arrays are currently not supported in `version 0.0.2`_

```c
on init
    declare i, j := 5
    for i := 0 to 4
        declare local j := 0
        message(j)                             // prints 5 in for every iteration
        message("sth")
    end for
    message(j)                                 // prints 5 since it refers to the j declared before the for-loop
    
    if 3 < 5                     
        declare local j := 1                  
        message(j)                             // prints 1 since it refers to the j declared in the if-statement scope
    else
        declare local j := 2
        message(j)                             // prints 2 since it refers to the local j declared in the else scope
    end if
    message(j)                                 // prints 5 again since it refers to the j declared before the 
                                               // for-loop
end on

on controller
    declare local j := 9
    message(j)                                 // prints 9 since j was declared locally on the controller callback level
end on
```

## Range-Based For-Loops

`cksp` introduces a new syntax to iterate over array elements using `key, value` pairs. The `key, value` variables do not 
have to be declared beforehand and only exist within the scope of the `for-loop`.

```c
declare array[5] := ("3","4","6","8","10")      // is equivalent to:
for key, val in array                           // for key := 0 to num_elements(array)-1
    message(key & ", " & val)                   // message(key & ", " & array[key])
end for
```

## Default Case in Select Statements

Since `vanilla KSP` is missing a shorthand for a `default case` in `select` statements, `cksp` implements the `default` 
keyword which gets substituted to cover the whole range of a _32-bit integer_ (`-2147483648 to 134217727`).

```c
select (CC[VCC_PITCH_BEND])
    case -8191 to -1
        message("Pitch Bend down")
    case 0
        message("Pitch Bend center")
    case 1 to 8191
        message("Pitch Bend up")
    case default
        message("We're not sure how you got this Pitch Bend value!")
end select
```
