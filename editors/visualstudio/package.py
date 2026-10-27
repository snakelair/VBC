import os
import io
import json
import zipfile
import shutil
import subprocess

vsix_path = r"editors\visualstudio\VbcExtension.vsix"
dll_path = r"editors\visualstudio\VbcExtension\bin\Debug\net472\VbcExtension.dll"
pdb_path = r"editors\visualstudio\VbcExtension\bin\Debug\net472\VbcExtension.pdb"
grammar_path = r"editors\vscode\syntaxes\vbc.tmLanguage.json"
pkgdef_path = r"editors\visualstudio\VbcExtension\VbcExtension.pkgdef"
base_templates_dir = r"editors\visualstudio\VbcExtension\ProjectTemplates"

# Clean old templates directory
if os.path.exists(base_templates_dir):
    shutil.rmtree(base_templates_dir)
os.makedirs(base_templates_dir, exist_ok=True)

vstemplate_xml = """<?xml version="1.0" encoding="utf-8"?>
<VSTemplate Version="3.0.0" xmlns="http://schemas.microsoft.com/developer/vstemplate/2005" Type="Project">
  <TemplateData>
    <Name>VBC Native Application</Name>
    <Description>Creates an ultra-fast native Visual Basic C (VBC) application with zero GC and Goose linear data stack.</Description>
    <ProjectType>VisualBasic</ProjectType>
    <TemplateID>Vbc.NativeApplication</TemplateID>
    <SortOrder>10</SortOrder>
    <CreateNewFolder>true</CreateNewFolder>
    <DefaultName>VbcApp</DefaultName>
    <ProvideDefaultName>true</ProvideDefaultName>
    <LocationField>Enabled</LocationField>
    <EnableLocationBrowseButton>true</EnableLocationBrowseButton>
    <NumberOfParentCategoriesToRollUp>2</NumberOfParentCategoriesToRollUp>
    <LanguageTag>VisualBasic</LanguageTag>
    <PlatformTag>Windows</PlatformTag>
    <ProjectTypeTag>Console</ProjectTypeTag>
  </TemplateData>
  <TemplateContent>
    <Project TargetFileName="$projectname$.vbproj" File="Project.vbproj" ReplaceParameters="true">
      <ProjectItem ReplaceParameters="true" OpenInEditor="true">Program.vbc</ProjectItem>
    </Project>
  </TemplateContent>
</VSTemplate>
"""

template_vbproj = """<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="15.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <Configuration Condition=" '$(Configuration)' == '' ">Debug</Configuration>
    <Platform Condition=" '$(Platform)' == '' ">x64</Platform>
    <ProjectGuid>{$guid2$}</ProjectGuid>
    <OutputType>Exe</OutputType>
    <RootNamespace>$safeprojectname$</RootNamespace>
    <AssemblyName>$safeprojectname$</AssemblyName>
    <TargetFrameworkVersion>v4.7.2</TargetFrameworkVersion>
    <FileAlignment>512</FileAlignment>
    <OutputPath>bin\\$(Configuration)\\</OutputPath>
    <IntermediateOutputPath>obj\\$(Configuration)\\</IntermediateOutputPath>
    <VbcExe Condition=" '$(VbcExe)' == '' ">c:\\Projects My\\VBC\\bin\\vbc.exe</VbcExe>
    <TargetName>$safeprojectname$</TargetName>
    <TargetPath>$(OutputPath)$(TargetName).exe</TargetPath>
    <EnableUnmanagedDebugging>true</EnableUnmanagedDebugging>
  </PropertyGroup>

  <ItemGroup>
    <Compile Include="Program.vbc" />
  </ItemGroup>

  <Target Name="Build">
    <MakeDir Directories="$(OutputPath)" />
    <Exec Command="&quot;$(VbcExe)&quot; compile &quot;$(MSBuildProjectDirectory)\\Program.vbc&quot; -o &quot;$(TargetPath)&quot; --emit-c" />
    <Message Importance="High" Text="[VBC] Successfully built $(TargetPath)" />
  </Target>

  <Target Name="Rebuild" DependsOnTargets="Clean;Build" />

  <Target Name="Clean">
    <Delete Files="$(TargetPath)" />
    <Delete Files="$(TargetPath).c" />
  </Target>
</Project>
"""

