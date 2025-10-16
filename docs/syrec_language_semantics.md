# Semantics of the SyReC language

Both the original paper introducing the syntax and first semantic properties of the SyReC language {cite:p}`wille2016syrec` and the RevLib project {cite:p}`wille2008revlib` adding some further semantic properties will serve as references for the remainder of this page and we are assuming that the reader has read the relevant sections in both documents before continuing.
The goal of this document is to not only specify original semantics of the language SyReC but also include extensions where a reference specification was missing or left room for interpretation.

```{note}
While the SyReC parser implements the semantics specified on this page, some properties might only be valid for this project due to the previously mentioned gaps in the reference specification.
```

We start by defining the semantics of the highest-level entity of a SyReC program, a so called _Module_, and work our way down to the most basic elements of the language:

## Module

- The RevLib project (v2.0.1, section 2.1) {cite:p}`wille2008revlib` states that the entry point of a well-formed SyReC program is either defined explicitly by a module with an identifier _main_ or is implicitly chosen as the last defined module of the program. Additionally, one can also specify the identifier of the module serving as the entry point of the SyReC program in the {doc}`parser configuration <library/ConfigurableOptions>`. With such an entry configured, only one module matching said identifier is allowed in the SyReC program (the same restriction applies if no such entry was configured and a module with identifier 'main' was defined).

- Omitting the specification of the dimensionality of any variable (i.e., the number of dimensions and the number of values per dimension) will cause the variable to be considered as a 1D variable storing a single value; The following example shows the two equivalent specifications:

  ```text
  // Omitting dimensionality of variable 'a'
  module main(inout a(4))
    ...

  // Explicit specification of dimensionality of variable 'a'
  module main(inout a[1](4))
    ...
  ```

- The value of every variable with bitwidth {math}`b` is assumed to be an unsigned integer and thus must be in the range {math}`[0, 2^{b} - 1]`.
- The maximum supported bitwidth of any variable is equal to 32.
- If the bitwidth of a variable is not declared then it is assumed to be equal to a {doc}`configurable default value <library/ConfigurableOptions>`.
- The parameter and variable identifiers must be unique in a SyReC module.
- If the synthesized version of a SyReC program should be compatible with the OpenQASM 2.0 standard, variable identifiers must start with one of the following characters: {code}`[a-zA-Z]` {cite:p}`cross2017openquantumassemblylanguage`.
- The identifier of the module and any of its parameters and local variables cannot start with the prefix {code}`__q` that is reserved for internal use.
  - Module overloading (i.e., the definition of a module sharing its identifier with another module while the signature [variable type, dimensionality and bitwidth] of their parameters do not match) is supported for all modules whose identifier is not equal to the one of the "main" module. However, overloading the implicitly defined main module of a SyReC program is possible.

    Two module signatures (module identifier + parameters) {math}`m_1` and {math}`m_2` are considered to be equal iff:
    - The module identifiers match
    - Both modules define the same number of parameters {math}`n`
      - For each parameter {math}`i: 0 \leq i < n` in the sequence of the formal parameters of both modules the following holds:
        - The type of parameter {math}`p_i` of module {math}`m_1` allows for an assignment of the parameter at the same position in the formal parameter list of module {math}`m_2` with the following table listing the assignability between the different SyReC variable types:

          ```{eval-rst}
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          |                                               | **Assigned from variable type**            |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | **Assigned to variable type**                 | *in*  | *inout* | *out* | *wire* | *state* |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | *in*                                          | X     | X       | X     | X      | X       |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | *inout*                                       | o     | X       | X     | X      | o       |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | *out*                                         | o     | X       | X     | X      | o       |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | *wire*                                        | o     | X       | X     | X      | o       |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | *state*                                       | o     | o       | o     | o      | o       |
          +-----------------------------------------------+-------+---------+-------+--------+---------+
          | o ... does not allow assignment                                                            |
          |                                                                                            |
          | X ... allows assignment                                                                    |
          +--------------------------------------------------------------------------------------------+
          ```

          - The number of dimensions match
          - The number of values for each of the defined dimensions matches
          - The bitwidth of the parameters match

    The signature of a module is expected to be unique in a SyReC program.

