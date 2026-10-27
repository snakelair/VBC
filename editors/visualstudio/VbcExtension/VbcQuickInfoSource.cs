using System;
using System.ComponentModel.Composition;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(IAsyncQuickInfoSourceProvider))]
    [Name("VbcQuickInfoSourceProvider")]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    [Order]
    public class VbcQuickInfoSourceProvider : IAsyncQuickInfoSourceProvider
    {
        public IAsyncQuickInfoSource? TryCreateQuickInfoSource(ITextBuffer textBuffer)
        {
            if (!VbcContentDefinition.IsVbcBuffer(textBuffer))
                return null;

            return new VbcQuickInfoSource(textBuffer);
        }
    }

    public class VbcQuickInfoSource : IAsyncQuickInfoSource
    {
        private readonly ITextBuffer _textBuffer;
        private bool _isDisposed;

        public VbcQuickInfoSource(ITextBuffer textBuffer)
        {
            _textBuffer = textBuffer;
        }

        public Task<QuickInfoItem?> GetQuickInfoItemAsync(IAsyncQuickInfoSession session, CancellationToken cancellationToken)
        {
            if (_isDisposed) return Task.FromResult<QuickInfoItem?>(null);

            var triggerPoint = session.GetTriggerPoint(_textBuffer.CurrentSnapshot);
            if (triggerPoint == null) return Task.FromResult<QuickInfoItem?>(null);

            var snapshot = _textBuffer.CurrentSnapshot;
            var point = triggerPoint.Value;
            var line = point.GetContainingLine();
            string lineText = line.GetText();
            int col = point.Position - line.Start.Position;

            int start = col;
            while (start > 0 && (char.IsLetterOrDigit(lineText[start - 1]) || lineText[start - 1] == '_' || lineText[start - 1] == '?' || lineText[start - 1] == '.' || lineText[start - 1] == '@'))
            {
                start--;
            }

            int end = col;
            while (end < lineText.Length && (char.IsLetterOrDigit(lineText[end]) || lineText[end] == '_' || lineText[end] == '?'))
            {
                end++;
            }

            if (start >= end) return Task.FromResult<QuickInfoItem?>(null);

            string word = lineText.Substring(start, end - start).Trim();
            string? tooltip = GetCTypeTooltip(word);

            if (string.IsNullOrEmpty(tooltip))
                return Task.FromResult<QuickInfoItem?>(null);

            var span = snapshot.CreateTrackingSpan(
                new SnapshotSpan(line.Start + start, line.Start + end),
                SpanTrackingMode.EdgeInclusive);

            return Task.FromResult<QuickInfoItem?>(new QuickInfoItem(span, tooltip));
        }

        private static string? GetCTypeTooltip(string word)
        {
            switch (word.ToLowerInvariant())
            {
                case "integer":
                    return "Type Integer [C: int32_t]\n32-bit signed integer in C99.";
                case "long":
                    return "Type Long [C: int64_t]\n64-bit signed integer in C99.";
                case "short":
                    return "Type Short [C: int16_t]\n16-bit signed integer in C99.";
                case "byte":
                    return "Type Byte [C: uint8_t]\n8-bit unsigned integer in C99.";
                case "sbyte":
                    return "Type SByte [C: int8_t]\n8-bit signed integer in C99.";
                case "single":
                    return "Type Single [C: float]\nSingle-precision 32-bit floating point.";
                case "double":
                    return "Type Double [C: double]\nDouble-precision 64-bit floating point.";
                case "string":
                    return "Type String [C: vbc_string_t (const char* ptr, int64_t len)]\nZero-GC linear stack string slice.";
                case "boolean":
                    return "Type Boolean [C: bool (int32_t)]\nBoolean flag.";
                case "char":
                    return "Type Char [C: char (int8_t)]\nSingle character.";
                case "object":
                    return "Type Object [C: void*]\nGeneric reference pointer.";
                case "void":
                    return "Type Void [C: void]\nEmpty return type.";
                case "intptr":
                    return "Type IntPtr [C: intptr_t]\nNative pointer-sized integer.";
                case "uintptr":
                    return "Type UIntPtr [C: uintptr_t]\nNative pointer-sized unsigned integer.";
                case "uinteger":
                    return "Type UInteger [C: uint32_t]\n32-bit unsigned integer in C99.";
                case "ulong":
                    return "Type ULong [C: uint64_t]\n64-bit unsigned integer in C99.";
                case "ushort":
                    return "Type UShort [C: uint16_t]\n16-bit unsigned integer in C99.";
                case "interface":
                    return "Keyword Interface [C: vbc_iface_t { void* obj; const void* _vtable; }]\nFat-pointer multiple interface dispatch.";
                case "implements":
                    return "Keyword Implements [C: Static interface table mapping]\nBinds class methods to interface vtable slots.";
                case "xdocument":
                    return "Type XDocument [C: vbc_xml_doc_t*]\nNative C XML Document DOM.";
                case "xelement":
                    return "Type XElement [C: vbc_xml_elem_t*]\nNative C XML Element DOM supporting XML Axis (.<child>, .@attr, ...<descendant>).";
                case "xattribute":
                    return "Type XAttribute [C: vbc_xml_attr_t*]\nNative C XML Attribute node.";
                case "stringbuilder":
                    return "Type StringBuilder [C: vbc_string_builder_t { char* data; int64_t len; int64_t cap; }]\nMutable string buffer on linear arena stack.";
                case "stopwatch":
                    return "Type Stopwatch [C: vbc_stopwatch_t { int64_t start_ticks; int64_t elapsed_ticks; bool is_running; }]\nHigh-resolution monotonic timer.";
                case "datetime":
                    return "Type DateTime [C: vbc_datetime_t (int64_t ticks)]\nTimestamp represented in 100ns ticks.";
                case "timespan":
                    return "Type TimeSpan [C: vbc_timespan_t (int64_t ticks)]\nTime duration represented in 100ns ticks.";
                case "guid":
                    return "Type Guid [C: vbc_guid_t (uint8_t bytes[16])]\n128-bit RFC 4122 UUIDv4 identifier.";
                case "math":
                    return "Namespace System.Math [C: <math.h>]\nStandard C99 math primitives.";
                case "path":
                    return "Namespace System.IO.Path [C: vbc_bcl.h path functions]\nCross-platform path operations.";
                case "file":
                    return "Namespace System.IO.File [C: vbc_bcl.h file functions]\nNative OS file I/O operations.";
                case "directory":
                    return "Namespace System.IO.Directory [C: vbc_bcl.h directory functions]\nNative OS directory operations.";
                case "environment":
                    return "Namespace System.Environment [C: vbc_bcl.h OS environment]\nOperating system environment properties.";
                case "dim":
                    return "Keyword Dim [C: Stack-allocated local / struct field]\nDeclares variable storage.";
                case "sub":
                    return "Keyword Sub [C: void func(vbc_stack_t* _stack, ...)]\nDeclares procedure returning void.";
                case "function":
                    return "Keyword Function [C: ReturnType func(vbc_stack_t* _stack, ...)]\nDeclares procedure returning a value.";
                case "class":
                    return "Keyword Class [C: typedef struct Name { const Name_VTable* _vtable; ... } Name;]\nReference type allocated on heap.";
                case "structure":
                    return "Keyword Structure [C: typedef struct Name { ... } Name;]\nValue type allocated on stack.";
                case "byval":
                    return "Keyword ByVal [C: T arg]\nArgument passed by value.";
                case "byref":
                    return "Keyword ByRef [C: T* arg]\nArgument passed by pointer.";
                case "httpclient":
                    return "Class HttpClient [C: vbc_http_client_t*]\nZero-alloc native HTTP client.";
                case "httpserver":
                    return "Class HttpServer [C: vbc_http_server_t*]\nHigh-performance native HTTP server.";
                case "console":
                    return "Module Console [C: stdout / stderr]\nConsole I/O.";
                default:
                    if (word.EndsWith("?"))
                    {
                        return $"Type {word} [C: vbc_nullable_{word.TrimEnd('?')} {{ bool has_value; {word.TrimEnd('?')} value; }}]\nNullable value type with .HasValue and .Value.";
                    }
                    return null;
            }
        }

        public void Dispose()
        {
            _isDisposed = true;
        }
    }
}
