#!/bin/bash -e

#set -x

# Path to sign-gxl-boot util
TOOL_PATH=$(dirname $(readlink -f $0))

SCRIPT_PATH=$(dirname $(readlink -f $0))
# Temporary files directory
if [ -z "$TMP" ]; then
    TMP=${SCRIPT_PATH}/tmp
fi

usage() {
    cat << EOF
    Usage:
        $(basename $0) --help

        Combine binaries into an unsigned bootloader:
            $(basename $0) --create-unsigned-bl
                --bl2 <bl2_new.bin> --bl30 <bl30_new.bin>
                --bl31 <bl31.img> --bl32 <bl32.img>
                --bl33 <bl33.bin> -o <bl.bin>

        Create a root hash for OTP:
            $(basename $0) --create-root-hash \
           --root-key-0 work/root.pem --root-key-1 work/root.pem \
           --root-key-2 work/root.pem --root-key-3 work/root.pem \
           -o work/rootkeys-hash.bin

    Environment Variables:
      TMP:          path to a temporary directory. Defaults to <this script's path>/tmp
EOF
    exit 1
}

check_file() {
    if [ ! -f "$2" ]; then echo Error: Unable to open $1: \""$2"\"; exit 1 ; fi
}

# Hash root/bl2 RSA keys'.  sha256(n[keylen] + e)
# $1: key hash version
# $2: precomputed binary key file
# $3: keylen
# $4: output hash file
hash_rsa_bin() {
    local keyhashver=$1
    local key=$2
    local keylen=$3
    local output=$4
    if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] || [ -z "$4" ]; then
        echo "Argument error"
        exit 1
    fi
    local insize=$(wc -c < $key)
    if [ $insize -ne 1036 ]; then
        echo "Keyfile wrong size"
        exit 1
    fi
    if [ $keylen -ne 1024 ] && [ $keylen -ne 2048 ] && [ $keylen -ne 4096 ]; then
        echo "Invalid keylen"
        exit 1
    fi
    local keylenbytes=$(( $keylen / 8 ))

    if [ $keyhashver -eq 2 ]; then
        cp $key $TMP/keydata
    else
        # modulus - rsa_public_key_t.n[key length]
        dd if=$key of=$TMP/keydata bs=$keylenbytes count=1 >& /dev/null
        # exponent - rsa_public_key_t.e
        dd if=$key of=$TMP/keydata bs=1 skip=512 count=4 \
            oflag=append conv=notrunc >& /dev/null
    fi
    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $TMP/keydata > $output
    else
        openssl sha -sha256 -binary $TMP/keydata > $output
    fi
}

# --pack-bl2  -i bl2.bin  -o bl2.bin.img
pack_bl2() {
    local input=""
    local output=""
    local argv=("$@")
    local i=0

    # Parse args
    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        case "$arg" in
            -i)
                i=$((i + 1))
                input="${argv[$i]}"
                ;;
            -o)
                i=$((i + 1))
                output="${argv[$i]}"
                ;;
            *)
                echo "Unknown option $arg"; exit 1
                ;;
        esac
        i=$((i + 1))
    done
    # Verify args
    check_file bl2  "$input"
    if [ -z "$output" ]; then echo Error: Missing output file option -o; exit 1; fi

    # Add header
    ${TOOL_PATH}/sign-boot-gxl --add-aml-block-header \
            -i $input \
            -o $TMP/bl2.img-noiv

    # Add nonce
    dd if=/dev/urandom of=$TMP/nonce.bin bs=16 count=1 >& /dev/null
    cat $TMP/nonce.bin $TMP/bl2.img-noiv > $TMP/bl2.img

    # Truncate to correct length
    # TODO should check that end of output is all zeroes before truncating
    truncate -s 49152 $TMP/bl2.img

    # Add sha256 hash into header at byte offset 80
    dd if=$TMP/bl2.img of=$TMP/chkdata bs=1 skip=16 count=64 >& /dev/null
    dd if=$TMP/bl2.img of=$TMP/chkdata bs=1 skip=112 \
        oflag=append conv=notrunc  >& /dev/null
    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $TMP/chkdata > $TMP/bl2.sha
    else
        openssl sha -sha256 -binary $TMP/chkdata > $TMP/bl2.sha
    fi
    dd if=$TMP/bl2.sha of=$TMP/bl2.img bs=1 seek=80 \
        conv=notrunc  >& /dev/null

    cp $TMP/bl2.img $output
}

