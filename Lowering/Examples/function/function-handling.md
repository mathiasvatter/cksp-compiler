# Compiler Rules for Parameter Handling in cksp (Strategy: Inlining for `ref`)

## 1. Fundamental Principle for `ref` Parameters: True Alias Semantics via Inlining

* **Goal:** When a `cksp` user declares a parameter with `ref`, the expectation is that modifications to this parameter within the function directly affect the original variable passed by the caller (alias behavior).
* **Primary Method:** To achieve this in KSP, function inlining is employed. The previously discussed copy-in/copy-out simulation for `ref` parameters (using global helper variables) is superseded by this inlining strategy.

## 2. Rules for Function Calls and Parameter Treatment:

### Case A: Function is Inlined Due to `ref` Parameters (when the call is *not* from within `on init`)

* **Condition:** The called `cksp` function declares at least one `ref` parameter in its signature.
* **Compiler Action:** The function call is replaced by the full body of the called function (inlining).
    * **Handling of `ref` Parameters:** Every occurrence of a formal `ref` parameter (e.g., `p_ref`) within the function body is directly substituted with the *actual argument* (the L-value, i.e., the "name" or accessor of the variable passed, e.g., `%array[$idx]`) from the call site. All operations on `p_ref` in the function's code thus operate directly on the caller's original variable.
    * **Handling of Non-`ref` Parameters (Pass-by-Value):** For every formal parameter that is *not* declared as `ref` (e.g., `p_val`):
        1.  The actual argument expression (e.g., `x * 2` or `$some_var`) is evaluated *once*.
        2.  The resulting value of this evaluation is assigned to a newly created, temporary KSP variable. This temporary variable receives a unique name (see Name Mangling) to avoid conflicts and represents the formal parameter `p_val` exclusively for this specific inlined call.
        3.  Every occurrence of `p_val` in the function body is substituted with this temporary KSP variable. This ensures that pass-by-value semantics (the value is passed once) are maintained.

### Case B: Function is *Always* Inlined Due to a Call from `on init`

* **Condition:** The function call occurs within an `on init` callback.
* **Compiler Action:** The function call is *always* inlined, regardless of whether the function has `ref` parameters or not.
    * **Handling of `ref` Parameters (if any):** Treated as described in Case A (direct substitution of the argument).
    * **Handling of Non-`ref` Parameters:** Treated as described in Case A (argument is evaluated and assigned to a unique temporary KSP variable for the inlined block).

### Case C: Function is *Not* Inlined (Standard KSP `call` Mechanism)

* **Condition for this case:**
    1.  The called `cksp` function has *no* `ref` parameters in its signature, AND
    2.  The function call does *not* occur within an `on init` callback.
* **Compiler Action:** The function is invoked using the standard KSP command `call function_name`.
    * **Handling of Parameters (must all be non-`ref`):** For each formal parameter `p_val` of the function:
        1.  The corresponding actual argument from the call site is evaluated.
        2.  The resulting value is assigned to the formal pararmeter (which is turned into a global KSP variable and uniquely renamed) This assignment must occur immediately before the KSP `call` command.
        3.  Within the KSP function, the value of the parameter is then read from this global placeholder variable.

## 3. Necessary Compiler Actions and Techniques During Inlining:

* **Substitution of Parameters:** As detailed in Case A and Case B above.
* **Gensym:** All formal parameter variables are turned into global ones and given unique names by a global Gensym instance (by appending a suffix). This is crucial to prevent name collisions with variables in the calling context or with variables from other inlined function calls.
* **Handling of `return` Statements:** `return` statements are handled by transformations beforehand  (ReturnParameterPromotion) and are at this stage already parameters.

## 4. Important Limitations and Considerations of this Strategy:

* **Code Bloat:** Frequent inlining, especially of large functions or functions called very often, can significantly increase the total size of the generated KSP script. 
