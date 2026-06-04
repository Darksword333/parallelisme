#!/bin/bash
export LC_ALL=C

# Compilation
echo "Compilation du programme..."
make clean
make

if [ ! -f ./bin/dijkstra ]; then
    echo "Erreur: la compilation a échoué."
    exit 1
fi

mkdir -p data/output

echo ""
echo "============================================="
echo "1. Vérification de la mémoire avec Valgrind"
echo "============================================="
echo "Lancement de Valgrind sur test.gr (Version Séquentielle Optimisée)..."
valgrind --leak-check=full --show-leak-kinds=all ./bin/dijkstra data/test.gr 1 2

echo ""
echo "Lancement de Valgrind sur test.gr (Version Parallèle Optimisée - 4 Threads)..."
valgrind --leak-check=full --show-leak-kinds=all ./bin/dijkstra data/test.gr 4 3

echo ""
echo "============================================="
echo "2. Lancement des mesures de performances"
echo "============================================="

CSV_FILE="data/output/results.csv"
echo "Graph,Version,Threads,Run,Time" > "$CSV_FILE"

RUNS=5
THREADS=(1 2 4 8 16)

# A. Rome Graph (3353 nodes) - Run all versions
echo "Mesures sur rome99.gr..."
# 1. Naive Seq
echo -n "  Naive Seq: "
for ((r=1; r<=RUNS; r++)); do
    res=$(./bin/dijkstra data/rome99.gr 1 0)
    t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
    echo -n "Run $r ($t_val s)  "
    echo "rome99,NaiveSeq,1,$r,$t_val" >> "$CSV_FILE"
done
echo ""

# 2. Naive Parallel
for t in "${THREADS[@]}"; do
    echo -n "  Naive Omp (threads=$t): "
    for ((r=1; r<=RUNS; r++)); do
        res=$(./bin/dijkstra data/rome99.gr "$t" 1)
        t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
        echo -n "Run $r ($t_val s)  "
        echo "rome99,NaiveOmp,$t,$r,$t_val" >> "$CSV_FILE"
    done
    echo ""
done

# 3. Optimized Seq
echo -n "  Opt Seq: "
for ((r=1; r<=RUNS; r++)); do
    res=$(./bin/dijkstra data/rome99.gr 1 2)
    t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
    echo -n "Run $r ($t_val s)  "
    echo "rome99,OptSeq,1,$r,$t_val" >> "$CSV_FILE"
done
echo ""

# 4. Optimized Parallel
for t in "${THREADS[@]}"; do
    echo -n "  Opt Omp (threads=$t): "
    for ((r=1; r<=RUNS; r++)); do
        res=$(./bin/dijkstra data/rome99.gr "$t" 3)
        t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
        echo -n "Run $r ($t_val s)  "
        echo "rome99,OptOmp,$t,$r,$t_val" >> "$CSV_FILE"
    done
    echo ""
done


# B. New York Graph (264346 nodes) - Run only Optimized versions
echo "Mesures sur new-york.gr..."
# 1. Optimized Seq
echo -n "  Opt Seq: "
for ((r=1; r<=RUNS; r++)); do
    res=$(./bin/dijkstra data/new-york.gr 1 2)
    t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
    echo -n "Run $r ($t_val s)  "
    echo "newyork,OptSeq,1,$r,$t_val" >> "$CSV_FILE"
done
echo ""

# 2. Optimized Parallel
for t in "${THREADS[@]}"; do
    echo -n "  Opt Omp (threads=$t): "
    for ((r=1; r<=RUNS; r++)); do
        res=$(./bin/dijkstra data/new-york.gr "$t" 3)
        t_val=$(echo "$res" | grep -oE "Time: [0-9.]+ s" | awk '{print $2}')
        echo -n "Run $r ($t_val s)  "
        echo "newyork,OptOmp,$t,$r,$t_val" >> "$CSV_FILE"
    done
    echo ""
done

echo ""
echo "Mesures terminées. Analyse statistique et génération des résultats..."

# Calcul des statistiques avec Python (bibliothèque standard)
python3 - <<EOF
import csv
import statistics
from collections import defaultdict

data = []
with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data.append({
            'Graph': row['Graph'],
            'Version': row['Version'],
            'Threads': int(row['Threads']),
            'Run': int(row['Run']),
            'Time': float(row['Time'])
        })