create_root_hash() {
    local rootkey0=""
    local rootkey1=""
    local rootkey2=""
    local rootkey3=""
    local output=""
    local sigver=""
    local keyhashver=""
    local argv=("$@")
    local i=0

    # Parse args
    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        i=$((i + 1))
        case "$arg" in
            --root-key-0)
                rootkey0="${argv[$i]}" ;;
            --root-key-1)
                rootkey1="${argv[$i]}" ;;
            --root-key-2)
                rootkey2="${argv[$i]}" ;;
            --root-key-3)
                rootkey3="${argv[$i]}" ;;
            --sig-ver)
                sigver="${argv[$i]}" ;;
            --key-hash-ver)
                keyhashver="${argv[$i]}" ;;
            -o)
                output="${argv[$i]}" ;;
            *)
                echo "Unknown option $arg"; exit 1
                ;;
        esac
        i=$((i + 1))
    done
    # Verify args
    check_file rootkey0 "$rootkey0"
    check_file rootkey1 "$rootkey1"
    check_file rootkey2 "$rootkey2"
    check_file rootkey3 "$rootkey3"
    if [ -z "$output" ]; then echo Error: Missing output file option -o; exit 1; fi
    if [ -z "$sigver" ]; then
        sigver=1
    fi
    if [ -z "$keyhashver" ]; then
        keyhashver=1
    fi

    local keylenbytes=$(get_pem_key_len $rootkey0)
    local keylen=$(( $keylenbytes * 8 ))

    # Convert PEM key to rsa_public_key_t (precomputed RSA public key)
    pem_to_bin $rootkey0 $TMP/rootkey0.bin
    pem_to_bin $rootkey1 $TMP/rootkey1.bin
    pem_to_bin $rootkey2 $TMP/rootkey2.bin
    pem_to_bin $rootkey3 $TMP/rootkey3.bin

    # hash of keys
    hash_rsa_bin $keyhashver $TMP/rootkey0.bin $keylen $TMP/rootkey0.sha
    hash_rsa_bin $keyhashver $TMP/rootkey1.bin $keylen $TMP/rootkey1.sha
    hash_rsa_bin $keyhashver $TMP/rootkey2.bin $keylen $TMP/rootkey2.sha
    hash_rsa_bin $keyhashver $TMP/rootkey3.bin $keylen $TMP/rootkey3.sha

    cat $TMP/rootkey0.sha $TMP/rootkey1.sha $TMP/rootkey2.sha $TMP/rootkey3.sha > $TMP/rootkeys.sha
    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $TMP/rootkeys.sha > $output
    else
        openssl sha -sha256 -binary $TMP/rootkeys.sha > $output
    fi
}


# Get key len in bytes of private PEM RSA key
# $1: PEM file
get_pem_key_len() {
    local pem=$1
    local bits=0
    if [ ! -f "$1" ]; then
        echo "Argument error, \"$1\""
        exit 1
    fi
    bits=$(openssl rsa -in $pem -text -noout | \
        grep '^Private-Key: (' | \
        sed 's/Private-Key: (//' | \
        sed 's/ bit)//')
    if [ "$bits" -ne 1024 ] && [ "$bits" -ne 2048 ] &&
       [ "$bits" -ne 4096 ] && [ "$bits" -ne 8192]; then
       echo "Unexpected key size  $bits"
       exit 1
    fi
    echo $(( $bits / 8 ))
}


# Pad file to len by adding 0's to end of file
# $1: file
# $2: len
pad_file() {
    local file=$1
    local len=$2
    if [ ! -f "$1" ] || [ -z "$2" ]; then
        echo "Argument error, \"$1\", \"$2\" "
        exit 1
    fi
    local filesize=$(wc -c < ${file})
    local padlen=$(( $len - $filesize ))
    if [ $len -lt $filesize ]; then
        echo "File larger than expected.  $filesize, $len"
        exit 1
    fi
    dd if=/dev/zero of=$file oflag=append conv=notrunc bs=1 \
        count=$padlen >& /dev/null
}

