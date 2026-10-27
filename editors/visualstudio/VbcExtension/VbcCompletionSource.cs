using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text.RegularExpressions;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Operations;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(ICompletionSourceProvider))]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    [Name("VbcCompletionSourceProvider")]
    public class VbcCompletionSourceProvider : ICompletionSourceProvider
    {
        [Import]
        internal ITextStructureNavigatorSelectorService NavigatorService = null!;

        [Import]
        internal IGlyphService GlyphService = null!;

        public ICompletionSource? TryCreateCompletionSource(ITextBuffer textBuffer)
        {
            if (!VbcContentDefinition.IsVbcBuffer(textBuffer))
                return null;

            return new VbcCompletionSource(textBuffer, NavigatorService, GlyphService);
        }
    }

    public class VbcCompletionSource : ICompletionSource
    {
        private readonly ITextBuffer _textBuffer;
        private readonly ITextStructureNavigatorSelectorService _navigatorService;
        private readonly IGlyphService _glyphService;
        private bool _isDisposed;

        private static readonly (string Text, string Desc, StandardGlyphGroup Group)[] StaticCompletions = new[]
        {
            // Keywords
            ("Dim", "Declares and allocates storage space for one or more variables [C: local variable / struct field].", StandardGlyphGroup.GlyphKeyword),
            ("DirectCast", "Introduces an explicit type conversion operation [C: ((TargetType)expr)].", StandardGlyphGroup.GlyphKeyword),
            ("Do", "Starts a Do...Loop block to execute code repeatedly [C: do { ... } while (...);].", StandardGlyphGroup.GlyphKeyword),
            ("Sub", "Declares a Sub procedure [C: void func(vbc_stack_t* _stack, ...)].", StandardGlyphGroup.GlyphKeyword),
            ("Function", "Declares a Function procedure [C: ReturnType func(vbc_stack_t* _stack, ...)].", StandardGlyphGroup.GlyphKeyword),
            ("Module", "Declares a static module container [C: Translation Unit / static functions].", StandardGlyphGroup.GlyphKeyword),
            ("Structure", "Declares a value type structure [C: typedef struct Name { ... } Name;].", StandardGlyphGroup.GlyphKeyword),
            ("Class", "Declares a reference type class [C: typedef struct Name { const Name_VTable* _vtable; ... } Name;].", StandardGlyphGroup.GlyphKeyword),
            ("Interface", "Declares an interface contract [C: Fat pointer struct { void* obj; const VTable* _vtable; }].", StandardGlyphGroup.GlyphKeyword),
            ("Implements", "Implements an interface contract [C: Static interface VTable mapping].", StandardGlyphGroup.GlyphKeyword),
            ("Enum", "Declares an enumeration [C: enum Name { ... }].", StandardGlyphGroup.GlyphKeyword),
            ("Property", "Declares a property with getter/setter [C: getter/setter functions].", StandardGlyphGroup.GlyphKeyword),
            ("End", "Terminates a block statement (End Sub, End Function, End Module, End If, ...).", StandardGlyphGroup.GlyphKeyword),
            ("If", "Conditional execution or binary null-coalescing [C: if (...) / ({ ... })].", StandardGlyphGroup.GlyphKeyword),
            ("Then", "Introduces a statement block executed if tested condition is true.", StandardGlyphGroup.GlyphKeyword),
            ("Else", "Introduces a fallback block if no previous condition is true.", StandardGlyphGroup.GlyphKeyword),
            ("ElseIf", "Introduces an alternative condition evaluation.", StandardGlyphGroup.GlyphKeyword),
            ("For", "Executes a counted loop [C: for (int64_t var = start; var <= end; var += step)].", StandardGlyphGroup.GlyphKeyword),
            ("To", "Specifies the terminal value for a loop counter variable.", StandardGlyphGroup.GlyphKeyword),
            ("Step", "Specifies the loop counter step increment.", StandardGlyphGroup.GlyphKeyword),
            ("Next", "Terminates a For...Next loop.", StandardGlyphGroup.GlyphKeyword),
            ("While", "Executes loop while condition remains True [C: while (...) { ... }].", StandardGlyphGroup.GlyphKeyword),
            ("Loop", "Terminates a Do...Loop block.", StandardGlyphGroup.GlyphKeyword),
            ("Until", "Executes loop until condition becomes True [C: while (!...)].", StandardGlyphGroup.GlyphKeyword),
            ("Select", "Executes multi-way branch depending on value [C: switch (...)].", StandardGlyphGroup.GlyphKeyword),
            ("Case", "Introduces a value branch in Select Case [C: case ...:].", StandardGlyphGroup.GlyphKeyword),
            ("Return", "Returns control and optional value to caller [C: return value;].", StandardGlyphGroup.GlyphKeyword),
            ("Imports", "Imports namespace declarations from standard library or modules.", StandardGlyphGroup.GlyphKeyword),
            ("Const", "Declares a compile-time constant [C: const or #define].", StandardGlyphGroup.GlyphKeyword),
            ("As", "Introduces type annotation for variables, parameters, or functions.", StandardGlyphGroup.GlyphKeyword),
            ("New", "Allocates and initializes an instance [C: heap/stack allocation + constructor].", StandardGlyphGroup.GlyphKeyword),
            ("And", "Bitwise or logical AND [C: & or &&].", StandardGlyphGroup.GlyphKeyword),
            ("Or", "Bitwise or logical OR [C: | or ||].", StandardGlyphGroup.GlyphKeyword),
            ("Not", "Bitwise or logical NOT [C: ~ or !].", StandardGlyphGroup.GlyphKeyword),
            ("Xor", "Bitwise exclusive OR [C: ^].", StandardGlyphGroup.GlyphKeyword),
            ("True", "Boolean true literal [C: true (1)].", StandardGlyphGroup.GlyphKeyword),
            ("False", "Boolean false literal [C: false (0)].", StandardGlyphGroup.GlyphKeyword),
            ("Nothing", "Default or null representation [C: NULL or zero-initialized struct].", StandardGlyphGroup.GlyphKeyword),
            ("ByVal", "Passes parameter by value [C: T arg].", StandardGlyphGroup.GlyphKeyword),
            ("ByRef", "Passes parameter by reference pointer [C: T* arg].", StandardGlyphGroup.GlyphKeyword),
            ("Public", "Public visibility [C: exported function / struct member].", StandardGlyphGroup.GlyphKeyword),
            ("Private", "Private visibility [C: static function / private member].", StandardGlyphGroup.GlyphKeyword),
            ("Friend", "Internal visibility [C: internal translation unit].", StandardGlyphGroup.GlyphKeyword),
            ("Protected", "Derived-class visibility.", StandardGlyphGroup.GlyphKeyword),
            ("Shared", "Static member associated with class/structure [C: static function/var].", StandardGlyphGroup.GlyphKeyword),
            ("Static", "Local variable preserved across invocations [C: static T var;].", StandardGlyphGroup.GlyphKeyword),
            ("ReadOnly", "Read-only field or property [C: const member].", StandardGlyphGroup.GlyphKeyword),
            ("Exit", "Exits Sub, Function, While, Do, or For [C: break / return].", StandardGlyphGroup.GlyphKeyword),
            ("Continue", "Continues to next iteration of a loop [C: continue;].", StandardGlyphGroup.GlyphKeyword),
            ("With", "References target object repeatedly.", StandardGlyphGroup.GlyphKeyword),
            ("Each", "Introduces loop iteration over collection elements.", StandardGlyphGroup.GlyphKeyword),
            ("In", "Iterates in collection.", StandardGlyphGroup.GlyphKeyword),
            ("Try", "Begins structured exception handling [C: setjmp / exception frame].", StandardGlyphGroup.GlyphKeyword),
            ("Catch", "Catches exceptions matching condition [C: exception handler block].", StandardGlyphGroup.GlyphKeyword),
            ("Finally", "Ensures cleanup block executes [C: cleanup handler].", StandardGlyphGroup.GlyphKeyword),
            ("Throw", "Throws an exception [C: longjmp / vbc_throw].", StandardGlyphGroup.GlyphKeyword),
            ("Using", "Scoped resource disposal block [C: automatic cleanup call].", StandardGlyphGroup.GlyphKeyword),
            ("TypeOf", "Checks runtime type against specified type [C: type descriptor check].", StandardGlyphGroup.GlyphKeyword),
            ("Is", "Object reference equality check [C: ptr1 == ptr2].", StandardGlyphGroup.GlyphKeyword),
            ("IsNot", "Object reference inequality check [C: ptr1 != ptr2].", StandardGlyphGroup.GlyphKeyword),
            ("AddressOf", "Creates a procedure function pointer [C: &FunctionName].", StandardGlyphGroup.GlyphKeyword),
            ("CType", "Explicit type cast [C: (TargetType)expr].", StandardGlyphGroup.GlyphKeyword),
            ("TryCast", "Safe reference type conversion [C: dynamic cast / NULL on failure].", StandardGlyphGroup.GlyphKeyword),

            // Types
            ("Integer", "Represents a 32-bit signed integer [C: int32_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Long", "Represents a 64-bit signed integer [C: int64_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Short", "Represents a 16-bit signed integer [C: int16_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Byte", "Represents an 8-bit unsigned integer [C: uint8_t].", StandardGlyphGroup.GlyphGroupClass),
            ("SByte", "Represents an 8-bit signed integer [C: int8_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Single", "Represents a single-precision 32-bit floating point number [C: float].", StandardGlyphGroup.GlyphGroupClass),
            ("Double", "Represents a double-precision 64-bit floating point number [C: double].", StandardGlyphGroup.GlyphGroupClass),
            ("Decimal", "Represents a high-precision decimal floating point number [C: double].", StandardGlyphGroup.GlyphGroupClass),
            ("String", "Represents zero-GC linear stack string slice [C: vbc_string_t (const char* ptr, int64_t len)].", StandardGlyphGroup.GlyphGroupClass),
            ("Boolean", "Represents a Boolean value [C: bool (int32_t)].", StandardGlyphGroup.GlyphGroupClass),
            ("Char", "Represents a single character [C: char (int8_t)].", StandardGlyphGroup.GlyphGroupClass),
            ("Object", "Base object pointer type [C: void*].", StandardGlyphGroup.GlyphGroupClass),
            ("Void", "Represents the absence of a return type [C: void].", StandardGlyphGroup.GlyphGroupClass),
            ("IntPtr", "Represents a native pointer-sized signed integer [C: intptr_t].", StandardGlyphGroup.GlyphGroupClass),
            ("UIntPtr", "Represents a native pointer-sized unsigned integer [C: uintptr_t].", StandardGlyphGroup.GlyphGroupClass),
            ("UInteger", "Represents a 32-bit unsigned integer [C: uint32_t].", StandardGlyphGroup.GlyphGroupClass),
            ("ULong", "Represents a 64-bit unsigned integer [C: uint64_t].", StandardGlyphGroup.GlyphGroupClass),
            ("UShort", "Represents a 16-bit unsigned integer [C: uint16_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Nullable", "Nullable value type wrapper [C: struct { bool has_value; T value; }].", StandardGlyphGroup.GlyphGroupClass),

            // XML & LINQ to XML
            ("XDocument", "Represents an XML document [C: vbc_xml_doc_t*].", StandardGlyphGroup.GlyphGroupClass),
            ("XElement", "Represents an XML element [C: vbc_xml_elem_t*] supporting XML Axis syntax.", StandardGlyphGroup.GlyphGroupClass),
            ("XAttribute", "Represents an XML attribute key-value pair [C: vbc_xml_attr_t*].", StandardGlyphGroup.GlyphGroupClass),

            // Base Class Library (BCL)
            ("StringBuilder", "Mutable string buffer on Goose linear stack [C: vbc_string_builder_t].", StandardGlyphGroup.GlyphGroupClass),
            ("Stopwatch", "Monotonic high-precision hardware timer [C: vbc_stopwatch_t].", StandardGlyphGroup.GlyphGroupClass),
            ("DateTime", "Instant in time measured in 100ns ticks [C: vbc_datetime_t (int64_t ticks)].", StandardGlyphGroup.GlyphGroupClass),
            ("TimeSpan", "Time duration interval measured in 100ns ticks [C: vbc_timespan_t (int64_t ticks)].", StandardGlyphGroup.GlyphGroupClass),
            ("Guid", "128-bit RFC 4122 UUIDv4 identifier [C: vbc_guid_t (uint8_t bytes[16])].", StandardGlyphGroup.GlyphGroupClass),
            ("Math", "Standard C99 math functions [C: <math.h>].", StandardGlyphGroup.GlyphGroupModule),
            ("Path", "Cross-platform path manipulation functions [C: vbc_bcl.h path functions].", StandardGlyphGroup.GlyphGroupModule),
            ("Directory", "Native filesystem directory operations [C: vbc_bcl.h directory functions].", StandardGlyphGroup.GlyphGroupModule),
            ("File", "Native OS file input/output operations [C: vbc_bcl.h file functions].", StandardGlyphGroup.GlyphGroupModule),
            ("Environment", "Operating system environment properties [C: vbc_bcl.h OS environment].", StandardGlyphGroup.GlyphGroupModule),

            // VBC Native Runtime Library
            ("Console", "Standard console input/output methods [C: stdout / stderr].", StandardGlyphGroup.GlyphGroupModule),
            ("WriteLine", "Writes specified data with newline to stdout [C: vbc_print_*(...)].", StandardGlyphGroup.GlyphGroupMethod),
            ("Write", "Writes specified data to stdout [C: vbc_print_*(...)].", StandardGlyphGroup.GlyphGroupMethod),
            ("ReadLine", "Reads next line of characters from stdin [C: vbc_console_readline].", StandardGlyphGroup.GlyphGroupMethod),
            ("ReadAllText", "Reads entire file into a string [C: vbc_file_read_all_text].", StandardGlyphGroup.GlyphGroupMethod),
            ("WriteAllText", "Writes string to file [C: vbc_file_write_all_text].", StandardGlyphGroup.GlyphGroupMethod),
            ("AppendAllText", "Appends string to file [C: vbc_file_append_all_text].", StandardGlyphGroup.GlyphGroupMethod),
            ("Exists", "Checks if file or directory exists [C: vbc_file_exists / vbc_directory_exists].", StandardGlyphGroup.GlyphGroupMethod),
            ("IntToStr", "Converts integer to string slice [C: vbc_int_to_str].", StandardGlyphGroup.GlyphGroupMethod),
            ("StringLength", "Returns length of string slice [C: str.len].", StandardGlyphGroup.GlyphGroupMethod),
            ("StringSub", "Extracts substring from string slice [C: vbc_str_sub].", StandardGlyphGroup.GlyphGroupMethod),
            ("Socket", "Native socket networking system [C: vbc_socket_t*].", StandardGlyphGroup.GlyphGroupClass),
            ("HttpClient", "Zero-alloc non-blocking HTTP client [C: vbc_http_client_t*].", StandardGlyphGroup.GlyphGroupClass),
            ("HttpServer", "Native ultra-low-latency HTTP server engine [C: vbc_http_server_t*].", StandardGlyphGroup.GlyphGroupClass),
            ("HttpRequest", "Encapsulates incoming HTTP request [C: vbc_http_req_t*].", StandardGlyphGroup.GlyphGroupClass),
            ("HttpResponse", "Encapsulates outgoing HTTP response [C: vbc_http_resp_t*].", StandardGlyphGroup.GlyphGroupClass)
        };

        public VbcCompletionSource(ITextBuffer textBuffer, ITextStructureNavigatorSelectorService navigatorService, IGlyphService glyphService)
        {
            _textBuffer = textBuffer;
            _navigatorService = navigatorService;
            _glyphService = glyphService;
        }

        public void AugmentCompletionSession(ICompletionSession session, IList<CompletionSet> completionSets)
        {
            if (_isDisposed) return;
            var triggerPoint = session.GetTriggerPoint(_textBuffer.CurrentSnapshot);
            if (triggerPoint == null) return;

            var snapshot = _textBuffer.CurrentSnapshot;
            var point = triggerPoint.Value;

            // Find current word before cursor
            var line = point.GetContainingLine();
            string lineText = line.GetText();
            int col = point.Position - line.Start.Position;

            int start = col;
            while (start > 0 && (char.IsLetterOrDigit(lineText[start - 1]) || lineText[start - 1] == '_'))
            {
                start--;
            }

            var applicableTo = snapshot.CreateTrackingSpan(
                new SnapshotSpan(line.Start + start, point),
                SpanTrackingMode.EdgeInclusive);

            string prefix = lineText.Substring(start, col - start);

            var completions = new List<Completion>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var item in StaticCompletions)
            {
                var glyph = _glyphService.GetGlyph(item.Group, StandardGlyphItem.GlyphItemPublic);
                completions.Add(new Completion(item.Text, item.Text, item.Desc, glyph, null));
                seen.Add(item.Text);
            }

            // Extract words from the current document
            string fullDoc = snapshot.GetText();
            var matches = Regex.Matches(fullDoc, @"\b[a-zA-Z_][a-zA-Z0-9_]*\b");
            foreach (Match m in matches)
            {
                string word = m.Value;
                if (word.Length >= 2 && !seen.Contains(word))
                {
                    var glyph = _glyphService.GetGlyph(StandardGlyphGroup.GlyphGroupVariable, StandardGlyphItem.GlyphItemPublic);
                    completions.Add(new Completion(word, word, "Identifier", glyph, null));
                    seen.Add(word);
                }
            }

            if (completions.Count > 0)
            {
                completions.Sort((a, b) => string.Compare(a.DisplayText, b.DisplayText, StringComparison.OrdinalIgnoreCase));
                var compSet = new CompletionSet("VBC", "VBC", applicableTo, completions, Enumerable.Empty<Completion>());
                compSet.SelectBestMatch();
                compSet.Filter();
                completionSets.Add(compSet);
            }
        }

        public void Dispose()
        {
            _isDisposed = true;
        }
    }
}
