#!/bin/bash
# report-failure.sh TITLE COMMAND...: runs COMMAND with its output shown as
# usual; if it fails, the last 40 lines also go into an error annotation,
# which (unlike the job log) anyone can read without signing in.
title=$1; shift
log=$(mktemp)
"$@" 2>&1 | tee "$log"
code=${PIPESTATUS[0]}
if [ "$code" -ne 0 ] && [ "$code" -ne 77 ]; then
	tail -40 "$log" | sed 's/%/%25/g; s/\r//g' | awk -v t="$title" -v c="$code" \
		'{ s = s "%0A" $0 } END { printf "::error title=%s (exit %s)::%s\n", t, c, s }'
fi
rm -f "$log"
exit "$code"