# add header to bl3x
pack_bl3x() {
    local input=""
    local output=""
    local arb_cvn=""
    local argv=("$@")
    local i=0

    # Parse args
    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        i=$((i + 1))
        case "$arg" in
            -i)
                input="${argv[$i]}" ;;
            -o)
                output="${argv[$i]}" ;;
            -v)
                arb_cvn="${argv[$i]}" ;;
            *)
                echo "Unknown option $arg"; exit 1
                ;;
        esac
        i=$((i + 1))
    done
    # Verify args
    check_file "input" "$input"
    if [ -z "$output" ]; then echo Error: Missing output file option -o; exit 1; fi
    if [ -z "$arb_cvn" ]; then
        arb_cvn="0"
    fi

    # Check padding
    local imagesize=$(wc -c < ${input})
    local rem=$(( $imagesize % 16 ))
    if [ $rem -ne 0 ]; then
        #echo "Input $input not 16 byte aligned?"
        local topad=$(( 16 - $rem ))
        imagesize=$(( $imagesize + $topad ))
        cp $input $TMP/blpad.bin
        pad_file $TMP/blpad.bin $imagesize
        input=$TMP/blpad.bin
    fi

    # Hash payload
    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $input > $TMP/bl-pl.sha
    else
        openssl sha -sha256 -binary $input > $TMP/bl-pl.sha
    fi
    # Add hash to header
    ${TOOL_PATH}/sign-boot-gxl --create-sbImage-header \
            --hash $TMP/bl-pl.sha \
            --arb-cvn $arb_cvn \
            -o $TMP/bl.hdr

    # Pad header
    # pad to (sizeof(sbImageHeader_t) - nonce[16] - checksum[32])
    pad_file $TMP/bl.hdr $(( 656 - 16 - 32))

    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $TMP/bl.hdr > $TMP/bl.hdr.sha
    else
        openssl sha -sha256 -binary $TMP/bl.hdr > $TMP/bl.hdr.sha
    fi
    # Create nonce
    dd if=/dev/urandom of=$TMP/nonce.bin bs=16 count=1 >& /dev/null

    # Combine nonce + hdr + sha + payload
    cat $TMP/nonce.bin $TMP/bl.hdr $TMP/bl.hdr.sha $input > $TMP/bl.bin

    cp $TMP/bl.bin $output
}

# Convert RSA private PEM key to precomputed binary key file
# If input is already the precomputed binary key file, then it is simply copied
# to the output
# $1: input RSA private .PEM
# $2: output precomputed binary key file
pem_to_bin() {
    local input=$1
    local output=$2
    if [ ! -f "$1" ] || [ -z "$2" ]; then
        echo "Argument error, \"$1\", \"$2\" "
        exit 1
    fi

    local insize=$(wc -c < $input)
    if [ $insize -eq 1036 ]; then
        # input is already precomputed binary key file
        cp $input $output
    fi

    local pycmd="import sys; \
                 sys.path.append(\"${TOOL_PATH}\"); \
                 import pem_extract_pubkey; \
                 sys.stdout.write(pem_extract_pubkey.extract_pubkey( \
                    \"$input\", headerMode=False));"
    /usr/bin/env python2.7 -c "$pycmd" > $output
}

