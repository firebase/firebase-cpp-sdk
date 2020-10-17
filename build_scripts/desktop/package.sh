#!/bin/bash -e
PYTHON=python

builtpath=$1
packagepath=$2
platform=$3
bindirrel=$4
bindir=$(cd $bindirrel && pwd -P)

# Desktop packaging settings.
if [[ "${platform}" == "windows" ]]; then
    prefix=''
    ext='lib'
else
    prefix='lib'
    ext='a'
fi


# Library dependencies to merge. Each should be a whitespace-delimited list of path globs.
deps_firebase_app="
*${prefix}firebase_instance_id_desktop_impl.${ext}
*${prefix}firebase_rest_lib.${ext}
"
deps_hidden_firebase_app="
*${prefix}curl.${ext}
*${prefix}z.${ext}
*${prefix}flatbuffers.${ext}
"
deps_hidden_firebase_database="
*${prefix}uv_a.${ext}
*${prefix}libuWS.${ext}
"
deps_hidden_firebase_firestore="
*/firestore-build/Firestore/*/${prefix}*.${ext}
*/firestore-build/*/grpc-build*/${prefix}*.${ext}
*/firestore-build/*/leveldb-build*/${prefix}*.${ext}
*/firestore-build/*/nanopb-build*/${prefix}*.${ext}
"

declare -a hide_namespaces
hide_namespaces=(flatbuffers flexbuffers reflection ZLib bssl uWS absl
base_raw_logging ConnectivityWatcher grpc
grpc_access_token_credentials grpc_alts_credentials
grpc_alts_server_credentials grpc_auth_context
grpc_channel_credentials grpc_channel_security_connector
grpc_chttp2_hpack_compressor grpc_chttp2_stream grpc_chttp2_transport
grpc_client_security_context grpc_composite_call_credentials
grpc_composite_channel_credentials grpc_core grpc_deadline_state
grpc_google_default_channel_credentials grpc_google_iam_credentials
grpc_google_refresh_token_credentials grpc_impl grpc_local_credentials
grpc_local_server_credentials grpc_md_only_test_credentials
grpc_message_compression_algorithm_for_level
grpc_oauth2_token_fetcher_credentials grpc_plugin_credentials
grpc_server_credentials grpc_server_security_connector
grpc_server_security_context
grpc_service_account_jwt_access_credentials grpc_ssl_credentials
grpc_ssl_server_credentials grpc_tls_credential_reload_config
grpc_tls_server_authorization_check_config GrpcUdpListener leveldb
leveldb_filterpolicy_create_bloom leveldb_writebatch_iterate strings
TlsCredentials TlsServerCredentials tsi)

# String to prepend to all hidden symbols.
rename_string=f_b_

declare -a demangle_cmds=${bindir}/c++filt
#,${bindir}/demumble
binutils_objcopy=${bindir}/objcopy
binutils_nm=${bindir}/nm
binutils_ar=${bindir}/ar

cache_file=/tmp/merge_libraries_cache.$$
declare -a additional_merge_libraries_params
merge_libraries_params=(
    --cache=${cache_file}
    --binutils_nm_cmd=${binutils_nm}
    --binutils_ar_cmd=${binutils_ar}
    --binutils_objcopy_cmd=${binutils_objcopy}
    --demangle_cmds=${demangle_cmds}
    --platform=${platform}
    --hide_cpp_namespaces=$(echo "${hide_namespaces[*]}" | sed 's| |,|g')
#    --verbosity=3
)

if [[ -z "${builtpath}" || -z "${packagepath}" || -z "${platform}" || -z "${bindir}" ]]; then
    echo "Usage: $0 <built desktop SDK path> <path to put packaged files into> <platform> <packaging tools directory>"
    echo "Platform is one of: linux darwin windows"
    echo "Packaging tools directory must contain: nm ar objcopy c++filt demumble"
    exit 1
fi

if [[ "${platform}" != "linux" && "${platform}" != "darwin" && "${platform}" != "windows" ]]; then
    echo "Invalid platform specified: ${platform}"
    exit 2
fi

if [[ ! -r "${builtpath}/CMakeCache.txt" ]]; then
    echo "Built desktop SDK not found at path '${builtpath}'."
    exit 2
fi

if [[ ! -x "${binutils_objcopy}" || ! -x "${binutils_ar}" || ! -x "${binutils_nm}" ]]; then
    echo "Packaging tools not found at path '${bindir}'."
    exit 2
fi


origpath=$( pwd -P )

mkdir -p "${packagepath}"
cd "${packagepath}"
destpath=$( pwd -P )
cd "${origpath}"

cd "${builtpath}"
sourcepath=$( pwd -P )
cd "${origpath}"

variant="x64"
mkdir -p "${destpath}/libs/${platform}/${variant}"

cd "${sourcepath}"

# Gather a comma-separated list of all files.
declare allfiles
for lib in $(find . -path '*.a'); do
    if [[ ! -z ${allfiles} ]]; then allfiles+=","; fi
    allfiles+="${lib}"
done

for product in *; do
    if [[ "${platform}" == "windows" ]]; then
	libfile="${product}/firebase_${product}.lib"
    else
	libfile="${product}/libfirebase_${product}.a"
    fi
    
    if [[ ! -r "${libfile}" ]]; then
	continue
    fi
    deps_var="deps_firebase_${product}"  # variable name
    deps_hidden_var="deps_hidden_firebase_${product}"  # variable name
    declare -a deps deps_basenames
    deps=() # regular list of all deps, hidden and visible
    deps_basenames=()  # Only used for script logging
    for dep in ${!deps_var} ${!deps_hidden_var}; do
	for found in $(find . -path ${dep}); do
	    deps[${#deps[@]}]="${found}"
	    deps_basenames[${#deps_basenames[@]}]=$(basename "${found}")
	done
    done
    deps_hidden='' # comma-separated list of hidden deps only
    for dep in ${!deps_hidden_var}; do
	for found in $(find . -path ${dep}); do
	    if [[ ! -z ${deps_hidden} ]]; then deps_hidden+=","; fi
	    deps_hidden+="${found}"
	done
    done

    echo -n $(basename "${libfile}")
    if [[ ! -z ${deps_basenames[*]} ]]; then
	echo -n " <= ${deps_basenames[*]}"
    fi
    echo
    mkdir -p $(dirname "${destpath}/libs/${platform}/${variant}/${libfile}")
    outfile="${destpath}/libs/${platform}/${variant}/${libfile}"
    rm -f "${outfile}"
    ${PYTHON} "${origpath}"/scripts/merge_libraries.py ${merge_libraries_params[*]} --output="${outfile}" --scan_libs="${allfiles}" --hide_c_symbols="${deps_hidden}" ${libfile} ${deps[*]}
done
cd "${origpath}"
