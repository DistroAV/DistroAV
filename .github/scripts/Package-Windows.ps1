[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    # Windows Portable Package
    Log-Group "Archiving Portable ${ProductName}..."
    tree /F "${ProjectRoot}/release/${Configuration}/${ProductName}"

    $PortableRoot = "${ProjectRoot}/release-portable/${Configuration}"
    $NewPluginPath = "${PortableRoot}/plugins/${ProductName}"
    $LegacyPluginDataPath = "${PortableRoot}/data/obs-plugins/${ProductName}"
    $LegacyPluginBinaryPath = "${PortableRoot}/obs-plugins/64bit"

    Remove-Item -Path $PortableRoot -Recurse -Force -ErrorAction SilentlyContinue

    # Package new plugin location for portable plugin distribution:
    # plugins/<plugin>/<plugin>.dll + plugins/<plugin>/data/*
    New-Item -ItemType Directory -Path $NewPluginPath -Force | Out-Null
    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/${ProductName}.dll" -Destination $NewPluginPath
    if ( Test-Path -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/${ProductName}.pdb" ) {
        Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/${ProductName}.pdb" -Destination $NewPluginPath
    }
    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/data" -Destination "${NewPluginPath}/data" -Recurse

    # Package legacy location for compatibility:
    # obs-plugins/64bit/*.dll + data/obs-plugins/<plugin>/*
    New-Item -ItemType Directory -Path $LegacyPluginDataPath -Force | Out-Null
    New-Item -ItemType Directory -Path $LegacyPluginBinaryPath -Force | Out-Null
    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/data/*" -Destination $LegacyPluginDataPath -Recurse
    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit/*" -Destination $LegacyPluginBinaryPath -Recurse
    tree /F "${ProjectRoot}/release-portable/${Configuration}"

    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release-portable/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release-portable/${OutputName}-portable.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    # Windows Installer
    # Declare the location of the InnoSetup setup file
    $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"
    if ( ! ( Test-Path -Path $IsccFile ) ) {
        throw 'InnoSetup install script not found. Run the build script or the CMake build and install procedures first.'
    }

    Log-Information 'Creating InnoSetup installer...'
    Push-Location -Stack BuildTemp
    Ensure-Location -Path "${ProjectRoot}/release"
    Remove-Item -Path Package -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item -Path ${Configuration} -Destination Package -Recurse
    Invoke-External iscc ${IsccFile} /O"${ProjectRoot}/release" /F"${OutputName}-Installer"
    Remove-Item -Path Package -Recurse
    Pop-Location -Stack BuildTemp

    Log-Group
}

Package
