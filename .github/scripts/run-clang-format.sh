#!/usr/bin/env bash
#
# Installs clang-format (if needed) and runs it on new or modified C/C++
# source files, using this repo's .clang-format style. Also checks for (and
# fixes) a missing trailing newline at end of file.
#
# Linux/macOS counterpart of .github/scripts/run-clang-format.ps1 (Windows). Written
# for bash 3.2+ so it works with macOS's stock /bin/bash as well as Linux.

set -euo pipefail

REQUIRED_MAJOR=19
EXTENSIONS="c h cpp hpp m mm"

BASE_REF=""
STAGED=false
CHECK=false

usage() {
    cat <<'EOF'
Usage: run-clang-format.sh [options]

Options:
  --base <ref>   Diff against this git ref (e.g. origin/master) instead of
                 the working tree. Formats/checks every file changed on the
                 current branch relative to <ref>.
  --staged       Only consider staged files (git diff --cached). Ignored
                 when --base is given.
  --check        Check formatting only; do not modify files. Exits non-zero
                 if any file would change.
  -h, --help     Show this help.

With neither --base nor --staged, formats uncommitted working-tree changes
(staged, unstaged, and untracked files).
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --base)
            if [[ $# -lt 2 ]]; then
                echo "--base requires a value" >&2
                exit 2
            fi
            BASE_REF="$2"
            shift 2
            ;;
        --staged)
            STAGED=true
            shift
            ;;
        --check)
            CHECK=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "Not inside a git repository." >&2
    exit 1
}
cd "$REPO_ROOT"

get_clang_format_version() {
    local exe="$1" out
    out=$("$exe" --version 2>/dev/null) || return 1
    if [[ "$out" =~ ([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
        return 0
    fi
    return 1
}

find_clang_format() {
    local name exe major
    for name in clang-format-19 clang-format; do
        if command -v "$name" >/dev/null 2>&1; then
            exe=$(command -v "$name")
            major=$(get_clang_format_version "$exe") || continue
            if [[ "$major" == "$REQUIRED_MAJOR" ]]; then
                printf '%s\n' "$exe"
                return 0
            fi
        fi
    done

    # The pip "clang-format" package installs a console script that may not
    # be on PATH (e.g. a --user install whose scripts dir isn't in PATH).
    if command -v python3 >/dev/null 2>&1; then
        local dir candidate dirs
        dirs=$(python3 -c "import sysconfig; print(sysconfig.get_path('scripts', 'posix_user')); print(sysconfig.get_path('scripts'))" 2>/dev/null) || dirs=""
        while IFS= read -r dir; do
            [[ -z "$dir" ]] && continue
            candidate="$dir/clang-format"
            if [[ -x "$candidate" ]]; then
                major=$(get_clang_format_version "$candidate") || continue
                if [[ "$major" == "$REQUIRED_MAJOR" ]]; then
                    printf '%s\n' "$candidate"
                    return 0
                fi
            fi
        done <<< "$dirs"
    fi

    return 1
}

install_clang_format() {
    echo "clang-format ${REQUIRED_MAJOR}.x not found; installing via pip..." >&2

    if ! command -v python3 >/dev/null 2>&1; then
        echo "python3 (with pip) is required to auto-install clang-format. Install it, or install clang-format ${REQUIRED_MAJOR} manually (e.g. 'brew install clang-format' on macOS, or your distro package manager on Linux), then re-run this script." >&2
        exit 1
    fi

    if ! python3 -m pip install --user --upgrade "clang-format~=${REQUIRED_MAJOR}.1"; then
        echo "pip install of clang-format failed." >&2
        exit 1
    fi
}

CLANG_FORMAT=$(find_clang_format) || true
if [[ -z "${CLANG_FORMAT:-}" ]]; then
    install_clang_format
    CLANG_FORMAT=$(find_clang_format) || true
    if [[ -z "${CLANG_FORMAT:-}" ]]; then
        echo "clang-format ${REQUIRED_MAJOR}.x still not available after installation attempt." >&2
        exit 1
    fi
fi
echo "Using clang-format: $CLANG_FORMAT"

is_source_file() {
    local f="$1" ext e
    ext="${f##*.}"
    for e in $EXTENSIONS; do
        [[ "$ext" == "$e" ]] && return 0
    done
    return 1
}

if [[ -n "$BASE_REF" ]]; then
    CHANGED=$(git diff --name-only --diff-filter=ACMR "${BASE_REF}...HEAD")
elif [[ "$STAGED" == true ]]; then
    CHANGED=$(git diff --name-only --cached --diff-filter=ACMR)
else
    CHANGED=$(git diff --name-only --diff-filter=ACMR HEAD; git ls-files --others --exclude-standard)
fi

FILES=()
while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    [[ -f "$f" ]] || continue
    is_source_file "$f" || continue
    FILES+=("$f")
done <<< "$CHANGED"

if [[ ${#FILES[@]} -gt 0 ]]; then
    UNIQUE_FILES=()
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        UNIQUE_FILES+=("$f")
    done < <(printf '%s\n' "${FILES[@]}" | sort -u)
    FILES=("${UNIQUE_FILES[@]}")
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No new or modified C/C++ source files to format."
    exit 0
fi

echo "Files to format:"
printf '  %s\n' "${FILES[@]}"

ends_with_newline() {
    local f="$1"
    [[ -s "$f" ]] || return 0 # empty file counts as fine
    [[ -z "$(tail -c1 "$f")" ]]
}

add_trailing_newline() {
    local f="$1"
    [[ -s "$f" ]] || return 0
    # Match the file's existing line-ending style (CRLF vs LF); default to LF.
    # (Uses od + substring match rather than grep: some grep builds run in a
    # text mode that silently strips \r, which would misdetect CRLF files.)
    local hex
    hex=$(od -An -v -tx1 "$f" | tr -d ' \n')
    if [[ "$hex" == *"0d0a"* ]]; then
        printf '\r\n' >> "$f"
    else
        printf '\n' >> "$f"
    fi
}

if [[ "$CHECK" == true ]]; then
    FAILED=()
    for f in "${FILES[@]}"; do
        if ! "$CLANG_FORMAT" -style=file --dry-run --Werror "$f" 2>/dev/null || ! ends_with_newline "$f"; then
            FAILED+=("$f")
        fi
    done
    if [[ ${#FAILED[@]} -gt 0 ]]; then
        echo "The following files need formatting:" >&2
        printf '  %s\n' "${FAILED[@]}" >&2
        exit 1
    fi
    echo "All files are properly formatted."
else
    if ! "$CLANG_FORMAT" -style=file -i "${FILES[@]}"; then
        echo "clang-format failed." >&2
        exit 1
    fi

    FIXED_EOF=()
    for f in "${FILES[@]}"; do
        if ! ends_with_newline "$f"; then
            add_trailing_newline "$f"
            FIXED_EOF+=("$f")
        fi
    done
    if [[ ${#FIXED_EOF[@]} -gt 0 ]]; then
        echo "Added missing trailing newline to:"
        printf '  %s\n' "${FIXED_EOF[@]}"
    fi

    echo "Formatted ${#FILES[@]} file(s)."
fi
