pad_version() {
    local version="$1"
    # Split version by dots and count components
    IFS='.' read -r -a parts <<< "$version"
    while [ "${#parts[@]}" -lt 3 ]; do
        parts+=("9")  # Append "0" to missing components
    done
    echo "${parts[0]}.${parts[1]}.${parts[2]}"
}

# Function to compare versions
version_ge() {
    printf '%s\n%s' "$2" "$1" | sort -C -V
}

# Required versions
GCC_REQUIRED="11.2.0"
CMAKE_REQUIRED="3.18.1"
PYTHON_REQUIRED="3.10.3"


# Check gcc commands and version
GCC_COMMANDS=$(compgen -c gcc | grep -E '^gcc(-[0-9]+)?$')
FOUND_GCC=""
for gcc in $GCC_COMMANDS; do
    # Get the version of the gcc binary
    GCC_VERSION=$($gcc -dumpversion 2>/dev/null)
    GCC_VERSION=$(pad_version "$GCC_VERSION")
    
    # Check if the version matches the required version
    if version_ge "$GCC_VERSION" "$GCC_REQUIRED"; then
        FOUND_GCC="$gcc"
        break
    fi
done

# Export the found gcc version or exit if not found
if [ -n "$FOUND_GCC" ]; then
    export CC="$FOUND_GCC"
    echo "gcc >=$GCC_REQUIRED found at $FOUND_GCC (version = $($FOUND_GCC -dumpversion 2>/dev/null)) and set as CC."
else
    echo "No gcc version $GCC_REQUIRED found on the system."
    return 1
fi



# Check cmake commands and version
CMAKE_COMMANDS=$(compgen -c cmake | grep -E '^cmake(-[0-9]+)?$')
FOUND_CMAKE=""
for cmake in $CMAKE_COMMANDS; do
    # Get the version of the gcc binary
    CMAKE_VERSION=$($cmake --version | head -n 1 | awk '{print $3}')
    CMAKE_VERSION=$(pad_version "$CMAKE_VERSION")
    
    # Check if the version matches the required version
    if version_ge "$CMAKE_VERSION" "$CMAKE_REQUIRED"; then
        FOUND_CMAKE="$cmake"
        break
    fi
done

# Export the found gcc version or exit if not found
if [ -n "$FOUND_CMAKE" ]; then
    export CMAKE="$FOUND_CMAKE"
    echo "cmake >= $CMAKE_REQUIRED found at $FOUND_CMAKE (version = $($FOUND_CMAKE --version | head -n 1 | awk '{print $3}')) and set as CMAKE."
else
    echo "No cmake version $CMAKE_REQUIRED found on the system."
    return 1
fi


# Check python commands and version
PYTHON_COMMANDS=$(compgen -c python | grep -E '^python([0-9]+)?$' | grep -v '^python2')
FOUND_PYTHON=""
for pycmd in $PYTHON_COMMANDS; do
    # Get the version of the gcc binary
    PYTHON_VERSION=$($pycmd --version | grep -oP '\d+\.\d+\.\d+')
    PYTHON_VERSION=$(pad_version "$PYTHON_VERSION")
    
    # Check if the version matches the required version
    if version_ge "$PYTHON_VERSION" "$PYTHON_REQUIRED"; then
        FOUND_PYTHON="$pycmd"
        break
    fi
done

# Export the found gcc version or exit if not found
if [ -n "$FOUND_PYTHON" ]; then
    export PYTHON_CMD="$FOUND_PYTHON"
    echo "python >= $PYTHON_REQUIRED found at $FOUND_PYTHON (version = $($FOUND_PYTHON --version | grep -oP '\d+\.\d+\.\d+')) and set as PYTHON_CMD."
else
    echo "No python version $PYTHON_REQUIRED found on the system."
    return 1
fi