# VBC Developer & AI Agent Guide (Grounding Reference)

> **Target Audience**: AI Coding Assistants (Antigravity, Cursor, Windsurf, Claude Code, GitHub Copilot) and Human Compiler Contributors.  
> **Purpose**: Eliminates LLM hallucinations, defines the exact VBC language specification, Goose memory model, self-hosting compilation pipeline, and verification workflow.

---

## 1. What is VBC? (Core Architecture)

**VBC** is a 100% self-hosted, pure native systems programming language that marries the clean syntax of **Visual Basic** with the zero-overhead memory model of **Goose**.

### Key Differentiators vs Standard VB.NET:
- **Zero GC & Zero .NET Dependency**: Does NOT use the CLR, .NET runtime, or garbage collector.
- **Goose Linear Data Stack**: Bump-allocators allocate variables, strings, and temporary objects linearly. Exiting a function or loop resets the stack pointer ($O(1)$ reclamation in 1 CPU instruction).
- **Native C99 Target**: The compiler parses VBC source and emits standards-compliant C99/C11 code, compiled directly via Clang or GCC into an optimized native executable (`-O2` / `-O3`).
- **Deterministic Polymorphism**: OOP classes use dynamic VTables generated in pure C99 without virtual machine overhead.

---

## 2. Compiler Pipeline & Source Code Map (`src/`)

