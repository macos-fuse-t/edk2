#!/bin/sh
set -eu

usage()
{
	echo "usage: $0 [debug|release|all]" >&2
	exit 2
}

flavor=${1:-all}
case "${flavor}" in
debug|release|all)
	;;
*)
	usage
	;;
esac

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "${script_dir}/../.." && pwd)
toolchain=${TOOL_CHAIN_TAG:-GCC5}
platform=OvmfPkg/Scorpi/ScorpiX64.dsc
artifacts=Build/ScorpiX64/Artifacts

cd "${repo_dir}"
set --
set +u
. ./edksetup.sh >/dev/null
set -u

build_one()
{
	target=$1
	name=$(printf '%s' "${target}" | tr '[:upper:]' '[:lower:]')
	outdir="${artifacts}/${name}"
	fvdir="Build/ScorpiX64/${target}_${toolchain}/FV"

	build -a X64 -t "${toolchain}" -b "${target}" -p "${platform}"

	mkdir -p "${outdir}"
	cp "${fvdir}/SCORPI_EFI.fd" "${outdir}/SCORPI_EFI.fd"
	cp "${fvdir}/SCORPI_VARS.fd" "${outdir}/SCORPI_VARS.fd"
}

case "${flavor}" in
debug)
	build_one DEBUG
	;;
release)
	build_one RELEASE
	;;
all)
	build_one DEBUG
	build_one RELEASE
	;;
esac
