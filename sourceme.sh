##############################################################################
##              Check Basic Dependency Requirements                         ##
##############################################################################
# Function to compare versions
# version_ge() {
#     printf '%s\n%s' "$2" "$1" | sort -C -V
# }
version_ge() {
    # Pad version strings with zeros to handle missing minor/patch versions
    printf -v ver1 "%03d%03d%03d" $(echo "$1" | tr '.' ' ')
    printf -v ver2 "%03d%03d%03d" $(echo "$2" | tr '.' ' ')
    [[ "$ver1" -ge "$ver2" ]]
}

# Required versions
GCC_REQUIRED="11.2.0"
CMAKE_REQUIRED="3.18.1"
PYTHON_REQUIRED="3.11.3"
PYTHON_CMD="python3"

# Check g++ and gcc versions
if command -v g++ > /dev/null 2>&1 && command -v gcc > /dev/null 2>&1; then
    GCC_VERSION=$(gcc -dumpversion)
    if version_ge "$GCC_VERSION" "$GCC_REQUIRED"; then
        echo "g++ and gcc version is $GCC_VERSION (meets requirement)"
    else
        echo "g++ and gcc version $GCC_VERSION is less than required version $GCC_REQUIRED"
        exit 1
    fi
else
    echo "g++ or gcc is not installed"
    exit 1
fi

# Check cmake version
if command -v cmake > /dev/null 2>&1; then
    CMAKE_VERSION=$(cmake --version | head -n 1 | awk '{print $3}')
    if version_ge "$CMAKE_VERSION" "$CMAKE_REQUIRED"; then
        echo "cmake version is $CMAKE_VERSION (meets requirement)"
    else
        echo "cmake version $CMAKE_VERSION is less than required version $CMAKE_REQUIRED"
        exit 1
    fi
else
    echo "cmake is not installed"
    exit 1
fi

# Check Python 3 version
if command -v python3 > /dev/null 2>&1; then
    PYTHON_VERSION=$(python3 --version | awk '{print $2}')
    if version_ge "$PYTHON_VERSION" "$PYTHON_REQUIRED"; then
        echo "Python3 version is $PYTHON_VERSION (meets requirement)"
    else
        echo "Python3 version $PYTHON_VERSION is less than required version $PYTHON_REQUIRED"
        echo "Checking for 'python' instead..."
        
        # Check Python (fallback)
        if command -v python > /dev/null 2>&1; then
            PYTHON_VERSION=$(python --version | awk '{print $2}')
            if version_ge "$PYTHON_VERSION" "$PYTHON_REQUIRED"; then
                echo "Python version is $PYTHON_VERSION (meets requirement)"
            else
                echo "Python version $PYTHON_VERSION is also less than required version $PYTHON_REQUIRED"
                exit 1
            fi
        else
            echo "'python' is not installed either"
            exit 1
        fi
    fi
else
    echo "Python3 is not installed. Checking for 'python' instead..."
    
    # Check Python (fallback)
    if command -v python > /dev/null 2>&1; then
        PYTHON_VERSION=$(python --version | awk '{print $2}')
        if version_ge "$PYTHON_VERSION" "$PYTHON_REQUIRED"; then
            echo "Python version is $PYTHON_VERSION (meets requirement)"
            PYTHON_CMD="python"
        else
            echo "Python version $PYTHON_VERSION is less than required version $PYTHON_REQUIRED"
            exit 1
        fi
    else
        echo "'python' is not installed either"
        exit 1
    fi
fi

if [  -n "${ZSH_VERSION:-}" ]; then
    DIR="$(readlink -f -- "${(%):-%x}")"
    SDK_HOME=$(dirname $DIR)
else
    SDK_HOME="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
fi

##############################################################################
##              Envirment Parameters for GVSoC                              ##
##############################################################################

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
    echo "Creating virtual environment 'pyenv_softhier'..."
    ${PYTHON_CMD} -m venv pyenv_softhier
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

