#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export WORKSPACE=$ROOT
export PACKAGES_PATH=$ROOT/drivers/DarwinBoot:$ROOT/crabapple:$ROOT/crabapple/Mu_Basecore
export EDK_TOOLS_PATH=$ROOT/crabapple/Mu_Basecore/BaseTools
export CONF_PATH=$ROOT/crabapple/Build/Conf
export CLANG_BIN=${CLANG_BIN:-$ROOT/.cache/toolchain/}
export PYTHON_COMMAND=python3
export PATH=$EDK_TOOLS_PATH/BinWrappers/PosixLike:$EDK_TOOLS_PATH/Source/C/bin:$PATH
cd "$ROOT"
build -p DarwinBootPkg/DarwinBoot.dsc -a AARCH64 -b "${TARGET:-DEBUG}" -t CLANGPDB -n "${JOBS:-4}"