template_program = """Imports System

Module Program
    Sub Main()
        Console.WriteLine("Hello from VBC in Visual Studio!")
        Console.WriteLine("100% Native Execution with Zero Garbage Collection.")
    End Sub
End Module
"""

vstman_xml = """<VSTemplateManifest Version="1.0" xmlns="http://schemas.microsoft.com/developer/vstemplatemanifest/2015">
  <VSTemplateContainer TemplateType="Project">
    <RelativePathOnDisk>VisualBasic\\VbcConsoleApp</RelativePathOnDisk>
    <TemplateFileName>MyTemplate.vstemplate</TemplateFileName>
    <VSTemplateHeader>
      <TemplateData xmlns="http://schemas.microsoft.com/developer/vstemplate/2005">
        <TemplateID>Vbc.NativeApplication</TemplateID>
        <Name>VBC Native Application</Name>
        <Description>Creates an ultra-fast native Visual Basic C (VBC) application with zero GC and Goose linear data stack.</Description>
        <ProjectType>VisualBasic</ProjectType>
        <SortOrder>10</SortOrder>
        <CreateNewFolder>true</CreateNewFolder>
        <DefaultName>VbcApp</DefaultName>
        <ProvideDefaultName>true</ProvideDefaultName>
        <LocationField>Enabled</LocationField>
        <EnableLocationBrowseButton>true</EnableLocationBrowseButton>
        <NumberOfParentCategoriesToRollUp>2</NumberOfParentCategoriesToRollUp>
        <LanguageTag>VisualBasic</LanguageTag>
        <LanguageTag>visualbasic</LanguageTag>
        <PlatformTag>Windows</PlatformTag>
        <PlatformTag>windows</PlatformTag>
        <ProjectTypeTag>Console</ProjectTypeTag>
        <ProjectTypeTag>console</ProjectTypeTag>
      </TemplateData>
    </VSTemplateHeader>
  </VSTemplateContainer>
  <VSTemplateContainer TemplateType="Project">
    <RelativePathOnDisk>VisualBasic\\1033\\VbcConsoleApp.zip</RelativePathOnDisk>
    <TemplateFileName>MyTemplate.vstemplate</TemplateFileName>
    <VSTemplateHeader>
      <TemplateData xmlns="http://schemas.microsoft.com/developer/vstemplate/2005">
        <TemplateID>Vbc.NativeApplication.1033</TemplateID>
        <Name>VBC Native Application</Name>
        <Description>Creates an ultra-fast native Visual Basic C (VBC) application with zero GC and Goose linear data stack.</Description>
        <ProjectType>VisualBasic</ProjectType>
        <SortOrder>10</SortOrder>
        <CreateNewFolder>true</CreateNewFolder>
        <DefaultName>VbcApp</DefaultName>
        <ProvideDefaultName>true</ProvideDefaultName>
        <LocationField>Enabled</LocationField>
        <EnableLocationBrowseButton>true</EnableLocationBrowseButton>
        <NumberOfParentCategoriesToRollUp>2</NumberOfParentCategoriesToRollUp>
        <LanguageTag>VisualBasic</LanguageTag>
        <LanguageTag>visualbasic</LanguageTag>
        <PlatformTag>Windows</PlatformTag>
        <PlatformTag>windows</PlatformTag>
        <ProjectTypeTag>Console</ProjectTypeTag>
        <ProjectTypeTag>console</ProjectTypeTag>
      </TemplateData>
    </VSTemplateHeader>
  </VSTemplateContainer>
  <VSTemplateContainer TemplateType="Project">
    <RelativePathOnDisk>VisualBasic\\1049\\VbcConsoleApp.zip</RelativePathOnDisk>
    <TemplateFileName>MyTemplate.vstemplate</TemplateFileName>
    <VSTemplateHeader>
      <TemplateData xmlns="http://schemas.microsoft.com/developer/vstemplate/2005">
        <TemplateID>Vbc.NativeApplication.1049</TemplateID>
        <Name>VBC Native Application</Name>
        <Description>Creates an ultra-fast native Visual Basic C (VBC) application with zero GC and Goose linear data stack.</Description>
        <ProjectType>VisualBasic</ProjectType>
        <SortOrder>10</SortOrder>
        <CreateNewFolder>true</CreateNewFolder>
        <DefaultName>VbcApp</DefaultName>
        <ProvideDefaultName>true</ProvideDefaultName>
        <LocationField>Enabled</LocationField>
        <EnableLocationBrowseButton>true</EnableLocationBrowseButton>
        <NumberOfParentCategoriesToRollUp>2</NumberOfParentCategoriesToRollUp>
        <LanguageTag>VisualBasic</LanguageTag>
        <LanguageTag>visualbasic</LanguageTag>
        <PlatformTag>Windows</PlatformTag>
        <PlatformTag>windows</PlatformTag>
        <ProjectTypeTag>Console</ProjectTypeTag>
        <ProjectTypeTag>console</ProjectTypeTag>
      </TemplateData>
    </VSTemplateHeader>
  </VSTemplateContainer>
</VSTemplateManifest>
"""

