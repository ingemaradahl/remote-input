#!/usr/bin/env bash

IWYU="include-what-you-use"
IWYU_EXIT_SUCCESS=2

echoerr() {
    echo -e "$*" >&2
}

fail() {
    echoerr "$(basename $0): $*"
    exit 1
}

emit_iwyu_mapping() {
    cat <<EOF
[
    { include: [
        "<bits/socket_type.h>", private,
        "<sys/socket.h>", public ]
    }, { include: [
        "<X11/keysymdef.h>", "private",
        "<X11/keysym.h>", "public"]
    }, { include: [
        "<linux/input-event-codes.h>", "private",
        "<linux/input.h>", "public"
        ]
    }
]
EOF
}

command -v "$IWYU" > /dev/null || fail "\"$IWYU\" not found"

exit_code=0
for f in *.c test/*.c; do
    $IWYU -Xiwyu --mapping_file=<(emit_iwyu_mapping) "$f"
    if [ $? -ne $IWYU_EXIT_SUCCESS ]; then
        exit_code=1
    fi
done

exit $exit_code
