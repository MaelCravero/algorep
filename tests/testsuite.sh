check_diff()
{
    for i in $(seq "$1"); do
        diff -q entries_server1.log entries_server"$i".log
        if [ $? -ne 0 ] ; then
            return 1;
        fi
    done

    return 0
}

run_test()
{
    echo "testing $1"

    # Sorry
    cat "$1" | make run > /dev/null

    check_diff 5

    if [ $? -ne 0 ] ; then
        echo "FAILED"
    else
        echo "SUCCESS"
    fi
}

for f in $(echo tests/*.txt); do
    run_test "$f"
done