- The maximum number of values storable in a dimension is equal to {math}`2^{32}`.
- A variable can have at most {math}`2^{32}` dimensions.
- The total number of elements storable in a variable is equal to {math}`2^{32}`, with 'an element' referring to _n_ consecutive qubits of the variable with _n_ being equal to the declared bitwidth of the variable (e.g. a variable declared as {math}`in \ a[2][3][4](6)` contains 24 6-qubit sized elements).
- The bitwidth of a variable must be larger than zero.
- The number of values for any dimension of a variable must be larger than zero.
- Variables of type 'state' require special handling and are treated as read-only in a SyReC program with the initial state being set during synthesis.
  Both of the available SyReC specifications, namely the original SyReC paper {cite:p}`wille2016syrec` as well as in the RevLib documentation {cite:p}`wille2008revlib`, only mention potential use-cases but fail to specify how the value transitions for these variables should be implemented.
  Thus, the support for 'state' variables in SyReC should be considered as prototypical with future version potentially offering better support or more concise specifications of the expected semantics.

## Statements

### Assignments

- To guarantee the reversibility of any assignment, the assigned-to variable parts cannot be accessed on the "other" (right-hand side of a non-unary assignment or left/right side of a variable swap) of the assignment. While this restriction is applied to all _VariableAccesses_ on the "other" side of the assignment, the restriction does not apply to the _VariableAccesses_ defined in the dimension access of any _VariableAccess_. The parser can only detect an overlap between two _VariableAccess_ instances {math}`l_{varA}` and {math}`r_{varA}` if the following conditions hold:

  ```{note}
  Loop variables are not evaluated to their current value in the following checks
  ```

  - The identifier of the accessed variables match
  - Assuming that {math}`l_{varA}` defines the indices {math}`l_{dimIdxs} = \{l_1, l_2, \dots, l_n\}` in its dimension access while {math}`r_{varA}` accesses the indices {math}`r_{dimIdxs} = \{r_1, r_2, \dots, r_n\}`, an overlap in the {math}`i`-th dimension is detected iff:
    - Both {math}`l_i` and {math}`r_i` evaluate to a constant at compile time and {math}`l_i = r_i`
    - An overlap was detected for all indices {math}`j` at positions {math}`0 < j < i` in the sequence of indices of the dimension accesses

      ```{note}
      Note that only {math}`min(len(l_{dimIdxs}), len(r_{dimIdxs}))` indices of the dimension accesses are checked
      ```

  - If an overlap in the dimension access was detected, the accessed bitranges of {math}`l_{varA}` (represented by the pair ({math}`l_{bitS}`, {math}`l_{bitE}`)) and of {math}`r_{varA}` (represented by ({math}`r_{bitS}`, {math}`r_{bitE}`)) are checked for an overlap using the following conditions:
    - All indices in both bitranges evaluate to constants and an overlap between the two ranges is detected.
    - A bit of each _VariableAccess_ evaluates to a constant and their values match.
    - A bit of one _VariableAccess_ evaluates to a constant while both indices of the accessed bit range of the other _VariableAccess_ evaluate to constants athen n overlap is reported if the bit range with known bounds overlaps said bit.

      ```{note}
      Out-of-range index values are not treated differently than values that are in range.
      A not specified bit range access of a _VariableAccess_ is assumed to access the full bitwidth of the referenced variable.
      ```

- While access on the assigned-to variable parts is, as previously described, not allowed in certain parts of an assignment, handling overlaps with the assigned-to variable parts in the dimension access of a _VariableAccess_ (as shown in the example below) needs special consideration:

  ```text
  module main(inout a(4), in b[3](2))
    a[0].1:2 += b[(a[0].0:2 + 2)]
  ```

  The reversibility of the assignment depends on whether the expression in the dimension access on the right-hand side of the assignment can be synthesized without leading to an assignment in which a qubit is assigned to itself (i.e. _a[0].1 += a[0].1_). Thus, the user must specify via the {doc}`parser configuration <library/ConfigurableOptions>` whether an overlapping access on the assigned to variable parts is allowed in a dimension access. By default, such an access is assumed to be not allowed. The same restrictions also apply to both sides of a _SwapStatement_ with the validity of the _SwapStatements_ in the example below depending on the used parser configuration.

  ```text
  module main(inout a(4), in b[3](2))
    b[(a[0].0:2 + 2)] <=> a[0].1:2;
    a[0].1:2 <=> b[(a[0].0:2 + 2)]
  ```