# Write manifest
vstman_path = os.path.join(base_templates_dir, "templateManifest0.noloc.vstman")
with open(vstman_path, "w", encoding="utf-8") as f:
    f.write(vstman_xml)

# Write unzipped template files
unzipped_dir = os.path.join(base_templates_dir, "VisualBasic", "VbcConsoleApp")
os.makedirs(unzipped_dir, exist_ok=True)
with open(os.path.join(unzipped_dir, "MyTemplate.vstemplate"), "w", encoding="utf-8") as f:
    f.write(vstemplate_xml)
with open(os.path.join(unzipped_dir, "Project.vbproj"), "w", encoding="utf-8") as f:
    f.write(template_vbproj)
with open(os.path.join(unzipped_dir, "Program.vbc"), "w", encoding="utf-8") as f:
    f.write(template_program)

# Write zipped template packages
zip_bytes = io.BytesIO()
with zipfile.ZipFile(zip_bytes, "w", compression=zipfile.ZIP_DEFLATED) as z:
    z.writestr("MyTemplate.vstemplate", vstemplate_xml)
    z.writestr("Project.vbproj", template_vbproj)
    z.writestr("Program.vbc", template_program)
zip_data = zip_bytes.getvalue()

for sub in ["", "1033", "1049"]:
    target_d = os.path.join(base_templates_dir, "VisualBasic", sub) if sub else os.path.join(base_templates_dir, "VisualBasic")
    os.makedirs(target_d, exist_ok=True)
    with open(os.path.join(target_d, "VbcConsoleApp.zip"), "wb") as f:
        f.write(zip_data)

content_types = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="vsixmanifest" ContentType="text/xml" />
  <Default Extension="dll" ContentType="application/octet-stream" />
  <Default Extension="pdb" ContentType="application/octet-stream" />
  <Default Extension="json" ContentType="application/json" />
  <Default Extension="xml" ContentType="text/xml" />
  <Default Extension="zip" ContentType="application/zip" />
  <Default Extension="vstman" ContentType="text/xml" />
  <Default Extension="vstemplate" ContentType="text/xml" />
  <Default Extension="vbproj" ContentType="text/xml" />
  <Default Extension="vbc" ContentType="text/plain" />
  <Default Extension="pkgdef" ContentType="text/plain" />
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
  <Override PartName="/manifest.json" ContentType="application/json" />
  <Override PartName="/catalog.json" ContentType="application/json" />
  <Override PartName="/extension.vsixmanifest" ContentType="text/xml" />
