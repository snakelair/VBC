import os
import shutil

bench_dir = os.path.join(os.path.dirname(__file__), "compiler_comparison")
os.makedirs(bench_dir, exist_ok=True)

# 1. HelloWorld (10 LOC)
with open(os.path.join(bench_dir, "01_HelloWorld.vbc"), "w", encoding="utf-8") as f:
    f.write("""Imports System

Module Program
    Sub Main()
        Console.WriteLine("Hello, VBC World!")
    End Sub
End Module
""")

# 2. Algorithms (60 LOC)
with open(os.path.join(bench_dir, "02_Algorithms.vbc"), "w", encoding="utf-8") as f:
    f.write("""Imports System

Module Algorithms
    Function Fib(n As Integer) As Integer
        If n <= 1 Then Return n
        Return Fib(n - 1) + Fib(n - 2)
    End Function

    Function Gcd(a As Integer, b As Integer) As Integer
        While b <> 0
            Dim t = b
            b = a Mod b
            a = t
        End While
        Return a
    End Function

    Function IsPrime(n As Integer) As Boolean
        If n < 2 Then Return False
        Dim i = 2
        While i * i <= n
            If n Mod i = 0 Then Return False
            i = i + 1
        End While
        Return True
    End Function

    Function CountPrimes(limit As Integer) As Integer
        Dim count = 0
        For i = 2 To limit
            If IsPrime(i) Then count = count + 1
        Next
        Return count
    End Function

    Sub Main()
        Dim f = Fib(15)
        Dim g = Gcd(105, 252)
        Dim p = CountPrimes(1000)
        Console.WriteLine(f)
        Console.WriteLine(g)
        Console.WriteLine(p)
    End Sub
End Module
""")

# 3. LinqDemo (60 LOC)
shutil.copyfile("samples/05_LinqDemo/Program.vbc", os.path.join(bench_dir, "03_LinqDemo.vbc"))

# 4. Entire Compiler (VbcAll.vbc ~1500 LOC)
shutil.copyfile("VbcAll.vbc", os.path.join(bench_dir, "04_VbcAll.vbc"))

# 5. StressLarge (5000 LOC)
with open(os.path.join(bench_dir, "05_StressLarge.vbc"), "w", encoding="utf-8") as f:
    f.write("Imports System\n\nModule StressModule\n")
    for i in range(250):
        f.write(f"""    Function Compute_{i}(a As Integer, b As Integer, c As Integer) As Integer
        Dim r1 = a * {i % 17 + 1} + b * {i % 13 + 1} - c
        Dim r2 = (r1 * 3) / 2 + (a Mod 7)
        If r2 > 100 Then
            r2 = r2 - 50
        Else
            r2 = r2 + 25
        End If
        Dim sum = 0
        For j = 0 To {i % 10 + 2}
            sum = sum + j * r2
        Next
        Return sum + r1
    End Function
""")
    f.write("\n    Sub Main()\n        Dim total As Long = 0\n")
    for i in range(250):
        f.write(f"        total = total + Compute_{i}({i}, {i*2}, {i+5})\n")
    f.write("""        Console.WriteLine(total)
    End Sub
End Module
""")

print("Successfully generated all 5 compiler benchmark test cases.")
