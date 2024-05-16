#!/bin/env bash

echo "inline IcaoAircraft ICAO_AIRCRAFT[] = {"

n=0

while read -a line ; do
	if [[ ${line[0]:0:1} != ";" ]] && [[ ${line[1]} != "----" ]] && [[ ${line[1]:2:1} =~ [0-9] ]] ; then
		echo "{\"${line[0]}\", '${line[1]:0:1}', '${line[1]:1:1}', ${line[1]:2:1}, '${line[1]:3:1}'},"
		((n++))
	fi
done

echo "{}";
echo "};"

echo "#define ICAO_AIRCRAFT_LEN  ${n}"
echo "#define ICAO_AIRCRAFT_LAST (ICAO_AIRCRAFT + ${n})"
