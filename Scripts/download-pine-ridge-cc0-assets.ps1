param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$OutputRoot = Join-Path $ProjectRoot "SourceArt\PineRidge\PolyHaven"
$ApiHeaders = @{
    "User-Agent" = "DiscGolfTourAssetPipeline/0.5 local-development"
}
$Downloaded = [System.Collections.Generic.List[object]]::new()

function Get-PolyHavenMetadata {
    param([Parameter(Mandatory)][string]$AssetId)

    Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$AssetId" -Headers $ApiHeaders
}

function Save-VerifiedFile {
    param(
        [Parameter(Mandatory)][string]$AssetId,
        [Parameter(Mandatory)][string]$Role,
        [Parameter(Mandatory)][object]$Descriptor,
        [Parameter(Mandatory)][string]$Destination
    )

    $Parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $Parent | Out-Null

    $NeedsDownload = $Force -or -not (Test-Path -LiteralPath $Destination)
    if (-not $NeedsDownload) {
        $CurrentHash = (Get-FileHash -LiteralPath $Destination -Algorithm MD5).Hash.ToLowerInvariant()
        $NeedsDownload = $CurrentHash -ne $Descriptor.md5
    }

    if ($NeedsDownload) {
        Write-Host "Downloading $AssetId / $Role"
        Invoke-WebRequest -Uri $Descriptor.url -Headers $ApiHeaders -OutFile $Destination
    }

    $Hash = (Get-FileHash -LiteralPath $Destination -Algorithm MD5).Hash.ToLowerInvariant()
    if ($Hash -ne $Descriptor.md5) {
        throw "MD5 mismatch for '$Destination': expected $($Descriptor.md5), received $Hash"
    }

    $File = Get-Item -LiteralPath $Destination
    if ($File.Length -ne [int64]$Descriptor.size) {
        throw "Size mismatch for '$Destination': expected $($Descriptor.size), received $($File.Length)"
    }

    $Downloaded.Add([ordered]@{
        assetId = $AssetId
        role = $Role
        relativePath = $Destination.Substring($ProjectRoot.Length + 1).Replace("\", "/")
        sourceUrl = $Descriptor.url
        byteSize = [int64]$Descriptor.size
        md5 = $Descriptor.md5
        license = "CC0-1.0"
        sourcePage = "https://polyhaven.com/a/$AssetId"
    })
}

$TerrainAssets = @(
    @{ Id = "forrest_ground_01"; Folder = "Textures\ForestGround01" },
    @{ Id = "leafy_grass"; Folder = "Textures\LeafyGrass" },
    @{ Id = "grass_path_2"; Folder = "Textures\GrassPath2" }
)

foreach ($Asset in $TerrainAssets) {
    $Metadata = Get-PolyHavenMetadata -AssetId $Asset.Id
    $DestinationRoot = Join-Path $OutputRoot $Asset.Folder
    Save-VerifiedFile -AssetId $Asset.Id -Role "Diffuse2K" -Descriptor $Metadata.Diffuse.'2k'.jpg `
        -Destination (Join-Path $DestinationRoot "$($Asset.Id)_diff_2k.jpg")
    Save-VerifiedFile -AssetId $Asset.Id -Role "NormalDX2K" -Descriptor $Metadata.nor_dx.'2k'.jpg `
        -Destination (Join-Path $DestinationRoot "$($Asset.Id)_nor_dx_2k.jpg")
    Save-VerifiedFile -AssetId $Asset.Id -Role "Roughness2K" -Descriptor $Metadata.Rough.'2k'.jpg `
        -Destination (Join-Path $DestinationRoot "$($Asset.Id)_rough_2k.jpg")
    Save-VerifiedFile -AssetId $Asset.Id -Role "AmbientOcclusion2K" -Descriptor $Metadata.AO.'2k'.jpg `
        -Destination (Join-Path $DestinationRoot "$($Asset.Id)_ao_2k.jpg")
}

$FirMetadata = Get-PolyHavenMetadata -AssetId "fir_sapling"
$FirRoot = Join-Path $OutputRoot "Models\FirSapling"
Save-VerifiedFile -AssetId "fir_sapling" -Role "StaticMeshFBX1K" -Descriptor $FirMetadata.fbx.'1k'.fbx `
    -Destination (Join-Path $FirRoot "fir_sapling_1k.fbx")

$FirTextureSelections = @(
    @{ Key = "branches_diff"; Role = "BranchesDiffuse1K"; Extension = "jpg" },
    @{ Key = "branches_nor_dx"; Role = "BranchesNormalDX1K"; Extension = "png" },
    @{ Key = "branches_rough"; Role = "BranchesRoughness1K"; Extension = "jpg" },
    @{ Key = "twigs_diff"; Role = "TwigsDiffuse1K"; Extension = "jpg" },
    @{ Key = "twigs_nor_dx"; Role = "TwigsNormalDX1K"; Extension = "png" },
    @{ Key = "twigs_rough"; Role = "TwigsRoughness1K"; Extension = "jpg" },
    @{ Key = "twigs_alpha"; Role = "TwigsOpacity1K"; Extension = "png" }
)

foreach ($Selection in $FirTextureSelections) {
    $Descriptor = $FirMetadata.($Selection.Key).'1k'.($Selection.Extension)
    $Destination = Join-Path $FirRoot "Textures\fir_sapling_$($Selection.Key)_1k.$($Selection.Extension)"
    Save-VerifiedFile -AssetId "fir_sapling" -Role $Selection.Role -Descriptor $Descriptor -Destination $Destination
}

$FixtureModels = @(
    @{
        Id = "boulder_01"
        Folder = "Models\Boulder01"
        TextureSelections = @(
            @{ Key = "Diffuse"; Role = "Diffuse1K"; Extension = "jpg" },
            @{ Key = "nor_dx"; Role = "NormalDX1K"; Extension = "png" },
            @{ Key = "Rough"; Role = "Roughness1K"; Extension = "jpg" },
            @{ Key = "AO"; Role = "AmbientOcclusion1K"; Extension = "jpg" }
        )
    },
    @{
        Id = "shrub_04"
        Folder = "Models\Shrub04"
        TextureSelections = @(
            @{ Key = "Diffuse"; Role = "Diffuse1K"; Extension = "jpg" },
            @{ Key = "nor_dx"; Role = "NormalDX1K"; Extension = "png" },
            @{ Key = "Rough"; Role = "Roughness1K"; Extension = "jpg" },
            @{ Key = "AO"; Role = "AmbientOcclusion1K"; Extension = "jpg" },
            @{ Key = "Alpha"; Role = "Opacity1K"; Extension = "png" }
        )
    }
)

foreach ($Asset in $FixtureModels) {
    $Metadata = Get-PolyHavenMetadata -AssetId $Asset.Id
    $DestinationRoot = Join-Path $OutputRoot $Asset.Folder
    Save-VerifiedFile -AssetId $Asset.Id -Role "StaticMeshFBX1K" -Descriptor $Metadata.fbx.'1k'.fbx `
        -Destination (Join-Path $DestinationRoot "$($Asset.Id)_1k.fbx")
    foreach ($Selection in $Asset.TextureSelections) {
        $Descriptor = $Metadata.($Selection.Key).'1k'.($Selection.Extension)
        $Destination = Join-Path $DestinationRoot "Textures\$($Asset.Id)_$($Selection.Key.ToLowerInvariant())_1k.$($Selection.Extension)"
        Save-VerifiedFile -AssetId $Asset.Id -Role $Selection.Role -Descriptor $Descriptor -Destination $Destination
    }
}