# Create FIP
create_fip_unsigned() {
    local bl30=""
    local bl31=""
    local bl32=""
    local bl31_info=""
    local bl32_info=""
    local bl33=""
    local kernel=""
    local output=""
    local argv=("$@")
    local i=0

    # Parse args
    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        i=$((i + 1))
        case "$arg" in
            --bl30)
                bl30="${argv[$i]}" ;;
            --bl31)
                bl31="${argv[$i]}" ;;
            --bl32)
                bl32="${argv[$i]}" ;;
            --bl31-info)
                bl31_info="${argv[$i]}" ;;
            --bl32-info)
                bl32_info="${argv[$i]}" ;;
            --bl33)
                bl33="${argv[$i]}" ;;
            --kernel)
                kernel="${argv[$i]}" ;;
            -o)
                output="${argv[$i]}" ;;
            *)
                echo "Unknown option $arg"; exit 1 ;;
        esac
        i=$((i + 1))
    done
    # Verify args
    check_file bl30 "$bl30"
    check_file bl31 "$bl31"
    check_file bl32 "$bl32"
    check_file bl31_info "$bl31_info"
    check_file bl32_info "$bl32_info"
    check_file bl33 "$bl33"
    check_file kernel "$kernel"

    if [ -z "$output" ]; then echo Error: Missing output file option -o ; exit 1; fi

    #dummy rsa/key/iv because utility can't handle unsigned yet
    dd if=/dev/zero of=$TMP/zerorsakey bs=1 count=1036 >& /dev/null
    dd if=/dev/zero of=$TMP/zeroaeskey bs=1 count=32 >& /dev/null
    dd if=/dev/zero of=$TMP/zeroaesiv bs=1 count=16 >& /dev/null

    # Create header and add keys
    local argv=("$@")
    ${TOOL_PATH}/sign-boot-gxl --create-fip-header \
        --bl30 $bl30 --bl30-key $TMP/zerorsakey \
        --bl30-aes-key $TMP/zeroaeskey --bl30-aes-iv $TMP/zeroaesiv \
        --bl31 $bl31 --bl31-key $TMP/zerorsakey \
        --bl31-aes-key $TMP/zeroaeskey --bl31-aes-iv $TMP/zeroaesiv \
        --bl32 $bl32 --bl32-key $TMP/zerorsakey  \
        --bl32-aes-key $TMP/zeroaeskey --bl32-aes-iv $TMP/zeroaesiv \
        --bl33 $bl33 --bl33-key $TMP/zerorsakey  \
        --bl33-aes-key $TMP/zeroaeskey --bl33-aes-iv $TMP/zeroaesiv \
        --kernel $kernel --kernel-key $TMP/zerorsakey    \
        --kernel-aes-key $TMP/zeroaeskey --kernel-aes-iv $TMP/zeroaesiv \
        --bl31-info $bl31_info --bl32-info $bl32_info \
        -o $TMP/fip.hdr

    # Pad header to size
    # pad to (sizeof(fip hdr) - nonce[16] - checksum[32])
    pad_file $TMP/fip.hdr $(( (16*1024) - 16 - 32))

    if `openssl version -v | grep -E "1.1" >/dev/null`; then
        openssl sha256 -binary $TMP/fip.hdr > $TMP/fip.hdr.sha
    else
        openssl sha -sha256 -binary $TMP/fip.hdr > $TMP/fip.hdr.sha
    fi
    # Create nonce
    dd if=/dev/urandom of=$TMP/nonce.bin bs=16 count=1 >& /dev/null

    # Combine nonce + hdr + sha
    cat $TMP/nonce.bin $TMP/fip.hdr $TMP/fip.hdr.sha > $TMP/fip.bin

    cp $TMP/fip.bin $output
}

# Check (bl31/bl32) input is .img
# 1: input
# returns True or False
is_img_format() {
    local input=$1
    if [ ! -f "$1" ]; then
        echo "Argument error, \"$1\""
        exit 1
    fi
    local insize=$(wc -c < $input)
    if [ $insize -le 512 ]; then
        # less than size of img header
        echo False
        return
    fi

    local inmagic=$(xxd -p -l 4 $input)
    if [ "$inmagic" == "65873412" ]; then
        # Input has FIP_TOC_ENTRY_EXT_FLAG, so it is in .img format.
        # Strip off 0x200 byte header.
        echo True
    else
        echo False
    fi
}

# Convert from .img to .bin.  If not .img format, copy to output.
# 1: input
# 2: output
convert_img_to_bin() {
    local input=$1
    local output=$2
    if [ ! -f "$1" ] || [ -z "$2" ]; then
        echo "Argument error, \"$1\", \"$2\" "
        exit 1
    fi
    local insize=$(wc -c < $input)
    if [ $insize -le 512 ]; then
        # less than size of img header
        cp $input $output
        return
    fi

    local inmagic=$(xxd -p -l 4 $input)
    if [ "$inmagic" == "65873412" ]; then
        # Input has FIP_TOC_ENTRY_EXT_FLAG, so it is in .img format.
        # Strip off 0x200 byte header.
        tail -c +513 $input > $output
    else
        cp $input $output
    fi
}


