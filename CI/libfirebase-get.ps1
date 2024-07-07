$SDK_VERSION = "12.1.0"
$ZIP_FILE = "firebase_cpp_sdk_windows_$SDK_VERSION.zip"
$URL = "https://dl.google.com/firebase/sdk/cpp/$ZIP_FILE"
$FIREBASE_CPP_SDK_DIR = "lib/firebase_cpp_sdk"

#Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $FIREBASE_CPP_SDK_DIR

if (Test-Path $FIREBASE_CPP_SDK_DIR) {
    Write-Output "Directory $FIREBASE_CPP_SDK_DIR already exists. Exiting script."
    return
}

New-Item -ItemType Directory -Path $FIREBASE_CPP_SDK_DIR

Push-Location $FIREBASE_CPP_SDK_DIR

Write-Output "Downloading Firebase C++ SDK version $SDK_VERSION..."
Invoke-WebRequest -Uri $URL -OutFile $ZIP_FILE

Write-Output "Extracting $ZIP_FILE..."
$extraction_inclusions = @(
    "include/*",
    "libs/windows/*/MD/x64/*",
    "CMakeLists.txt",
    "generate_xml_from_google_services_json.exe",
    "NOTICES",
    "readme.md"
)
$BASE_DIR_NAME = Split-Path -Leaf $FIREBASE_CPP_SDK_DIR
$prefixed_paths = $extraction_inclusions | ForEach-Object { "$BASE_DIR_NAME/$_" }

# Unzip the selected files
Expand-Archive -Path $ZIP_FILE -DestinationPath ".."

# Manually move the specific files/directories to the correct location
$prefixed_paths | ForEach-Object {
    $source = Join-Path -Path ".." -ChildPath $_
    if (Test-Path -Path $source) {
        Move-Item -Path $source -Destination "."
    }
}

Write-Output "Cleaning up..."
Remove-Item $ZIP_FILE

Pop-Location

Write-Output "Firebase C++ SDK setup complete."