grouped = defaultdict(list)
for r in data:
    grouped[(r['Graph'], r['Version'], r['Threads'])].append(r['Time'])

summary = {}
for key, times in grouped.items():
    summary[key] = {
        'min': min(times),
        'max': max(times),
        'avg': statistics.mean(times),
        'median': statistics.median(times)
    }

# Enregistrement du résumé dans un CSV
with open('data/output/summary.csv', 'w') as f:
    writer = csv.writer(f)
    writer.writerow(['Graph', 'Version', 'Threads', 'MinTime', 'MaxTime', 'AvgTime', 'MedianTime', 'SpeedupMin', 'SpeedupAvg'])
    
    for graph in ['rome99', 'newyork']:
        # Base seq times for speedup calculation
        if graph == 'rome99':
            base_naive_min = summary[('rome99', 'NaiveSeq', 1)]['min']
            base_naive_avg = summary[('rome99', 'NaiveSeq', 1)]['avg']
            base_opt_min = summary[('rome99', 'OptSeq', 1)]['min']
            base_opt_avg = summary[('rome99', 'OptSeq', 1)]['avg']
        else:
            base_opt_min = summary[('newyork', 'OptSeq', 1)]['min']
            base_opt_avg = summary[('newyork', 'OptSeq', 1)]['avg']
            
        for (g, v, t), stats in sorted(summary.items()):
            if g != graph:
                continue
            if v == 'NaiveSeq' or v == 'NaiveOmp':
                speedup_min = base_naive_min / stats['min']
                speedup_avg = base_naive_avg / stats['avg']
            else:
                speedup_min = base_opt_min / stats['min']
                speedup_avg = base_opt_avg / stats['avg']
                
            writer.writerow([
                g, v, t, stats['min'], stats['max'], stats['avg'], stats['median'],
                speedup_min, speedup_avg
            ])

# Affichage des résultats
print("\n" + "="*80)
print("=== RÉSULTATS SYNTHÉTIQUES SUR ROME (3353 NOEUDS) ===")
print("="*80)
print(f"{'Version':<12} | {'Threads':<8} | {'Min Time (s)':<12} | {'Median Time':<12} | {'Avg Time (s)':<12} | {'Speedup (Min)':<12}")
print("-"*80)

# Rome Naive
stats = summary[('rome99', 'NaiveSeq', 1)]
print(f"{'Naive Seq':<12} | {'1':<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {'1.00x':<12}")
for t in THREADS:
    stats = summary[('rome99', 'NaiveOmp', t)]
    spd = summary[('rome99', 'NaiveSeq', 1)]['min'] / stats['min']
    print(f"{'Naive Omp':<12} | {t:<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {spd:<11.2f}x")

print("-"*80)
# Rome Opt
stats = summary[('rome99', 'OptSeq', 1)]
print(f"{'Opt Seq':<12} | {'1':<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {'1.00x':<12}")
for t in THREADS:
    stats = summary[('rome99', 'OptOmp', t)]
    spd = summary[('rome99', 'OptSeq', 1)]['min'] / stats['min']
    print(f"{'Opt Omp':<12} | {t:<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {spd:<11.2f}x")

print("\n" + "="*80)
print("=== RÉSULTATS SYNTHÉTIQUES SUR NEW YORK (264346 NOEUDS) ===")
print("="*80)
print(f"{'Version':<12} | {'Threads':<8} | {'Min Time (s)':<12} | {'Median Time':<12} | {'Avg Time (s)':<12} | {'Speedup (Min)':<12}")
print("-"*80)
stats = summary[('newyork', 'OptSeq', 1)]
print(f"{'Opt Seq':<12} | {'1':<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {'1.00x':<12}")
for t in THREADS:
    stats = summary[('newyork', 'OptOmp', t)]
    spd = summary[('newyork', 'OptSeq', 1)]['min'] / stats['min']
    print(f"{'Opt Omp':<12} | {t:<8} | {stats['min']:<12.6f} | {stats['median']:<12.6f} | {stats['avg']:<12.6f} | {spd:<11.2f}x")

print("="*80)
print("\nSynthèse enregistrée dans data/output/summary.csv")
EOF
