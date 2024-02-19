#!/bin/env bash

echo "#define CA_BUNDLE \\"

while read line ; do
	echo "\"${line}\\n\" \\"
done

echo '""'
