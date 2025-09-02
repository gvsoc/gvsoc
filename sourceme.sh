if [  -n "${ZSH_VERSION:-}" ]; then
	DIR="$(readlink -f -- "${(%):-%x}")"
	SDK_HOME=$(dirname $DIR)
else
	SDK_HOME="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
fi

export PATH=$SDK_HOME/install/bin:$PATH
export PYTHONPATH=$SDK_HOME/install/python:$PYTHONPATH
export PULPOS_HOME=$SDK_HOME/pulpos

##############################################################################
## 				Envirment Parameters for DRAMSys Integration 				##
##############################################################################
export LD_LIBRARY_PATH=$SDK_HOME/third_party/DRAMSys:$LD_LIBRARY_PATH
