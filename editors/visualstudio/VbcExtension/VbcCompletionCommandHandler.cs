using System;
using System.ComponentModel.Composition;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    [Export(typeof(IVsTextViewCreationListener))]
    [Name("VbcCompletionHandlerProvider")]
    [ContentType(VbcContentDefinition.ContentType)]
    [ContentType("Basic")]
    [TextViewRole(PredefinedTextViewRoles.Editable)]
    public class VbcCompletionHandlerProvider : IVsTextViewCreationListener
    {
        [Import]
        internal IVsEditorAdaptersFactoryService AdapterService = null!;

        [Import]
        internal ICompletionBroker CompletionBroker = null!;

        public void VsTextViewCreated(IVsTextView textViewAdapter)
        {
            var textView = AdapterService.GetWpfTextView(textViewAdapter);
            if (textView == null) return;

            if (!VbcContentDefinition.IsVbcBuffer(textView.TextBuffer))
                return;

            textView.Properties.GetOrCreateSingletonProperty(() =>
                new VbcCompletionCommandHandler(textViewAdapter, textView, CompletionBroker));
        }
    }

    public class VbcCompletionCommandHandler : IOleCommandTarget
    {
        private readonly IOleCommandTarget _nextCommandHandler;
        private readonly ITextView _textView;
        private readonly ICompletionBroker _broker;
        private ICompletionSession? _currentSession;

        public VbcCompletionCommandHandler(IVsTextView textViewAdapter, ITextView textView, ICompletionBroker broker)
        {
            _textView = textView;
            _broker = broker;

            textViewAdapter.AddCommandFilter(this, out _nextCommandHandler);
        }

        public int QueryStatus(ref Guid pguidCmdGroup, uint cCmds, OLECMD[] prgCmds, IntPtr pCmdText)
        {
            return _nextCommandHandler.QueryStatus(ref pguidCmdGroup, cCmds, prgCmds, pCmdText);
        }

        public int Exec(ref Guid pguidCmdGroup, uint nCmdID, uint nCmdexecopt, IntPtr pvaIn, IntPtr pvaOut)
        {
            if (VsShellUtilities.IsInAutomationFunction(ServiceProvider.GlobalProvider))
            {
                return _nextCommandHandler.Exec(ref pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);
            }

            uint commandId = nCmdID;
            char typedChar = char.MinValue;

            if (pguidCmdGroup == VSConstants.VSStd2K && nCmdID == (uint)VSConstants.VSStd2KCmdID.TYPECHAR)
            {
                typedChar = (char)(ushort)Marshal.GetObjectForNativeVariant(pvaIn);
            }

            // Commit completion on Tab or Enter if session is active
            if (nCmdID == (uint)VSConstants.VSStd2KCmdID.RETURN || nCmdID == (uint)VSConstants.VSStd2KCmdID.TAB)
            {
                if (_currentSession != null && !_currentSession.IsDismissed)
                {
                    if (_currentSession.SelectedCompletionSet != null &&
                        _currentSession.SelectedCompletionSet.SelectionStatus.IsSelected)
                    {
                        _currentSession.Commit();
                        return VSConstants.S_OK;
                    }
                    _currentSession.Dismiss();
                }
            }

            // Pass the keystroke along to the editor
            int retVal = _nextCommandHandler.Exec(ref pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);

            // Filter on Backspace or Delete
            if (nCmdID == (uint)VSConstants.VSStd2KCmdID.BACKSPACE || nCmdID == (uint)VSConstants.VSStd2KCmdID.DELETE)
            {
                if (_currentSession != null && !_currentSession.IsDismissed)
                {
                    _currentSession.Filter();
                }
            }
            // Trigger completion on letters, underscore, or dot
            else if (char.IsLetter(typedChar) || typedChar == '_' || typedChar == '.')
            {
                TriggerCompletion();
            }
            // Trigger on Ctrl+Space / CompleteWord
            else if (pguidCmdGroup == VSConstants.VSStd2K && nCmdID == (uint)VSConstants.VSStd2KCmdID.COMPLETEWORD)
            {
                TriggerCompletion();
            }

            return retVal;
        }

        private void TriggerCompletion()
        {
            if (_currentSession != null && !_currentSession.IsDismissed)
            {
                _currentSession.Filter();
                return;
            }

            var caret = _textView.Caret.Position.BufferPosition;
            var triggerPoint = caret.Snapshot.CreateTrackingPoint(caret.Position, PointTrackingMode.Positive);

            _currentSession = _broker.CreateCompletionSession(_textView, triggerPoint, true);
            if (_currentSession != null)
            {
                _currentSession.Dismissed += (s, e) => _currentSession = null;
                _currentSession.Start();
            }
        }
    }
}
