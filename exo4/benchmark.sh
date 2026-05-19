#!/bin/bash
export LC_ALL=C
make clean
make

echo "Valgrind check on pb1 (Sequential):"
valgrind --leak-check=full ./bin/knapsack data/pb1.txt 0

echo "Valgrind check on pb1 (Parallel):"
valgrind --leak-check=full ./bin/knapsack data/pb1.txt 2

PROBLEMS=("data/pb1.txt" "data/pb2.txt" "data/pb6.txt")
THREADS=(0 2 4 8 16)
RUNS=5

# Generate HTML
cat <<EOF > report.html
<!DOCTYPE html>
<html>
<head>
    <title>Knapsack Parallelization Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        table { border-collapse: collapse; width: 100%; margin-bottom: 30px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: center; }
        th { background-color: #f2f2f2; }
        h1, h2 { color: #333; }
    </style>
</head>
<body>
    <h1>Knapsack Problem: OpenMP Parallelization Report</h1>
    <p>Measurement over $RUNS runs.</p>
EOF

for pb in "${PROBLEMS[@]}"; do
    echo "Processing $pb..."
    echo "    <h2>Problem: $pb</h2>" >> report.html
    echo "    <table>" >> report.html
    echo "        <tr><th>Threads</th><th>Average Time (s)</th><th>Speedup</th><th>Max Utility</th></tr>" >> report.html

    seq_time=0
    max_util=0

    for t in "${THREADS[@]}"; do
        echo "  Threads: $t"
        total_time=0
        for ((i=1; i<=RUNS; i++)); do
            res=$(./bin/knapsack "$pb" "$t")
            time=$(echo "$res" | cut -d',' -f1)
            util=$(echo "$res" | cut -d',' -f2)
            total_time=$(echo "$total_time + $time" | bc -l)
            max_util=$util
        done
        avg_time=$(echo "$total_time / $RUNS" | bc -l)
        
        speedup="1.00"
        if [ "$t" -eq 0 ]; then
            seq_time=$avg_time
            thread_label="Sequential"
        else
            thread_label="$t"
            if (( $(echo "$seq_time > 0" | bc -l) )); then
                speedup=$(echo "scale=2; $seq_time / $avg_time" | bc -l)
            fi
        fi

        avg_time_fmt=$(printf "%.6f" "$avg_time")
        
        echo "        <tr><td>$thread_label</td><td>$avg_time_fmt</td><td>${speedup}x</td><td>$max_util</td></tr>" >> report.html
    done
    echo "    </table>" >> report.html
done

cat <<EOF >> report.html
</body>
</html>
EOF

echo "Done! HTML report generated in report.html"