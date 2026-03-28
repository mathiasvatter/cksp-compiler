gh issue list \
    -s all \
    -L 500 \
    --json number \
    -R mathiasvatter/cksp-compiler-issues | \
    jq -r '.[] | .number' | \
    while read issue; do
        gh issue transfer "$issue" mathiasvatter/cksp-compiler -R mathiasvatter/cksp-compiler-issues
        sleep 3
    done