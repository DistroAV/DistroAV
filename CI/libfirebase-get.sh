#!/bin/bash

SDK_VERSION="12.1.0"
ZIP_FILE="firebase_cpp_sdk_${SDK_VERSION}.zip"
URL="https://dl.google.com/firebase/sdk/cpp/${ZIP_FILE}"
FIREBASE_CPP_SDK_DIR="lib/firebase_cpp_sdk"

#rm -rf ${FIREBASE_CPP_SDK_DIR}

if [ -d "${FIREBASE_CPP_SDK_DIR}" ]; then
  echo "firebase_cpp_sdk: ${FIREBASE_CPP_SDK_DIR} already exists; skipping."
  return 0 2>/dev/null || exit 0
fi

mkdir -p ${FIREBASE_CPP_SDK_DIR}

pushd ${FIREBASE_CPP_SDK_DIR} >/dev/null

echo "firebase_cpp_sdk: Downloading v${SDK_VERSION}..."
curl -L ${URL} -o ${ZIP_FILE}

echo "firebase_cpp_sdk: Extracting ${ZIP_FILE}..."
extraction_inclusions=(
  "include/*"
  "libs/linux/x86_64/cxx11/*"
  "libs/darwin/universal/*"
  "CMakeLists.txt"
  "generate_xml_from_google_services_json.py"
  "NOTICES"
  "readme.md"
)
BASE_DIR_NAME=$(basename ${FIREBASE_CPP_SDK_DIR})
prefixed_paths=()
for path in "${extraction_inclusions[@]}"; do
  prefixed_paths+=("${BASE_DIR_NAME}/${path}")
done
unzip ${ZIP_FILE} "${prefixed_paths[@]}" -d ..

echo "firebase_cpp_sdk: Cleaning up..."
rm ${ZIP_FILE}

popd >/dev/null

echo "firebase_cpp_sdk: Setup complete."
