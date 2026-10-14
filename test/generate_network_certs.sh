#!/bin/bash
# Test-only credentials, generated outside the source tree. No keychain changes.
set -euo pipefail
openssl_bin=$1
cert_dir=$2
mkdir -p "$cert_dir"
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -sha256 -days 3650 \
    -subj /CN=IXWebSocket-Test-CA -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign,cRLSign -keyout "$cert_dir/ca.key" -out "$cert_dir/ca.pem" >/dev/null 2>&1
for name in server wrong client; do
    "$openssl_bin" req -new -newkey rsa:2048 -nodes -sha256 -subj "/CN=$name" \
        -keyout "$cert_dir/$name.key" -out "$cert_dir/$name.csr" >/dev/null 2>&1
    days=365
    if [[ "$name" == client ]]; then
        printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=clientAuth\n' > "$cert_dir/extensions"
    else
        san='DNS:localhost,IP:127.0.0.1,IP:::1'
        if [[ "$name" == wrong ]]; then san='DNS:wrong.invalid'; fi
        printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=%s\n' "$san" > "$cert_dir/extensions"
    fi
    "$openssl_bin" x509 -req -sha256 -days "$days" -in "$cert_dir/$name.csr" \
        -CA "$cert_dir/ca.pem" -CAkey "$cert_dir/ca.key" -CAcreateserial \
        -extfile "$cert_dir/extensions" -out "$cert_dir/$name.pem" >/dev/null 2>&1
done
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -sha256 -days 3650 \
    -subj /CN=Untrusted-Client -keyout "$cert_dir/untrusted.key" -out "$cert_dir/untrusted.pem" >/dev/null 2>&1
# A chain with an intermediate verifies that certFile sends the full chain.
"$openssl_bin" req -new -newkey rsa:2048 -nodes -sha256 -subj /CN=Intermediate \
    -keyout "$cert_dir/intermediate.key" -out "$cert_dir/intermediate.csr" >/dev/null 2>&1
printf 'basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\n' > "$cert_dir/extensions"
"$openssl_bin" x509 -req -sha256 -days 365 -in "$cert_dir/intermediate.csr" \
    -CA "$cert_dir/ca.pem" -CAkey "$cert_dir/ca.key" -CAcreateserial \
    -extfile "$cert_dir/extensions" -out "$cert_dir/intermediate.pem" >/dev/null 2>&1
printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost,IP:127.0.0.1\n' > "$cert_dir/extensions"
"$openssl_bin" x509 -req -sha256 -days 365 -in "$cert_dir/server.csr" \
    -CA "$cert_dir/intermediate.pem" -CAkey "$cert_dir/intermediate.key" -CAcreateserial \
    -extfile "$cert_dir/extensions" -out "$cert_dir/chain.pem" >/dev/null 2>&1
cat "$cert_dir/intermediate.pem" >> "$cert_dir/chain.pem"
cp "$cert_dir/server.key" "$cert_dir/chain.key"
# Use the checked-in credential whose validity ended in 2021 for the expiry case.
fixture_dir="$(cd "$(dirname "$0")" && pwd)/.certs"
cp "$fixture_dir/trusted-server-crt.pem" "$cert_dir/expired.pem"
cp "$fixture_dir/trusted-server-key.pem" "$cert_dir/expired.key"
cp "$fixture_dir/trusted-ca-crt.pem" "$cert_dir/expired-ca.pem"
touch "$cert_dir/ready"
