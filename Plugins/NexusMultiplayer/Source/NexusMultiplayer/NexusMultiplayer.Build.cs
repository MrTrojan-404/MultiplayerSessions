// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NexusMultiplayer : ModuleRules
{
	public NexusMultiplayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		   PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore", 
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "OnlineSubsystem",  
            "OnlineSubsystemUtils", 
            "Slate",
            "SlateCore",
            "UMG",
            "Sockets",
            "Networking",
        });

        // ── Steam ─────────────────────────────────────────────────────────────
        // Optional — strip if your project doesn't ship on Steam.
        // To disable: set bEnableSteam = false and remove OnlineSubsystemSteam
        // from your project's .uproject Plugins array.

        bool bEnableSteam = true;
        if (bEnableSteam)
        {
            PrivateDependencyModuleNames.Add("OnlineSubsystemSteam");
            PublicDefinitions.Add("NEXUS_STEAM_ENABLED=1");
        }
        else
        {
            PublicDefinitions.Add("NEXUS_STEAM_ENABLED=0");
        }

        // ── EOS ───────────────────────────────────────────────────────────────
        // Disabled until FNexusEOSOnline is implemented.
        // To enable: set bEnableEOS = true and add EOS SDK to your project.

        bool bEnableEOS = true;
        if (bEnableEOS)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "EOSSDK",
                "OnlineSubsystemEOS",
            });
            PublicDefinitions.Add("NEXUS_EOS_ENABLED=1");
        }
        else
        {
            PublicDefinitions.Add("NEXUS_EOS_ENABLED=0");
        }

        // ── Nakama ────────────────────────────────────────────────────────────
        // Requires the NakamaUnreal plugin in your project's Plugins folder.
        // Download: https://github.com/heroiclabs/nakama-unreal
        // To disable: set bEnableNakama = false — removes all Nakama code paths.

        bool bEnableNakama = true;
        if (bEnableNakama)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "NakamaUnreal"
            });
            PublicDefinitions.Add("NEXUS_NAKAMA_ENABLED=1");
        }
        else
        {
            PublicDefinitions.Add("NEXUS_NAKAMA_ENABLED=0");
        }

        // ── JSON ──────────────────────────────────────────────────────────────
        // Used by NexusNakamaMatchCode for storage payload parsing.

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "JsonUtilities",
        });
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