# input bl2, bl30/31/32/33kernel .bin
create_unsigned_bl() {
    local bl2=""
    local bl30=""
    local bl31=""
    local bl32=""
    local bl33=""
    local kernel=""
    local output=""
    local argv=("$@")
    local i=0

    # Parse args
    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        i=$((i + 1))
        case "$arg" in
            --bl2)
                bl2="${argv[$i]}" ;;
            --bl30)
                bl30="${argv[$i]}" ;;
            --bl31)
                bl31="${argv[$i]}" ;;
            --bl32)
                bl32="${argv[$i]}" ;;
            --bl33)
                bl33="${argv[$i]}" ;;
            --kernel)
                kernel="${argv[$i]}" ;;
            -o)
                output="${argv[$i]}" ;;
            *)
                echo "Unknown option $arg"; exit 1 ;;
        esac
        i=$((i + 1))
    done
    # Verify args
    check_file bl2 "$bl2"
    check_file bl30 "$bl30"
    check_file bl31 "$bl31"
    check_file bl32 "$bl32"
    check_file bl33 "$bl33"
    if [ ! -z "$kernel" ]; then
        check_file kernel "$kernel"
    else
        echo > $TMP/kernel
        kernel=$TMP/kernel
    fi

    if [ -z "$output" ]; then echo Error: Missing output file option -o ; exit 1; fi

    if [ "$(is_img_format $bl31)" != "True" ]; then
        echo Error. Expected .img format for \"$bl31\"
        exit 1
    fi
    if [ "$(is_img_format $bl32)" != "True" ]; then
        echo Error. Expected .img format for \"$bl32\"
        exit 1
    fi

    convert_img_to_bin $bl31 $TMP/bl31.bin
    convert_img_to_bin $bl32 $TMP/bl32.bin

    pack_bl2 -i $bl2 -o $TMP/bl2.bin.img

    pack_bl3x -i $bl30 -o $TMP/bl30.bin.img
    pack_bl3x -i $TMP/bl31.bin -o $TMP/bl31.bin.img
    pack_bl3x -i $TMP/bl32.bin -o $TMP/bl32.bin.img
    pack_bl3x -i $bl33 -o $TMP/bl33.bin.img

    create_fip_unsigned \
        --bl30     $TMP/bl30.bin.img \
        --bl31     $TMP/bl31.bin.img \
        --bl32     $TMP/bl32.bin.img \
        --bl31-info     $bl31 \
        --bl32-info     $bl32 \
        --bl33     $TMP/bl33.bin.img \
        --kernel $kernel \
        -o $TMP/fip.hdr.out

    cat $TMP/bl2.bin.img $TMP/fip.hdr.out $TMP/bl30.bin.img $TMP/bl31.bin.img \
        $TMP/bl32.bin.img $TMP/bl33.bin.img > $output

    local filesize=$(wc -c < $output)
    local limit_len=$((2*1024*1024))
    if [ $filesize -gt $limit_len ]; then
        echo "uboot size larger than expected. $filesize  $limit_len"
        exit 1
        return
    fi
    echo
    echo Created unsigned bootloader $output successfully
}

parse_main() {
    local i=0
    local argv=()
    for arg in "$@" ; do
        argv[$i]="$arg"
        i=$((i + 1))
    done

    i=0
    while [ $i -lt $# ]; do
        arg="${argv[$i]}"
        case "$arg" in
            -h|--help)
                usage
                break ;;
            --create-unsigned-bl)
                create_unsigned_bl "${argv[@]:$((i + 1))}"
                break ;;
            --create-root-hash)
                create_root_hash "${argv[@]:$((i + 1))}"
                break ;;
            *)
                echo "Unknown first option $1"; exit 1
                ;;
        esac
        i=$((i + 1))
    done
}

cleanup() {
    if [ ! -d "$TMP" ]; then return; fi
    local tmpfiles="bl2.bin.img bl2.img bl2.img-noiv bl2.sha
    bl30.bin.img bl31.bin bl31.bin.img bl32.bin bl32.bin.img
    bl33.bin.img bl.bin bl.hdr bl.hdr.sha blpad.bin
    bl-pl.sha chkdata fip.bin fip.hdr fip.hdr.out
    fip.hdr.sha kernel nonce.bin zeroaesiv zeroaeskey zerorsakey"
    for i in $tmpfiles ; do
        rm -f $TMP/$i
    done
    rmdir $TMP || true
}

trap cleanup EXIT

cleanup
if [ ! -d "$TMP" ]; then mkdir "$TMP" ; fi
parse_main "$@"
