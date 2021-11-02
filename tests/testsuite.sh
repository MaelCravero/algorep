check_diff()
{
    cut -d " " -f 6- entries_server1.log > 1.log
    for i in $(seq "$1"); do
        cut -d " " -f 6- entries_server"$i".log > "$i".log

        diff -q 1.log "$i".log
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
