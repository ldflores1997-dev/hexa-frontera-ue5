using System.IO;
using UnrealBuildTool;

public class HexaFrontera : ModuleRules
{
	public HexaFrontera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// El núcleo de reglas (Core/) es C++ estándar compartido con los tests de CMake.
		bUseUnity = false;
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Core", "include"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent",
			"Slate",
			"SlateCore",
		});

		// Textura blanca global (GWhiteTexture) para dibujar las cartas con Canvas.
		PrivateDependencyModuleNames.Add("RenderCore");
	}
}
