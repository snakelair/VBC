using System;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(ISmartIndentProvider))]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    public class VbcSmartIndentProvider : ISmartIndentProvider
    {
        public ISmartIndent? CreateSmartIndent(ITextView textView)
        {
            if (!VbcContentDefinition.IsVbcBuffer(textView.TextBuffer))
                return null;

            return new VbcSmartIndent(textView);
        }
    }

    public class VbcSmartIndent : ISmartIndent
    {
        private readonly ITextView _textView;
        private const int TabSize = 4;

        private static readonly string[] Modifiers = new[]
        {
            "public", "private", "friend", "protected", "shared",
            "async", "overrides", "overridable", "mustoverride",
            "default", "readonly", "writeonly", "iterator"
        };

        public VbcSmartIndent(ITextView textView)
        {
            _textView = textView;
        }

        public int? GetDesiredIndentation(ITextSnapshotLine line)
        {
            ITextSnapshot snapshot = line.Snapshot;
            int lineNumber = line.LineNumber;

            if (lineNumber == 0) return 0;

            // Find preceding non-empty line
            ITextSnapshotLine? prevLine = null;
            string prevText = "";
            for (int p = lineNumber - 1; p >= 0; p--)
            {
                var candidate = snapshot.GetLineFromLineNumber(p);
                string text = StripComment(candidate.GetText().Trim());
                if (!string.IsNullOrEmpty(text))
                {
                    prevLine = candidate;
                    prevText = text;
                    break;
                }
            }

            if (prevLine == null) return 0;

            int prevIndent = GetLineIndentation(prevLine);
            string currTrim = StripComment(line.GetText().Trim());

            // Check if previous line opens a block
            if (IsOpenBlock(prevText))
            {
                // If current line already has closing keyword, keep it at prevIndent
                if (IsCloseBlock(currTrim))
                {
                    return prevIndent;
                }
                return prevIndent + TabSize;
            }

            // Check if current line closes a block
            if (IsCloseBlock(currTrim))
            {
                return Math.Max(0, prevIndent - TabSize);
            }

            return prevIndent;
        }

        private static string StripComment(string text)
        {
            int apostrophe = text.IndexOf('\'');
            if (apostrophe >= 0)
            {
                text = text.Substring(0, apostrophe).TrimEnd();
            }

            int remIdx = text.IndexOf("Rem ", StringComparison.OrdinalIgnoreCase);
            if (remIdx >= 0)
            {
                text = text.Substring(0, remIdx).TrimEnd();
            }

            return text;
        }

        private static int GetLineIndentation(ITextSnapshotLine line)
        {
            string raw = line.GetText();
            int spaces = 0;
            foreach (char ch in raw)
            {
                if (ch == ' ') spaces++;
                else if (ch == '\t') spaces += TabSize;
                else break;
            }
            return spaces;
        }

        private static bool IsOpenBlock(string trimmed)
        {
            // Strip modifiers (Public, Private, Shared, etc.)
            bool stripped;
            do
            {
                stripped = false;
                foreach (var mod in Modifiers)
                {
                    if (trimmed.StartsWith(mod + " ", StringComparison.OrdinalIgnoreCase))
                    {
                        trimmed = trimmed.Substring(mod.Length).TrimStart();
                        stripped = true;
                        break;
                    }
                }
            } while (stripped);

            if (trimmed.StartsWith("Module ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.Equals("Module", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Sub ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Function ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Structure ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Class ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Interface ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Enum ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Property ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("For ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("While ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.Equals("While", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Do ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.Equals("Do", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Select ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("With ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.Equals("Try", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Try ", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Catch", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Finally", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("Else", StringComparison.OrdinalIgnoreCase) ||
                trimmed.StartsWith("ElseIf ", StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            // Check multiline If: ends with "Then"
            if (trimmed.StartsWith("If ", StringComparison.OrdinalIgnoreCase) &&
                trimmed.EndsWith("Then", StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            return false;
        }

        private static bool IsCloseBlock(string trimmed)
        {
            return trimmed.StartsWith("End ", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("Next", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("Loop", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("Else", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("ElseIf", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("Catch", StringComparison.OrdinalIgnoreCase) ||
                   trimmed.StartsWith("Finally", StringComparison.OrdinalIgnoreCase);
        }

        public void Dispose()
        {
        }
    }
}
