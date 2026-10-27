using System;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Utilities;

namespace Vbc.VisualStudio
{
    public static class VbcContentDefinition
    {
        public const string ContentType = "vbc";

        [Export]
        [Name(ContentType)]
        [BaseDefinition("code")]
        public static ContentTypeDefinition VbcContentTypeDefinition = null!;

        [Export]
        [FileExtension(".vbc")]
        [ContentType(ContentType)]
        public static FileExtensionToContentTypeDefinition VbcFileExtensionDefinition = null!;

        public static bool IsVbcBuffer(ITextBuffer buffer)
        {
            if (buffer.ContentType.IsOfType(ContentType))
                return true;

            if (buffer.Properties.TryGetProperty(typeof(ITextDocument), out ITextDocument doc))
            {
                if (doc != null && !string.IsNullOrEmpty(doc.FilePath))
                {
                    return doc.FilePath.EndsWith(".vbc", StringComparison.OrdinalIgnoreCase);
                }
            }

            return false;
        }

        static VbcContentDefinition()
        {
            EnsureProjectTemplateInstalled();
        }

        public static void EnsureProjectTemplateInstalled()
        {
            try
            {
                string docs = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
                if (string.IsNullOrEmpty(docs) || !System.IO.Directory.Exists(docs)) return;

                string[] vsDirs = System.IO.Directory.GetDirectories(docs, "Visual Studio *");
                string extensionDir = System.IO.Path.GetDirectoryName(typeof(VbcContentDefinition).Assembly.Location) ?? "";

                string[] candidates = new[]
                {
                    System.IO.Path.Combine(extensionDir, "ProjectTemplates", "VisualBasic", "1033", "VbcConsoleApp.zip"),
                    System.IO.Path.Combine(extensionDir, "ProjectTemplates", "VisualBasic", "VbcConsoleApp.zip"),
                    System.IO.Path.Combine(extensionDir, "ProjectTemplates", "VbcConsoleApp.zip")
                };

                string? srcZip = null;
                foreach (var c in candidates)
                {
                    if (System.IO.File.Exists(c)) { srcZip = c; break; }
                }

                if (srcZip != null)
                {
                    foreach (var vsDir in vsDirs)
                    {
                        string targetDir = System.IO.Path.Combine(vsDir, "Templates", "ProjectTemplates", "Visual Basic");
                        System.IO.Directory.CreateDirectory(targetDir);
                        string targetZip = System.IO.Path.Combine(targetDir, "VbcConsoleApp.zip");
                        System.IO.File.Copy(srcZip, targetZip, true);
                    }
                }
            }
            catch { }
        }
    }
}