The compiler is written in VBC itself and resides in [`src/`](file:///c:/Projects%20My/VBC/src):

| File | Lines | Responsibility |
|---|---|---|
| [`src/Token.vbc`](file:///c:/Projects%20My/VBC/src/Token.vbc) | ~300 | Token definitions (`TOK_*`), keyword table, `GetTokenKindName()` |
| [`src/Lexer.vbc`](file:///c:/Projects%20My/VBC/src/Lexer.vbc) | ~300 | Scanning characters, string escapes, numbers, hex/binary, symbols |
| [`src/Ast.vbc`](file:///c:/Projects%20My/VBC/src/Ast.vbc) | ~200 | Arena allocator, node definitions (`NODE_*`), `GetNodeKindName()` |
| [`src/Parser.vbc`](file:///c:/Projects%20My/VBC/src/Parser.vbc) | ~1,200 | Recursive-descent parser producing Arena AST nodes |
| [`src/EmitterCore.vbc`](file:///c:/Projects%20My/VBC/src/EmitterCore.vbc) | ~670 | Type resolution, symbols, temps (`NextTemp`), C type mapping |
| [`src/EmitterTypes.vbc`](file:///c:/Projects%20My/VBC/src/EmitterTypes.vbc) | ~270 | C99 `struct`, `class`, VTable, and `interface` code generation |
| [`src/EmitterStmt.vbc`](file:///c:/Projects%20My/VBC/src/EmitterStmt.vbc) | ~470 | Statements: assignments, conditionals, loops, `Using`, `Try/Catch` |
| [`src/EmitterExpr.vbc`](file:///c:/Projects%20My/VBC/src/EmitterExpr.vbc) | ~1,100 | Expressions: literals, binary/unary ops, method invocations, arrays |
| [`src/Emitter.vbc`](file:///c:/Projects%20My/VBC/src/Emitter.vbc) | ~310 | `EmitCompilationUnitToC()` orchestrator and header emission |
| [`src/Driver.vbc`](file:///c:/Projects%20My/VBC/src/Driver.vbc) | ~15 | Invokes Clang/GCC on generated C files and runs binaries |
| [`src/Lsp.vbc`](file:///c:/Projects%20My/VBC/src/Lsp.vbc) | ~115 | Language Server Protocol (LSP 3.17) standard handler |
| [`src/Program.vbc`](file:///c:/Projects%20My/VBC/src/Program.vbc) | ~260 | CLI interface, AI introspection tools, and native test runner |

---

## 3. The Goose Memory Model (Rules for Writing VBC)

### 3.1. The Linear Data Stack (`vbc_stack_t`)
- Every execution context receives a pointer to a linear memory arena `vbc_stack_t* _stack`.
- Memory is allocated by advancing a bump pointer:
  $$\text{aligned\_ptr} = (\text{ptr} + (\text{align} - 1)) \ \& \ \sim(\text{align} - 1)$$
- Returning from a function resets `stack->ptr` back to the caller's frame mark ($O(1)$).
- **Return Values**: When a function returns a string or composite type, the return value is constructed directly in the caller's stack frame or passed back safely before frame reset.

### 3.2. Types: Value vs Reference
- **`Structure`**: Value type. Allocated directly on the current stack frame. Copied by value.
- **`Class`**: Reference type. Allocated on the bump-stack via `New`. Passed via pointer `Me`. Dynamic polymorphism via VTable.
- **`Using ... End Using` (RAII)**: Ensures deterministic disposal of resources (e.g. database transactions, sockets, file handles).

### 3.3. Strings (`vbc_string_t`)
- Strings are immutable byte slices: `{ const char* data; int64_t len; }`.
- String literals are zero-allocation (`vbc_str_lit`).
- Slices do not allocate new memory; they reference existing memory ranges.
- Concatenation (`a & b`) allocates in the current stack arena.

---

## 4. Supported Language Syntax & Types

### Primitive Types
- `Integer`: 32-bit signed integer (`int32_t`)
- `Long`: 64-bit signed integer (`int64_t`)
- `Double`: 64-bit IEEE float (`double`)
- `Boolean`: `True` / `False` (`bool`)
- `Byte`: 8-bit unsigned integer (`uint8_t`)
- `String`: Contiguous Goose string slice (`vbc_string_t`)
- `T?` / `Nullable(Of T)`: Nullable value type with `.HasValue` and `.Value`

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `Mod`
- Comparison: `=`, `<>`, `<`, `<=`, `>`, `>=`
- Logical / Bitwise: `And`, `Or`, `Xor`, `Not`, `AndAlso`, `OrElse`, `<<` (`Shl`), `>>` (`Shr`)
- Hex literals: `&HFF`, `&h0E1`, `0x1A2B`
- Null-coalescing: `If(nullableVal, defaultVal)`
- Null-conditional access: `obj?.Member`, `arr?(index)`

### Control Flow
- `If condition Then ... ElseIf ... Else ... End If`
- `For i = start To end [Step s] ... Next`
- `For Each item In collection ... Next`
- `While condition ... End While`
- `Using res = New Resource() ... End Using`
- `Try ... Catch ex As Exception ... Finally ... End Try`
- `SyncLock lockObj ... End SyncLock`

### Object-Oriented Features
```vb
Class Animal
    Public Name As String
    Public Sub New(name As String)
        Me.Name = name
    End Sub
    Public Overridable Function Speak() As String
        Return "..."
    End Function
End Class

Class Dog Inherits Animal
    Public Breed As String
    Public Sub New(name As String, breed As String)
        MyBase.New(name)
        Me.Breed = breed
    End Sub
    Public Overrides Function Speak() As String
        Return "Woof! I am " & Breed
    End Function
End Class
```

---

## 5. Golden Rules for AI Agents (Do's & Don'ts)

### ❌ NEVER DO:
1. **Do NOT import or use standard .NET libraries**:
   - `System.Collections.Generic.List(Of T)` does NOT exist. Use native arrays `Dim arr() As Integer = [1, 2, 3]`.
   - Do NOT use `Console.ReadLine()`.
   - Do NOT use .NET Reflection, Attributes, LINQ lambda chains (`.Where(x => ...)`).
2. **Do NOT edit `bootstrap/vbc_bootstrap.c` directly**:
   - `bootstrap/vbc_bootstrap.c` is generated automatically. Always edit `src/*.vbc` and run `build.bat rebuild`.
3. **Do NOT allocate with standard `malloc()` without stack awareness**:
   - All runtime objects belong to the Goose stack or dedicated subsystem handles.

### ✅ ALWAYS DO:
1. **Always verify changes using AI introspection and native tests**:
   - Fast dry-run check: `.\vbc.exe --check <file.vbc>`
   - Inspect AST changes: `.\vbc.exe --dump-ast <file.vbc>`
   - Inspect tokens: `.\vbc.exe --dump-tokens <file.vbc>`
   - Full regression suite: `.\vbc.exe test samples`
2. **Always perform Self-Hosted Rebuild when modifying `src/`**:
   - Run `build.bat rebuild` to compile `src/` into `vbc.exe` and synchronize `bootstrap/vbc_bootstrap.c`.

---

## 6. Developer CLI Reference

```cmd
# Check syntax and AST (instant dry-run, no Clang invocation)
vbc --check path/to/file.vbc

# Dump lexical tokens (for debugging parser/lexer issues)
vbc --dump-tokens path/to/file.vbc

# Dump AST node hierarchy (for debugging AST construction)
vbc --dump-ast path/to/file.vbc

# Compile source to native executable
vbc compile path/to/file.vbc -o app.exe [--emit-c]

# Compile and run immediately
vbc run path/to/file.vbc

# Run all 12 native regression test suites
vbc test samples

# Self-hosted rebuild of compiler from src/
build.bat rebuild

# Clean all temporary files and PDBs
build.bat clean
```
