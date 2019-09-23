#!/bin/bash

set -eo pipefail

generate_cert_and_key() {
    local path=$1
    # "ec" or "rsa"
    local type=${2}
    local is_ca=${3:-"false"}
    local CN=$4
    local ca_path=$5
    local ca_name=${6:-ca}

    mkdir -p ${path}

    if [[ "${type}" == "rsa" ]]; then
        openssl genrsa -out "${path}/${CN}-key.pem"
    elif [[ "${type}" == "ec" ]]; then
        openssl ecparam -genkey -param_enc named_curve -name prime256v1 -out "${path}/${CN}-key.pem"
    else
        echo "Error: usage: type (param \$2) should be 'rsa' or 'ec'" >&2 && exit 1
    fi
    echo "generated ${path}/${CN}-key.pem"

    if [[ "${is_ca}" == "true" ]]; then
        openssl req  -new -x509 -sha256 -days 3650 \
            -reqexts v3_req -extensions v3_ca \
            -subj "/O=machinezone/O=IXWebSocket/CN=${CN}" \
            -key "${path}/${CN}-key.pem" \
            -out "${path}/${CN}-crt.pem"

    else
        openssl req -new -sha256 \
            -key "${path}/${CN}-key.pem" \
            -subj "/O=machinezone/O=IXWebSocket/CN=${CN}" \
            -out "${path}/${CN}.csr"
    

        if [ -z "${ca_path}" ]; then
            # self-signed
            openssl x509 -req -in "${path}/${CN}.csr" \
                -signkey "${path}/${CN}-key.pem" -days 365 -sha256 \
                -extfile <(printf "subjectAltName=DNS:localhost,IP:127.0.0.1") \
                -outform PEM -out "${path}/${CN}-crt.pem"
        
        else
            openssl x509 -req -in ${path}/${CN}.csr \
                -CA "${ca_path}/${ca_name}-crt.pem" \
                -CAkey "${ca_path}/${ca_name}-key.pem" \
                -CAcreateserial -days 365 -sha256 \
                -extfile <(printf "subjectAltName=DNS:localhost,IP:127.0.0.1") \
                -outform PEM -out "${path}/${CN}-crt.pem"
        fi
    fi

    rm -f ${path}/${CN}.csr
    echo "generated ${path}/${CN}-crt.pem"
}

# main

outdir=${1:-"./.certs"}
type=${2:-"rsa"}

if ! which openssl &>/dev/null; then
    
    if ! grep -qa -E 'docker|lxc' /proc/1/cgroup; then
        # launch a container with openssl and run this script there
        docker run --rm -i -v $(pwd):/work alpine sh -c "apk add bash openssl && /work/generate_certs.sh /work/${outdir} && chown -R $(id -u):$(id -u) /work/${outdir}"
    else
        echo "Please install openssl in this container to generate test certs, or launch outside of docker" >&2 && exit 1
    fi
else
    
    generate_cert_and_key "${outdir}" "${type}" "true" "trusted-ca"
    generate_cert_and_key "${outdir}" "${type}" "false" "trusted-server" "${outdir}" "trusted-ca"
    generate_cert_and_key "${outdir}" "${type}" "false" "trusted-client" "${outdir}" "trusted-ca"

    generate_cert_and_key "${outdir}" "${type}" "true" "untrusted-ca"
    generate_cert_and_key "${outdir}" "${type}" "false" "untrusted-client" "${outdir}" "untrusted-ca"
    generate_cert_and_key "${outdir}" "${type}" "false" "selfsigned-client"
fi