````{note}

The overlap checks in many cases require that the checked indices evaluate to constant values at compile time (with the value of loop variables not being evaluated [except for loops performing a single iteration]) while in all other cases either no or a potential overlap is reported. Additionally, the parser will only report overlaps detected at compile time thus no overlap being reported does not indicate the absence of an overlap, as the following example shows:

```text
module main(inout a(4))
  for $i = 0 to (#a - 1) do
    a.0 += (a.$i + 2)
  rof
```

The parser will not report an overlap in the assignment due to the index of the accessed bit in the _VariableAccess_ on the right-hand side of the assignment not evaluating to a constant at compile time. However, the first iteration of the loop will generate an assignment of the form (_a.0 += (a.0 + 2)_) which cannot be reversed. We recommend also implementing overlap checks in any component using the generated IR representation of the SyReC program that could evaluate the value range of loop variables, e.g. in the logic synthesis process.

````

### Call-/UncallStatements

- The current implementation does not require that the module referenced by a _Call/UncallStatement_ must be defined at a position prior to the currently processed _Call/UncallStatement_ in the SyReC program.
- A _CallStatement_ will execute the referenced module starting from the first statement up to and including the last statement defined in its module body while an _UncallStatement_ will perform an execution in the reverse direction with both semantics being inherited from the predecessor language of SyReC (see Janus {cite:p}`yokoyama2007janus`).
- Recursive module calls are allowed but it is the responsibility of the developer of the SyReC program to prevent an infinite recursion. However, calls to the implicitly or explicitly defined main module of the SyReC program are not allowed.

  ````{note}

  Recursive calls to overloads of the implicitly defined main module are possible as long as the last module of the SyReC program is not called.

  ```text
  module add(in a(4), in b(4), out c(4))
    c += (a + b)

  // Implicitly defined main module
  module add(in a(8), in b(8), out c(8))
    wire tmp_1(4), tmp_2(4), wire tmp_3(4)

    tmp_1 ^= a.0:3;
    tmp_2 ^= b.0:3;
    call add(tmp_1, tmp_2, tmp_3); // Call OK -> module add(in a(4), ...) called
    c.0:4 ^= tmp_3;
    call add(a, b, c) // Call NOK -> implicit main module called
  ```

  ````

  ````{attention}

  Take care when defining conditional recursive calls since contrary to compiled/interpreted programming languages the recursive calls still needs to be synthesized despite the guard condition not evaluating to true. An example for an infinite recursive is shown in the following example:

  ```text
  module recIncrToTwo(inout a(2))
    if (a < 2) then
      ++= a;
      call recIncrToTwo(a)
    else
      skip
    fi (a < 2)

  module main(inout a(2))
    call recIncrToTwo(a)
  ```

  The guard condition might evaluate to false after two calls of the module _recIncrToTwo_ were inlined but the statements of the true-branch of the _IfStatement_ still need to be synthesized thus resulting in an infinite recursion.

  ````

- While the SyReC parser allows a variable to be used multiple times as a caller argument in a _Call/UncallStatement_, it is for now the responsibility of the user to prevent non-reversible assignments in the called module. An example of such an invalid access is shown in the following example:

  ```text
  module swap(inout left(4), inout right(4))
    left <=> right

  module main(inout a(4))
    // Call will result in access on assigned to variables parts
    // on both sides of SwapStatement (a <=> a)
    call swap(a, a)
  ```

### ForStatement

- While the SyReC grammar in the reference documents does not specify the keyword _do_ prior to the body of a ForStatement, the examples shown in said documents all use this keyword and thus, we assume that this is a typo in the grammar and the _do_ keyword is required.
- The initial value of a loop variable can be used in the initialization of the iteration ranges 'end' and 'stepsize' value as shown in the following example:

  ```text
  module main(...)
    for $i = 0 to ($i + 1) step ($i + 2) do
      ...
    rof

  // Is equivalent to
  module main(...)
    for $i = 0 to 1 step 2 do
      ...
    rof
  ```