$SignWoodMetadata = Get-PolyHavenMetadata -AssetId "weathered_planks"
$SignWoodRoot = Join-Path $OutputRoot "Textures\WeatheredPlanks"
$SignWoodSelections = @(
    @{ Key = "Diffuse"; Role = "Diffuse1K"; Extension = "jpg" },
    @{ Key = "nor_dx"; Role = "NormalDX1K"; Extension = "png" },
    @{ Key = "Rough"; Role = "Roughness1K"; Extension = "jpg" },
    @{ Key = "AO"; Role = "AmbientOcclusion1K"; Extension = "jpg" }
)
foreach ($Selection in $SignWoodSelections) {
    $Descriptor = $SignWoodMetadata.($Selection.Key).'1k'.($Selection.Extension)
    $Destination = Join-Path $SignWoodRoot "weathered_planks_$($Selection.Key.ToLowerInvariant())_1k.$($Selection.Extension)"
    Save-VerifiedFile -AssetId "weathered_planks" -Role $Selection.Role -Descriptor $Descriptor -Destination $Destination
}

$Manifest = [ordered]@{
    schema = "disc_golf_third_party_source_manifest"
    schemaVersion = 1
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    source = "Poly Haven"
    license = "CC0-1.0"
    licenseUrl = "https://polyhaven.com/license"
    selection = [ordered]@{
        terrainResolution = "2K"
        foliageTextureResolution = "1K"
        fixtureTextureResolution = "1K"
        terrainFormat = "JPG"
        normalConvention = "DirectX"
        foliageGeometry = "FBX"
        fixtureGeometry = "FBX"
    }
    files = $Downloaded
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$ManifestPath = Join-Path $OutputRoot "asset_manifest.json"
$Manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ManifestPath -Encoding utf8
Write-Host "Verified $($Downloaded.Count) files. Manifest: $ManifestPath"
