# CadPreview

Read-only 2D PNG previews for CAD files. This package wraps the native `cadpreview` library
(Windows x64 and ARM64).

```csharp
using CadPreview;

using var preview = Previewer.Render("part.bin", new RenderOptions { Size = 512 });
preview.Save("part.png");
Console.WriteLine($"tier {preview.Info.Tier}, ok {preview.Info.Ok}, reason {preview.Info.Reason}");
```

Unreadable or unsupported files do not throw. They produce a placeholder image with
`Info.Ok == false` and a `Reason`. Only invalid arguments throw.

Version 0.1 renders placeholders only; format support arrives in later releases. The native
library needs the Microsoft Visual C++ 2015-2026 runtime.

Source, license (Apache-2.0) and third-party notices: https://github.com/ibarra000/cadpreview
