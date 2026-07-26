#!/bin/bash

SYSCONFIG_TOOL=
SDK_INSTALL_DIR=
SYSCFG_FILE=$1

if [ -z "$SYSCFG_FILE" ]; then
    echo "Usage: $0 <syscfg file path>"
    exit -1
fi
if [ ! -f "$SYSCFG_FILE" ]; then
    echo "Error: '$SYSCFG_FILE' is not a file."
    exit -1
fi

search_dirs=("/c/ti" "/d/ti" "/e/ti" "/f/ti")
for dirpath in ${search_dirs[@]}
do
    if [ -d "$dirpath" ]; then
        echo "Searching $dirpath ..."
        # search syscfg tools
        if [ -z "$SYSCONFIG_DIR" ]; then
            SYSCONFIG_DIR=`find "$dirpath" -maxdepth 2 -type d -name "sysconfig_*"`
            if [ ! -z "$SYSCONFIG_DIR" ]; then
                echo " - Found syscfg tools: $SYSCONFIG_DIR"
                SYSCONFIG_TOOL="$SYSCONFIG_DIR/nodejs/node $SYSCONFIG_DIR/dist/cli.js"
            fi
        fi
        # search sdk
        if [ -z "$SDK_INSTALL_DIR" ]; then
            SDK_INSTALL_DIR=`find "$dirpath" -maxdepth 2 -type d -name "mspm0_sdk_2_*"`
            if [ ! -z "$SDK_INSTALL_DIR" ]; then
                echo " - Found ti sdk: $SDK_INSTALL_DIR"
            fi
        fi
    fi
done

if [ -z "$SYSCONFIG_TOOL" ]; then
    echo "Not found ti sysconfig tools."
    exit -1
fi
if [ -z "$SDK_INSTALL_DIR" ]; then
    echo "Not found ti mspm0 sdk."
    exit -1
fi

OUT_DIR=./source
echo "Generating to $OUT_DIR ..."
mkdir -p $OUT_DIR
SYSCFG_CMD_STUB="$SYSCONFIG_TOOL --compiler gcc --product $SDK_INSTALL_DIR/.metadata/product.json"
$SYSCFG_CMD_STUB --listGeneratedFiles --listReferencedFiles --output $OUT_DIR $SYSCFG_FILE
$SYSCFG_CMD_STUB --output $OUT_DIR $SYSCFG_FILE