</Types>
"""

rels = """<?xml version="1.0" encoding="utf-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Type="http://schemas.microsoft.com/developer/vsx-schema/2011" Target="/extension.vsixmanifest" Id="R1" />
  <Relationship Type="http://schemas.microsoft.com/developer/vsx-schema-design/2011" Target="/manifest.json" Id="R2" />
  <Relationship Type="http://schemas.microsoft.com/developer/vsx-schema-design/2011" Target="/catalog.json" Id="R3" />
</Relationships>
"""

manifest_xml = """<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Id="VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882" Version="1.0.1" Language="en-US" Publisher="VBC" />
    <DisplayName>VBC Language Support</DisplayName>
    <Description xml:space="preserve">Visual Basic C (VBC) language extension for Visual Studio with Project Templates, IntelliSense and Goose data stack memory model.</Description>
    <MoreInfo>https://github.com/aardappel/goose</MoreInfo>
    <License></License>
    <Icon></Icon>
    <PreviewImage></PreviewImage>
    <Tags>vbc, visual basic, intellisense, lsp, goose, project template</Tags>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Community" Version="[17.0, 19.0)">
      <ProductArchitecture>amd64</ProductArchitecture>
    </InstallationTarget>
    <InstallationTarget Id="Microsoft.VisualStudio.Enterprise" Version="[17.0, 19.0)">
      <ProductArchitecture>amd64</ProductArchitecture>
    </InstallationTarget>
    <InstallationTarget Id="Microsoft.VisualStudio.Pro" Version="[17.0, 19.0)">
      <ProductArchitecture>amd64</ProductArchitecture>
    </InstallationTarget>
  </Installation>
  <Dependencies>
    <Dependency Id="Microsoft.Framework.NDP" DisplayName="Microsoft .NET Framework" Version="[4.7.2,)" />
  </Dependencies>
  <Prerequisites>
    <Prerequisite Id="Microsoft.VisualStudio.Component.CoreEditor" Version="[17.0, 19.0)" DisplayName="Visual Studio core editor" />
  </Prerequisites>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.VsPackage" d:Source="File" Path="VbcExtension.pkgdef" />
    <Asset Type="Microsoft.VisualStudio.MefComponent" d:Source="File" Path="VbcExtension.dll" />
    <Asset Type="Microsoft.VisualStudio.TextmateGrammar" d:Source="File" Path="Grammars/vbc.tmLanguage.json" />
    <Asset Type="Microsoft.VisualStudio.ProjectTemplate" d:Source="File" Path="ProjectTemplates" />
    <Asset Type="Microsoft.VisualStudio.ProjectTemplate" d:Source="File" Path="ProjectTemplates/VisualBasic/VbcConsoleApp.zip" />
    <Asset Type="Microsoft.VisualStudio.ProjectTemplate" d:Source="File" Path="ProjectTemplates/VisualBasic/1033/VbcConsoleApp.zip" />
    <Asset Type="Microsoft.VisualStudio.ProjectTemplate" d:Source="File" Path="ProjectTemplates/VisualBasic/1049/VbcConsoleApp.zip" />
  </Assets>
