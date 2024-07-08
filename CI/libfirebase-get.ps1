$SDK_VERSION = "12.1.0"
$ZIP_FILE = "firebase_cpp_sdk_windows_$SDK_VERSION.zip"
$URL = "https://dl.google.com/firebase/sdk/cpp/$ZIP_FILE"
$FIREBASE_CPP_SDK_DIR = "lib/firebase_cpp_sdk"

#Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $FIREBASE_CPP_SDK_DIR

if (Test-Path $FIREBASE_CPP_SDK_DIR) {
    Write-Output "firebase_cpp_sdk: $FIREBASE_CPP_SDK_DIR already exists; skipping."
    return
}

Push-Location "lib"

Write-Output "firebase_cpp_sdk: Downloading v$SDK_VERSION..."
Invoke-WebRequest -Uri $URL -OutFile $ZIP_FILE

Write-Output "firebase_cpp_sdk: Extracting $ZIP_FILE..."
Expand-Archive -Path $ZIP_FILE

$BASE_ZIP_FILE_NAME = $ZIP_FILE -replace '\.zip$'
$BASE_ZIP_OUTPUT_DIR = $BASE_ZIP_FILE_NAME -replace '_\d+\.\d+\.\d+$'
$BASE_FIREBASE_CPP_SDK_DIR = Split-Path -Leaf $FIREBASE_CPP_SDK_DIR
Move-Item -Path $BASE_ZIP_FILE_NAME/* -Destination .
Remove-Item $BASE_ZIP_FILE_NAME
Rename-Item -Path $BASE_ZIP_OUTPUT_DIR -NewName $BASE_FIREBASE_CPP_SDK_DIR

Write-Output "firebase_cpp_sdk: Cleaning up..."
Remove-Item $ZIP_FILE

Pop-Location

Write-Output "firebase_cpp_sdk: Setup complete."
