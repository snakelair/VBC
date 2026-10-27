using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(ITaggerProvider))]
    [TagType(typeof(IOutliningRegionTag))]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    public class VbcOutliningTaggerProvider : ITaggerProvider
    {
        public ITagger<T>? CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            if (!VbcContentDefinition.IsVbcBuffer(buffer))
                return null;

            return buffer.Properties.GetOrCreateSingletonProperty(() => new VbcOutliningTagger(buffer)) as ITagger<T>;
        }
    }

    public class VbcOutliningTagger : ITagger<IOutliningRegionTag>
    {
        private readonly ITextBuffer _buffer;

        private static readonly string[] Modifiers = new[]
        {
            "public", "private", "friend", "protected", "shared",
            "async", "overrides", "overridable", "mustoverride",
            "default", "readonly", "writeonly", "iterator"
        };

        public VbcOutliningTagger(ITextBuffer buffer)
        {
            _buffer = buffer;
            _buffer.Changed += (s, e) => TagsChanged?.Invoke(this, new SnapshotSpanEventArgs(new SnapshotSpan(_buffer.CurrentSnapshot, 0, _buffer.CurrentSnapshot.Length)));
        }

        public event EventHandler<SnapshotSpanEventArgs>? TagsChanged;

        public IEnumerable<ITagSpan<IOutliningRegionTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            if (spans.Count == 0) yield break;

            var snapshot = spans[0].Snapshot;
            var stack = new Stack<(string Kind, int StartLine, int StartPos, string Text)>();

            foreach (var line in snapshot.Lines)
            {
                string rawText = line.GetText().Trim();
                if (string.IsNullOrEmpty(rawText) || rawText.StartsWith("'") || rawText.StartsWith("Rem", StringComparison.OrdinalIgnoreCase))
                    continue;

                string text = rawText;
                bool stripped;
                do
                {
                    stripped = false;
                    foreach (var mod in Modifiers)
                    {
                        if (text.StartsWith(mod + " ", StringComparison.OrdinalIgnoreCase))
                        {
                            text = text.Substring(mod.Length).TrimStart();
                            stripped = true;
                            break;
                        }
                    }
                } while (stripped);

                // Check block openers
                if (text.StartsWith("Module ", StringComparison.OrdinalIgnoreCase) || text.Equals("Module", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("Module", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("Sub ", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("Sub", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("Function ", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("Function", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("Structure ", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("Structure", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("Class ", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("Class", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("If ", StringComparison.OrdinalIgnoreCase) && text.EndsWith("Then", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("If", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("For ", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("For", line.LineNumber, line.Start.Position, rawText));
                }
                else if (text.StartsWith("While ", StringComparison.OrdinalIgnoreCase) || text.Equals("While", StringComparison.OrdinalIgnoreCase))
                {
                    stack.Push(("While", line.LineNumber, line.Start.Position, rawText));
                }

                // Check block closers
                bool isClose = false;
                if (text.StartsWith("End Module", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End Sub", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End Function", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End Structure", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End Class", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End If", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("Next", StringComparison.OrdinalIgnoreCase)) { isClose = true; }
                else if (text.StartsWith("End While", StringComparison.OrdinalIgnoreCase)) { isClose = true; }

                if (isClose && stack.Count > 0)
                {
                    var opener = stack.Pop();
                    if (line.LineNumber > opener.StartLine)
                    {
                        var span = new SnapshotSpan(snapshot, opener.StartPos, line.End.Position - opener.StartPos);
                        yield return new TagSpan<IOutliningRegionTag>(span,
                            new OutliningRegionTag(false, false, opener.Text + " ...", opener.Text + "\n...\n" + rawText));
                    }
                }
            }
        }
    }
}