</PackageManifest>
"""

all_files = [
    {"fileName": "/extension.vsixmanifest", "sha256": None},
    {"fileName": "/VbcExtension.pkgdef", "sha256": None},
    {"fileName": "/VbcExtension.dll", "sha256": None},
    {"fileName": "/VbcExtension.pdb", "sha256": None},
    {"fileName": "/Grammars/vbc.tmLanguage.json", "sha256": None},
    {"fileName": "/ProjectTemplates/templateManifest0.noloc.vstman", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/VbcConsoleApp/MyTemplate.vstemplate", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/VbcConsoleApp/Project.vbproj", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/VbcConsoleApp/Program.vbc", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/VbcConsoleApp.zip", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/1033/VbcConsoleApp.zip", "sha256": None},
    {"fileName": "/ProjectTemplates/VisualBasic/1049/VbcConsoleApp.zip", "sha256": None}
]

manifest_json = json.dumps({
    "id": "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882",
    "version": "1.0.1",
    "type": "Vsix",
    "vsixId": "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882",
    "extensionDir": "[installdir]\\Common7\\IDE\\Extensions\\VbcLanguage",
    "files": all_files,
    "installSizes": {"targetDrive": 85000},
    "dependencies": {
        "Microsoft.VisualStudio.Component.CoreEditor": "[17.0,19.0)"
    }
})

catalog_json = json.dumps({
    "manifestVersion": "1.1",
    "info": {
        "id": "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882,version=1.0.1",
        "manifestType": "Extension"
    },
    "packages": [
        {
            "id": "Component.VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882",
            "version": "1.0.1",
            "type": "Component",
            "extension": True,
            "dependencies": {
                "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882": "1.0.1",
                "Microsoft.VisualStudio.Component.CoreEditor": "[17.0,19.0)"
            },
            "localizedResources": [
                {
                    "language": "en-US",
                    "title": "VBC Language Support & Project Templates",
                    "description": "Visual Basic C (VBC) language extension for Visual Studio with Project Templates, IntelliSense and Goose data stack memory model."
                }
            ]
        },
        {
            "id": "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882",
            "version": "1.0.1",
            "type": "Vsix",
            "payloads": [
                {"fileName": "VbcExtension.vsix", "size": 35000}
            ],
            "vsixId": "VbcLanguage.4a8e2b91-7f3c-41d6-8b29-e0d19c35b882",
            "extensionDir": "[installdir]\\Common7\\IDE\\Extensions\\VbcLanguage",
            "installSizes": {"targetDrive": 85000}
        }
    ]
})

if os.path.exists(vsix_path):
    os.remove(vsix_path)

with zipfile.ZipFile(vsix_path, "w", compression=zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", content_types)
    z.writestr("_rels/.rels", rels)
    z.writestr("extension.vsixmanifest", manifest_xml)
    z.writestr("manifest.json", manifest_json)
    z.writestr("catalog.json", catalog_json)
    z.write(dll_path, "VbcExtension.dll")
    if os.path.exists(pdb_path):
        z.write(pdb_path, "VbcExtension.pdb")
    z.write(grammar_path, "Grammars/vbc.tmLanguage.json")
    z.write(pkgdef_path, "VbcExtension.pkgdef")
    
    for root, dirs, files in os.walk(base_templates_dir):
        for f in files:
            full_p = os.path.join(root, f)
            arc_p = os.path.relpath(full_p, r"editors\visualstudio\VbcExtension")
            z.write(full_p, arc_p.replace("\\", "/"))

print(f"Created {vsix_path}. Installing via VSIXInstaller...")
vsix_installer = r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\VSIXInstaller.exe"
if os.path.exists(vsix_installer):
    cmd = [vsix_installer, "/quiet", os.path.abspath(vsix_path)]
    res = subprocess.run(cmd)
    print(f"VSIXInstaller exited with code: {res.returncode}")

# Deploy user templates
docs_dir = os.path.expanduser("~/Documents")
for vs_folder in ["Visual Studio 18", "Visual Studio 2022"]:
    user_vb_dir = os.path.join(docs_dir, vs_folder, "Templates", "ProjectTemplates", "Visual Basic")
    if os.path.exists(os.path.join(docs_dir, vs_folder)):
        os.makedirs(user_vb_dir, exist_ok=True)
        user_zip = os.path.join(user_vb_dir, "VbcConsoleApp.zip")
        with open(user_zip, "wb") as f:
            f.write(zip_data)
        print(f"Installed user template to: {user_zip}")

print("\nSuccessfully packaged and registered extension!")
