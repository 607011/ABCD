#!/bin/bash
source .env
rsync -av --progress --files-from=files-to-copy.txt . ${DEST} 
