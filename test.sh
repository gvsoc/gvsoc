if [  -n "${ZSH_VERSION:-}" ]; then
	DIR="$(readlink -f -- "${(%):-%x}")"
	SDK_HOME=$(dirname $DIR)
else
	SDK_HOME="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
fi

source sourceme.sh

export SIRACUSA_SDK_HOME=$SDK_HOME/tests/pulp-sdk-siracusa
export PULP_SDK_HOME=$SDK_HOME/tests/pulp-sdk
export PULPOS_HOME=$SDK_HOME/pulpos
