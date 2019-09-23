#!/bin/bash

set -eo pipefail

generate_cert_and_key() {
    local path=$1
    local CN=$2
    local ca_path=$3
    local ca_name=${4:-ca}

    mkdir -p ${path}

    openssl genrsa -out "${path}/${CN}-key.pem" 2048 &>/dev/null
    echo "generated ${path}/${CN}-key.pem"

    openssl req -new -sha256 \
        -key "${path}/${CN}-key.pem" \
        -subj "/O=machinezone/O=IXWebSocket/CN=${CN}" \
        -reqexts SAN \
        -config <(cat /etc/ssl/openssl.cnf \
            <(printf "\n[SAN]\nsubjectAltName=DNS:localhost,DNS:127.0.0.1")) \
        -out "${path}/${CN}.csr" &>/dev/null
    
    if [ -z "${ca_path}" ]; then
        # self-signed
        openssl x509 -req -in "${path}/${CN}.csr" \
            -signkey "${path}/${CN}-key.pem" -days 365 -sha256 \
            -outform PEM -out "${path}/${CN}-crt.pem" &>/dev/null
    
    else
        openssl x509 -req -in ${path}/${CN}.csr \
            -CA "${ca_path}/${ca_name}-crt.pem" \
            -CAkey "${ca_path}/${ca_name}-key.pem" \
            -CAcreateserial -days 365 -sha256 \
            -outform PEM -out "${path}/${CN}-crt.pem" &>/dev/null
    fi

    rm -f ${path}/${CN}.csr
    echo "generated ${path}/${CN}-crt.pem"
}

# main

outdir=${1:-"./.certs"}

if ! which openssl &>/dev/null; then
    
    if ! grep -qa -E 'docker|lxc' /proc/1/cgroup; then
        # launch a container with openssl and run this script there
        docker run --rm -i -v $(pwd):/work alpine sh -c "apk add bash openssl && /work/generate_certs.sh /work/${outdir} && chown -R $(id -u):$(id -u) /work/${outdir}"
    else
        echo "Please install openssl in this container to generate test certs, or launch outside of docker" >&2 && exit 1
    fi
else
    
    generate_cert_and_key "${outdir}" "trusted-ca"
    generate_cert_and_key "${outdir}" "trusted-server" "${outdir}" "trusted-ca"
    generate_cert_and_key "${outdir}" "trusted-client" "${outdir}" "trusted-ca"

    generate_cert_and_key "${outdir}" "untrusted-ca"
    generate_cert_and_key "${outdir}" "untrusted-client" "${outdir}" "untrusted-ca"
    generate_cert_and_key "${outdir}" "selfsigned-client"
fi