- The identifier of a loop variable (excluding the dollar sign prefix) is allowed to be equal to the one of another variable as long as the latter is not a loop variable defined in a parent loop:

  ```text
  // Loop variable identifier equal to identifier of module parameter
  module main(inout a(4), in i(2))
    for $i = 0 to (#a - 1) do
      a.0:1 += (i + $i)
    rof

  module main(inout a(4))
    for $i = 0 to (#a - 1) do
      // Loop variable identifier matches the one of parent loop => Error
      for $i = 0 to (#a - 1) do
        ++= a
      rof
    rof
  ```

- Due to the requirement that the number of iterations performed by a ForStatement is known at compile time, assignments to loop variables are forbidden.
- If the step size of a ForStatement is not defined, it is assumed to equal 1.
- If the user does not specify a loop variable definition or start-end-value iteration range pair but only a single _Number_ component then it is assumed that this _Number_ defines the end value of the iteration range while the start value is assumed to be equal to 0.
  Note that this assumptions also holds if the user defines a negative stepsize. The following example showcases two equivalent loop definitions, one only specifying a single _Number_ component while the other defines the start-end-stepsize triple in the loop header:

  ```text
  module main(inout a(4))
    for (#a - 1) do
      --= a
    rof

  // Is equivalent to
  module main(inout a(4))
    for 0 to (#a - 1) step 1 do
      --= a
    rof
  ```

