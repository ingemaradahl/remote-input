#!/usr/bin/awk -f

BEGIN {
    first_entry = -1;

    print "/* THIS FILE IS GENERATED. DO NOT EDIT! */\n"
    print "#include <stdint.h>\n"
    print "uint16_t lookup_keycode(uint16_t keycode) {"
}

/^\s*[0-9]+\s+[0-9]+\s*$/ {
    if (first_entry == -1) {
        print "    static const uint16_t keymap[] = {"
        printf("          %d\n", $2);
        first_entry = $1;
        previous_entry = first_entry;
        next;
    }

    if (previous_entry + 1 == $1) {
        printf("        , %d\n", $2);
        previous_entry = $1;
        next;
    }

    for (i = previous_entry + 1; i < $1; i++) {
        printf("        , %d\n", i);
    }

    printf("        , %d\n", $2);

    previous_entry = $1;
}

# Fail for lines not matching the above or empty pattern
!/^\s*[0-9]+\s+[0-9]+\s*$|^\s*$/ {
    printf("Bad pattern: '%s'\n", $0) > "/dev/stderr";
    exit(1);
}

END {
    if (first_entry == -1) {
        print "    return keycode;"
        print "}"
        exit 0;
    }

    print "    };"
    printf("    static const size_t keymap_start = %d;\n", first_entry);
    printf("    static const size_t keymap_end = %d +\n", first_entry);
    print "            sizeof(keymap) / sizeof(keymap[0]);\n"
    print "    if (keycode >= keymap_start && keycode < keymap_end) {"
    print "        return keymap[keycode - keymap_start];"
    print "    }\n"
    print "    return keycode;"
    print "}"
}
