using CadPreview;

if (args.Length != 3)
{
    Console.Error.WriteLine("usage: NuGetSmoke <input> <output.png> <expected-native-version>");
    return 2;
}

var messages = 0;
using var preview = Previewer.Render(args[0], new RenderOptions { Size = 256, Log = (_, _) => messages++ });
preview.Save(args[1]);

byte[] signature = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
var written = File.ReadAllBytes(args[1]);
if (!written.AsSpan(0, Math.Min(8, written.Length)).SequenceEqual(signature) || preview.Info.Width != 256)
{
    Console.Error.WriteLine("smoke test failed: output is not a 256 px PNG");
    return 1;
}

using var fromMemory = Previewer.Render(File.ReadAllBytes(args[0]), new RenderOptions { Size = 64 });
var format = Previewer.DetectFormat(args[0]);
if (messages == 0 || fromMemory.Info.Width != 64 || format.Name.Length == 0)
{
    Console.Error.WriteLine("smoke test failed: log, memory rendering or detection did not work");
    return 1;
}

if (Previewer.Version != args[2])
{
    Console.Error.WriteLine($"smoke test failed: native version {Previewer.Version}, expected {args[2]}");
    return 1;
}

Console.WriteLine($"CadPreview {Previewer.Version}: tier {preview.Info.Tier}, reason {preview.Info.Reason}, format {format.Name}");
return 0;
