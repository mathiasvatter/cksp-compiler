# Features

## Single-line Declarations

Use a comma separated syntax to declare variables, arrays or ui_controls in one line and assign values to them.

```javascript
declare var1, var2, var3
declare arr1[3], arr2[3], arr3[3]
declare var1, arr1[3]

declare var1, var2, var3 := 0, 1.1, "string"                            
declare arr1[3], arr2[3], arr3[3] := (0), (4,3,2), ("3","2","1")    // declares all arrays with right-hand values
declare var1, arr1[3] := 0                                          // declares var1 und arr1 mit values 0

declare read var1, pers var2, var3 := 0, 1, 2                       // declares var1 as read and var2 as persistent
declare read ui_button btn_a, ui_label lbl_b, ui_switch swi_c[3]    // declares btn_a as a read ui_button, lbl_b as ui_label and a ui_control array swi_c

```

## Single-line Assignments





