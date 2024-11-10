##############################################################################
##              Check Basic Dependency Requirements                         ##
##############################################################################
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
GCC_COMMANDS=$(compgen -c gcc | grep -E '^gcc-[0-9]+')
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
CMAKE_COMMANDS=$(compgen -c cmake | grep -E '^cmake-[0-9]+')
echo $CMAKE_COMMANDS
FOUND_CMAKE=""
for cmake in $CMAKE_COMMANDS; do
    # Get the version of the gcc binary
    echo $cmake
    CMAKE_VERSION=$($cmake --version | head -n 1 | awk '{print $3}')
    echo $CMAKE_VERSION
    CMAKE_VERSION=$(pad_version "$CMAKE_VERSION")
    echo $CMAKE_VERSION
    
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
echo $PYTHON_COMMANDS
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

##############################################################################
##              Envirment Parameters for GVSoC                              ##
##############################################################################
if [  -n "${ZSH_VERSION:-}" ]; then
    DIR="$(readlink -f -- "${(%):-%x}")"
    SDK_HOME=$(dirname $DIR)
else
    SDK_HOME="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
fi

export PATH=$SDK_HOME/install/bin:$PATH
export PYTHONPATH=$SDK_HOME/install/python:$PYTHONPATH

##############################################################################
##              Envirment Parameters for DRAMSys Integration                ##
##############################################################################
export SYSTEMC_HOME=$SDK_HOME/third_party/systemc_install
export LD_LIBRARY_PATH=${SYSTEMC_HOME}/lib64:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SDK_HOME/third_party/DRAMSys:$LD_LIBRARY_PATH


##############################################################################
##              Envirment Parameters for Tool-Chains                        ##
##############################################################################
export PATH=$SDK_HOME/third_party/toolchain/v1.0.16-pulp-riscv-gcc-centos-7/bin:$PATH


##############################################################################
##              Check and Setup SoftHier Simulatior Envirment               ##
##############################################################################
# Check if "third_party" folder exists; if not, run the preparation step
if [ ! -d "third_party" ]; then
    echo "Running softhier_preparation..."
    make softhier_preparation
else
    if [ -d "third_party/systemc" ] && \
       [ -d "third_party/systemc_install" ] && \
       [ -d "third_party/toolchain" ] && \
       [ -f "third_party/DRAMSys/libDRAMSys_Simulator.so" ]; then
        echo "Skipping softhier_preparation as 'third_party' folder already exists and is complete."
    else
        echo "need rebuild softhier_preparation as 'third_party' folder is broken."
        make clean_preparation
        make softhier_preparation
    fi
fi

# Check if "pyenv_softhier" folder exists; if not, create the virtual environment
if [ ! -d "pyenv_softhier" ]; then
    $PYTHON_CMD -m venv pyenv_softhier
    source pyenv_softhier/bin/activate
    pip install --upgrade pip
    pip3 install -r requirements.txt
else
    echo "Skipping virtual environment creation as 'pyenv_softhier' folder already exists."
fi

if [[ "$VIRTUAL_ENV" != *"pyenv_softhier"* ]]; then
    echo "Activating virtual environment 'pyenv_softhier'..."
    source pyenv_softhier/bin/activate
else
    echo "'pyenv_softhier' environment is already active. Skipping activation."
fi

