## 4. Implementing recursive Functions (Defunctionalize the Continuation)
- Transform recursive Functions into continuation passing style
- Defunctionalize by turning functions into data structures
- Allow recursive functions on these data structures by lowering them to loops
- <https://www.pathsensitive.com/2019/07/the-best-refactoring-youve-never-heard.html>

- Tail-Call Optimization implementieren?
```cksp
struct List
    declare value: int
    declare next: List
end struct

function sum_list(lst: List): int
    if lst = nil
        return 0
    else
        return lst.value + sum_list(lst.next)
    end if
end function
```
Post Lowering:
```cksp
function sum_list_iterative(lst: List): int
    declare sum: int
    sum := 0 // <- base case
    while lst # nil
        sum := sum + lst.value
        lst := lst.next
    end while
    return sum
end function
```



