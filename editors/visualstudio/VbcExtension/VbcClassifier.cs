using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(IClassifierProvider))]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    public class VbcClassifierProvider : IClassifierProvider
    {
        [Import]
        internal IClassificationTypeRegistryService ClassificationRegistry = null!;

        public IClassifier? GetClassifier(ITextBuffer textBuffer)
        {
            if (!VbcContentDefinition.IsVbcBuffer(textBuffer))
                return null;

            return textBuffer.Properties.GetOrCreateSingletonProperty(() => new VbcClassifier(ClassificationRegistry));
        }
    }

    public class VbcClassifier : IClassifier
    {
        private readonly IClassificationType _keywordType;
        private readonly IClassificationType _stringType;
        private readonly IClassificationType _commentType;
        private readonly IClassificationType _numberType;
        private readonly IClassificationType _userType;

        private static readonly HashSet<string> Keywords = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "Module", "Sub", "Function", "Structure", "Class", "Interface", "Enum", "Property",
            "End", "If", "Then", "Else", "ElseIf",
            "For", "To", "Step", "Next", "While", "Do", "Loop", "Until",
            "Select", "Case", "Return", "Imports", "Dim", "Const",
            "As", "New", "And", "Or", "Not", "Xor", "ByVal", "ByRef",
            "True", "False", "Nothing", "Exit", "Continue", "With", "Each", "In",
            "Try", "Catch", "Finally", "Throw", "Using", "TypeOf", "Is", "IsNot", "AddressOf",
            "DirectCast", "CType", "TryCast",
            "Public", "Private", "Friend", "Protected", "Shared", "Static",
            "ReadOnly", "WriteOnly", "Async", "Await", "Yield",
            "Auto", "Declare", "Lib", "Alias", "SyncLock", "Get", "Set",
            "Inherits", "Implements", "Overridable", "Overrides", "MustOverride"
        };

        private static readonly HashSet<string> Types = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "Integer", "Long", "Short", "Byte", "SByte", "Single", "Double", "Decimal",
            "Boolean", "String", "Char", "Object", "Void",
            "IntPtr", "UIntPtr", "UInteger", "ULong", "UShort",
            "HttpClient", "HttpServer", "HttpRequest", "HttpResponse"
        };

        public VbcClassifier(IClassificationTypeRegistryService registry)
        {
            _keywordType = registry.GetClassificationType("keyword");
            _stringType = registry.GetClassificationType("string");
            _commentType = registry.GetClassificationType("comment");
            _numberType = registry.GetClassificationType("number");
            _userType = registry.GetClassificationType("type") ??
                        registry.GetClassificationType("class name") ??
                        registry.GetClassificationType("symbol definition") ??
                        registry.GetClassificationType("keyword");
        }

#pragma warning disable 67
        public event EventHandler<ClassificationChangedEventArgs>? ClassificationChanged;
#pragma warning restore 67

        public IList<ClassificationSpan> GetClassificationSpans(SnapshotSpan span)
        {
            var classifications = new List<ClassificationSpan>();
            ITextSnapshot snapshot = span.Snapshot;
            string text = span.GetText();
            int spanStart = span.Start.Position;

            int i = 0;
            int len = text.Length;

            while (i < len)
            {
                char ch = text[i];

                // 1. Whitespace
                if (char.IsWhiteSpace(ch))
                {
                    i++;
                    continue;
                }

                // 2. Comments (' or Rem)
                if (ch == '\'')
                {
                    int commentLen = len - i;
                    classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + i, commentLen), _commentType));
                    break;
                }
                if ((ch == 'R' || ch == 'r') && i + 3 <= len &&
                    string.Equals(text.Substring(i, 3), "Rem", StringComparison.OrdinalIgnoreCase) &&
                    (i + 3 == len || char.IsWhiteSpace(text[i + 3])))
                {
                    int commentLen = len - i;
                    classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + i, commentLen), _commentType));
                    break;
                }

                // 3. String literals ("...")
                if (ch == '"')
                {
                    int startStr = i;
                    i++; // skip opening quote
                    while (i < len)
                    {
                        if (text[i] == '"')
                        {
                            if (i + 1 < len && text[i + 1] == '"')
                            {
                                i += 2; // escaped double quote
                                continue;
                            }
                            i++; // closing quote
                            break;
                        }
                        i++;
                    }
                    classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + startStr, i - startStr), _stringType));
                    continue;
                }

                // 4. Hex numbers (&H...)
                if (ch == '&' && i + 2 < len && (text[i + 1] == 'H' || text[i + 1] == 'h'))
                {
                    int startNum = i;
                    i += 2;
                    while (i < len && Uri.IsHexDigit(text[i]))
                    {
                        i++;
                    }
                    classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + startNum, i - startNum), _numberType));
                    continue;
                }

                // 5. Decimal and float numbers
                if (char.IsDigit(ch))
                {
                    int startNum = i;
                    bool hasDot = false;
                    while (i < len)
                    {
                        if (char.IsDigit(text[i])) { i++; }
                        else if (text[i] == '.' && !hasDot && i + 1 < len && char.IsDigit(text[i + 1])) { hasDot = true; i++; }
                        else break;
                    }
                    classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + startNum, i - startNum), _numberType));
                    continue;
                }

                // 6. Identifiers, Keywords and Types
                if (char.IsLetter(ch) || ch == '_')
                {
                    int startWord = i;
                    while (i < len && (char.IsLetterOrDigit(text[i]) || text[i] == '_'))
                    {
                        i++;
                    }
                    string word = text.Substring(startWord, i - startWord);

                    if (Keywords.Contains(word))
                    {
                        classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + startWord, i - startWord), _keywordType));
                    }
                    else if (Types.Contains(word))
                    {
                        classifications.Add(new ClassificationSpan(new SnapshotSpan(snapshot, spanStart + startWord, i - startWord), _userType));
                    }
                    continue;
                }

                i++;
            }

            return classifications;
        }
    }
}
