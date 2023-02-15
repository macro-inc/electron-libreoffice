[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
. "$PSScriptRoot\PathHelpers.ps1"
. "$PSScriptRoot\InstallHelpers.ps1"
. "$PSScriptRoot\VisualStudioHelpers.ps1"

$bootstrapperUrl = "https://aka.ms/vs/16/release/vs_Community.exe"

$workLoads = @(
	"--includeRecommended"
  "--add Microsoft.VisualStudio.Component.VC.ATLMFC"
  "--add Microsoft.VisualStudio.Component.Windows10SDK.20348"
  "--add Microsoft.VisualStudio.Component.Windows11SDK.22000"
  "--add Microsoft.VisualStudio.Workload.NativeDesktop"
)
$workLoadsArgument = [String]::Join(" ", $workLoads)

# Install VS
Install-VisualStudio -BootstrapperUrl $bootstrapperUrl -WorkLoads $workLoadsArgument

# Find the version of VS installed for this instance
# Only supports a single instance
$vsProgramData = Get-Item -Path "C:\ProgramData\Microsoft\VisualStudio\Packages\_Instances"
$instanceFolders = Get-ChildItem -Path $vsProgramData.FullName

if ($instanceFolders -is [array])
{
    Write-Host "More than one instance installed"
    exit 1
}

# Updating content of MachineState.json file to disable autoupdate of VSIX extensions
$vsInstallRoot = (Get-VisualStudioInstance).InstallationPath
$newContent = '{"Extensions":[{"Key":"1e906ff5-9da8-4091-a299-5c253c55fdc9","Value":{"ShouldAutoUpdate":false}},{"Key":"Microsoft.VisualStudio.Web.AzureFunctions","Value":{"ShouldAutoUpdate":false}}],"ShouldAutoUpdate":false,"ShouldCheckForUpdates":false}'
Set-Content -Path "$vsInstallRoot\Common7\IDE\Extensions\MachineState.json" -Value $newContent

# Install Windows 11 SDK version 10.0.22621.0
$sdkUrl = "https://go.microsoft.com/fwlink/p/?linkid=2196241"
$sdkFileName = "sdksetup22621.exe"
$argumentList = ("/q", "/norestart", "/ceip off", "/features OptionId.UWPManaged OptionId.UWPCPP OptionId.UWPLocalized OptionId.DesktopCPPx86 OptionId.DesktopCPPx64 OptionId.DesktopCPParm64")
Install-Binary -Url $sdkUrl -Name $sdkFileName -ArgumentList $argumentList

# Windows 10 SDK (10.0.15063.468), for debugger tools
$sdkUrl = "https://go.microsoft.com/fwlink/?linkid=2164145"
$sdkFileName = "sdksetup15053.exe"
$argumentList = ("/q", "/norestart", "/ceip off", "/features OptionId.WindowsDesktopDebuggers")
Install-Binary -Url $sdkUrl -Name $sdkFileName -ArgumentList $argumentList
