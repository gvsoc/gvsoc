if [  -n "${ZSH_VERSION:-}" ]; then
   	DIR="$(readlink -f -- "${(%):-%x}")"
   	SDK_HOME=$(dirname $DIR)
else
   	SDK_HOME="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
fi

if [ -n "${GVSOC_WORKDIR}" ]; then
    SDK_INSTALL=${GVSOC_WORKDIR}
else
    SDK_INSTALL=${SDK_HOME}
fi

export PATH=$SDK_INSTALL/install/bin:$PATH
export PYTHONPATH=$SDK_INSTALL/install/python:$PYTHONPATH

##############################################################################
## 				Envirment Parameters for DRAMSys Integration 				##
##############################################################################
export LD_LIBRARY_PATH=$SDK_HOME/third_party/DRAMSys:$LD_LIBRARY_PATH
