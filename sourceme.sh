source check_and_set_env.sh
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
export PYTHONPATH=$SDK_HOME/soft_hier/flex_cluster_utilities:$PYTHONPATH

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
# export PATH=$SDK_HOME/third_party/toolchain/install/bin:$PATH


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