- Due to the assumption that all variable values can be represented by unsigned integer values, negative step size values are converted to their unsigned value using the C++17 value conversion semantics (see [chapter 7.8 of document N4659](https://open-std.org/JTC1/SC22/WG21/docs/standards)). The same conversion is applied to all negative values determined at compile time.
- Semantic/Syntax errors in the statements of the body of a loop performing no iterations are reported due to the parser not implementing the dead code elimination technique.
- The iteration range of SyReC loops can be determined as shown in the following example:

  ```text
  module main(inout a(32))
    // Loop will perform three iterations (loop variable *$i* will have values {math}`0, 2, 4`).
    for $i = 0 to 5 step 2 do
      ++= a
    rof

  // Equivalent C loop
  unsigned int a = ...;
  for (unsigned int i = 0; i < 5; i += 2) {
    ++= a
  }

  // Loop will perform three iterations (loop variable *$i* will have values {math}`5, 3, 1`).
  for $i = 5 to 0 step 2 do
    ++= a
  rof

  ```

- The value of the step size of a ForStatement cannot be defined as or evaluate to 0 to prevent an infinite loop.

### IfStatement

- The components of an IfStatement will be referred to as _if <GUARD_CONDITION> then <TRUE_BRANCH> else <FALSE_BRANCH> fi <CLOSING_GUARD_CONDITION_. To be able to identify the matching guard condition for a closing guard condition, the expressions used to define both of these components must consist of the same characters (with an expression evaluating to the same value while consisting of different or additional characters not being considered equal). An example of an IfStatement violating this rule is the following:

  ```text
  module main(inout a(4), in b(2))
    if ((a.0 + b.1) * 2) then
      skip
    else
      skip
    // Despite the simplified closing guard condition evaluating to the same
    // expression as the guard condition, the two expressions are not
    // considered equal due to the difference in the operands '2' and '#b'
    // between the two expressions
    fi ((a.0 + b.1) * #b)

  ```

- Semantic/Syntax errors in any simplified expression of either the guard or closing guard conditions are reported even if the violating expression can be omitted due to a simplification.
- Semantic/Syntax errors in the not executed branch of an IfStatement are reported due to the parser not implementing the dead code elimination optimization technique.
- The bitlength of the expression defined in the guard as well as closing guard condition must evaluate to 1.

### SwapStatement

- Both operands of the swap operation must have the same bitwidth.
- Whether the access on the assigned to variable parts in the dimension access of any _VariableAccess_ on the opposite side of the SwapStatement is allowed depends on the value of the corresponding flag in the parser configuration (see {doc}`flag <library/ConfigurableOptions>`).
- Assignments to the same variable parts between the two sides of the SwapStatement are not allowed and a semantic error is reported if the parser can detect such an overlap.
- Since the indices of the accessed dimension of a variable can be defined as a non-compile time constant expression and due to the inability to evaluate said expressions one is able to define a SwapStatement that operates on the same qubits in both operands as shown in the following example:

  ```text
  module main(inout a[2](4), in b(2))
    a[b] <=> a[b]
  ```

  Undefined or implementation specific behaviour will occur if such a _SwapStatement_ is synthesized, and it is the responsibility of the user to prevent such constructs.

## VariableAccess

- All indices defined in the dimension or bit/bitrange access of a variable access are zero-based.
- The dimension access can be omitted for variables with a single dimension containing only a single value (i.e., _module main(inout a(4)) ++= a_).
- It is the responsibility of the user to guarantee that the value of an expression not evaluable at compile time that is used to define the accessed index of a dimension in the dimension access component of the variable access to be within the valid value range for the accessed dimension. Otherwise, undefined or implementation specific behaviour can occur during synthesis.
- If a non-compile time constant expression is used to define the accessed value of a dimension then the bitwidth of the expression cannot be larger than the number of bits required store the index to the last element in the unrolled variable, e.g. the variable {math}`in \ a[2][3](2)` contains 6 elements with 3 bits being required to store the largest index (5) required to address any element in _a_.
- If the value of an index in either the dimension or bit/bitrange access evaluates to a constant at compile time, a validation of whether it is within the defined bounds of the accessed variable is performed and an error reported in case of an out-of-range value.
- Each expression defining the accessed value of the dimension will use an expected operand bitwidth for its operands that is only valid until the expression was processed. Any already existing expected operand bitwidth from outside the expression is ignored (i.e. set in the parent expression of the currently processed _VariableAccess_). Assuming that the expression of the first accessed dimension of the _VariableAccess_ on the right-hand side of the assignment in the following example is processed:

  ```text
  module main(inout a[2](4), in c[2][3](4), in b(2))
    a[0].1:2 += c[(b.0 + 2)][a[1]].0:1
  ```

  The expected operand bitwidth set by the _VariableAccess_ on the left-hand side of the assignment has a length of 2, which is satisfied by the _VariableAccess_ on the right-hand side.
  However, the expected operand bitwidth of the operands in the expression of the first dimension of the _VariableAccess_ on _c_ has a value of 1, while for the second dimension it is equal to 4.

- The SyReC parser does not require that the start index of a bit range access be larger or equal to the end index and thus supports the following index combinations:

  ```text
  module main(inout a(4))
    ++= a.0:2;
    --= a.2:0;
    ++= a.0:0
  ```

- The number of indices accessed in the dimension access component of a _VariableAccess_ must be equal to the number of dimensions of the referenced variable. An example of a valid and invalid DimensionAccesses is shown below:

  ```text
  module main(inout a[2][4](4), inout b(2))
    ++= a[0][1]; // OK
    --= b;       // OK
    ++= a[0]     // NOK: Number of accessed dimension does not match number of dimensions of variable 'a'
  ```

## Expressions

- Expressions with constant operands are evaluated at compile time.
- All SyReC operations except the upper-bit multiplication (`\*>`) and logical negation (`!`) operation are evaluated according to the value semantics of the corresponding C++ operation using unsigned integer operands.
- The logical negation of an integer constant {math}`C` at compile time is calculated by first converting {math}`C` to a boolean using the predicate {math}`p(C) = C > 0` and then negating the result of {math}`p(C)`.
- Arithmetic and logical simplifications are applied at compile time by default (i.e., will result in a simplification of the expression ({math}`(a + b) * 0` to {math}`0`). However, semantic/syntax errors in the operands of even the simplified subexpressions are reported with the following code sample showcasing an example:

  ```text
  module main(inout a[2](4))
    a[0] += ((a[2] + 2) * (#a - 4))
  ```

  While the right-hand side expression of the assignment is simplified to the integer constant _0_, the semantic error causes by the out-of-range index access in the _VariableAccess_ _a[2]_ is still reported.
  - All operands of an expression must have the same bitwidth (excluding constant integers which are truncated to the expected bitwidth using the {doc}`configured truncation operation <library/ConfigurableOptions>`), with the parser using the first bitrange with known bounds as the reference bitwidth (if such an access exists in the operands). Any bit access will set the expected operand bitwidth of a _VariableAccess_ to 1.

    ```{note}
    Logical and relational operations will 'truncate' the expected bitwidth of its result to 1.
    ```

    - The following table will showcase the expected operand bitwidth for each operand as well as subexpression of the right-hand side expression of the defined assignment:

      ```text
      module main(inout a(6), in b(3))
        a.0 += (((a.1:3 + 2) > b) || (a.1 != 1))
      ```

      Note that while the expected operand bitwidth of the top-most expression is equal to 1, the operands in one of its subexpressions _((a.1:3 + 2) > b)_ are allowed to have a different bitwidth due to the relational operation _>_ causing the result to be aggregated into a 1-bit result thus satisfying the expected operand bitwidth for the logical OR operation of the parent expression.

      ```{eval-rst}
      +-----------------------------------+--------------------------------+
      | **Expression**                    | **Expected operand bitwidth**  |
      +===================================+================================+
      | a.1:3                             |                              3 |
      +-----------------------------------+--------------------------------+
      | 2                                 |                             32 |
      +-----------------------------------+--------------------------------+
      | (a.1:3 + 2)                       | 3                              |
      +-----------------------------------+--------------------------------+
      | b                                 | 3                              |
      +-----------------------------------+--------------------------------+
      | ((a.1:3 + 2) > b)                 | 1                              |
      +-----------------------------------+--------------------------------+
      | a.1                               | 1                              |
      +-----------------------------------+--------------------------------+
      | 1                                 | 32                             |
      +-----------------------------------+--------------------------------+
      | (a.1 != 1)                        | 1                              |
      +-----------------------------------+--------------------------------+
      | (((a.1:3 + 2) > b) || (a.1 != 1)) | 1                              |
      +-----------------------------------+--------------------------------+
      ```

- All integer constant values are truncated to the expected operand bitwidth, if the latter exists for the expression; otherwise, the values are left unchanged. However, integer constant values defined in the shift amount component of a _ShiftExpression_ are not truncated since they modify the left-hand side of the _ShiftExpression_ and "build" the result instead of being an operand of the overall expression.

  The following code example will showcase a few examples and assumes that constant integer values are truncated using the modulo operation

  ```text
  module main(inout a[2](4), in b(2), in c(4))
    // Expected operand bitwidth set by a[0].0:1 to 2
    a[0].0:1 += (b + 4);
    for $i = 0 to (#a - 1) do
      // Expected operand bitwidth set by a[(b + 2) + 5].$i to 1
      a[(b + 3) + 5].$i += (c.$i + b.0) + 3;
      // Expected operand bitwidth set by b.2:0 to 3
      a[1].0:($i + 2) += (b.2:0 + 5);
      // Expected operand bitwidth set by a[0].1:2 to 2
      a[0].1:2 += (((b << 4) + 2) << 1);
      // Expected operand bitwidth for expression E1 (a > 4) is equal to 4.
      // Expected operand bitwidth for expression E2 (b != c.0:1) is equal to 2.
      // Expected operand bitwidth of expression E3 = (E1 || E2) is equal to 1.
      a[0].0 += ((a[1] > 5) || (b != c.0:1));
      // Expected operand bitwidth for expression E1 (a[1].1:2 + 5) is equal to 2.
      // Expected operand bitwidth for expression E2 (a[1] != 18) is equal to 4.
      // Expected operand bitwidth of expression E3 = (E1 || E2) is equal to 1.
      a[0].1 += ((a[1].1:2 + 5) || (a[1] != 18)) // This statement is semantically incorrect
    rof
  ```

  The SyReC program above is transformed to

  ```text
  module main(inout a[2](4), in b(2), in c(4))
    // 4 MOD 3 = 1
    a[0].0:1 += (b + 1);
    for $i = 0 to (#a - 1) do
      // 3 MOD 1 = 0 causes simplification of right-hand side expression
      // Note that the expression ((b + 2) + 5) uses a separate expected
      // operand bitwidth of 2 and is simplified to (b + 2).
      // Constant operand (3) of right-hand side of assignment is simplified to zero:
      //  3 MOD 3 = 0 and 5 MOD 3 = 2
      a[(b + 2)].$i += (c.$i + b.0);
      // 5 MOD 7 => 1
      a[1].0:($i + 2) += (b.2:0 + 5)
      // Expected operand bitwidth of 2 causes simplification of (b << 4) to 0
      // since shift amount is larger than expected bitwidth
      // Remaining expression 2 << 1 evaluated to 4 => 4 MOD 3 = 1
      a[0].1:2 += 1;
      // Expected operand bitwidth of 4 for original expression E1 (a[1] > 5) does not modify original operand 5 MOD 15 = 5.
      a[0].0 += ((a[1] > 5) || (b != c.0:1));
      // Expected operand bitwidth for original expression E1 (a[1].1:2 + 5) causes truncation of 5 MOD 3 = 2.
      // Expected operand bitwidth for original expression E2 (a[1] != 18) causes truncation of 18 MOD 15 = 3.
      // Expected operand bitwidth of expression E3 = (E1 || E2) is equal to 1.
      // The subexpressions E1 does not satisfy the expected operand bitwidth of the logical OR operation
      // and thus the right-hand side expression of the assignment is not semantically correct
      a[0].1 += ((a[1].1:2 + 2) || (a[1] != 3))
    rof
  ```

  Setting the expected operand bitwidth for one of the operands of an expression that was previously not known will cause an execution of the integer constant truncation in said operand:

  ```text
  // Original SyReC program
  module main(inout a[2](4), in b(2))
    for $i = 0 to (#a - 2) do
      // Expected operand bitwidth of expression (a[1].$i:($i + 1) + 2) is unknown
      // Expected operand bitwidth of expression (b + 2) is equal to 2
      a[0]:1 += (((a[1].$i:($i + 1) + 4) + (b + 5)) || a[0].2)
    rof

  // Original SyReC program (after integer constant truncation)
  module main(inout a[2](4), in b(2))
    for $i = 0 to (#a - 2) do
      a[0]:1 += (((a[1].$i:($i + 1) + 1) + (b + 2)) || a[0].2)
    rof
  ```

  However, the intermediate results of an expression containing only integer constants at compile-time are not truncated during the evaluation of said expression. The evaluation steps of the right-hand side
  expression of the assignment in the following example will serve as an example assuming that the integer constant truncation is using the bitwise AND operation:

  ```text
  module main(inout a(2), in b(2), in c(1))
   // Is equal to: a += (((2 + 2) * 3) - 1)
   // And simplified to: a += 3
   a += (((#b + 2) * 3) - #c)
  ```

  ```{eval-rst}
  +----------------+------------+-----------------------------+
  | **Expression** | **Result** | **Result operand bitwidth** |
  +================+============+=============================+
  | (#b + 2)       |          4 |                          32 |
  +----------------+------------+-----------------------------+
  | (4 * 3)        |         12 |                          32 |
  +----------------+------------+-----------------------------+
  | (12 - #c)      |         11 |                          32 |
  +----------------+------------+-----------------------------+
  | 3              |          3 |                           2 |
  +----------------+------------+-----------------------------+
  ```

- The following enumeration defines how the right-hand side operand of the supported integer constant truncation operations is calculated for a expected operand bitwidth {math}`b`:
  - Bitwise AND: {math}`2^{b} - 1`
  - Modulo: {math}`2^{b} - 1`

- Expressions with constant integer operands are evaluated using the C++ semantics for unsigned integers.
- Operands of an expression using the relational (`<`, `>`, `<=`, `>=`, `=`, `!=`), logical (`||`, `&&`) or the unary operation (`!`) must to have a bitwidth equal to 1.

## Numbers

- In addition to integer literals also hexadecimal and binary literals can be defined (a custom extension of the reference SyReC grammar).
- Hexadecimal literals are defined using the prefix ('0x' or '0X') followed by at least one digit or one of the letters (A,B,C,D,E,F) with the latter not being case-sensitive. The first number after the prefix '0x' is the most significant one (e.g. 0x1A is equal to the integer 26).
- Binary literals are defined using the prefix ('0b' or '0B') followed by at least one binary value (0, 1) with the most significant bit being defined immediately after the prefix '0b' (e.g. 0b00011010 is equal to the integer 26).
- Hexadecimal and binary literals cannot be used to define the number of values in a dimension or the bitwidth of a SyReC module parameter or local variable.
