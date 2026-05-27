#!/bin/bash
export LC_ALL=C

# Compilation
echo "Compilation du programme..."
make clean
make

if [ ! -f ./bin/contrast ]; then
    echo "Erreur: la compilation a échoué."
    exit 1
fi

mkdir -p data/output

echo ""
echo "============================================="
echo "1. Vérification de la mémoire avec Valgrind"
echo "============================================="
echo "Lancement de Valgrind sur image0.ppm (Version Séquentielle)..."
valgrind --leak-check=full --show-leak-kinds=all ./bin/contrast data/image0.ppm data/output/image0_valgrind_seq.pgm 0

echo ""
echo "Lancement de Valgrind sur image0.ppm (Version Parallèle - 2 Threads)..."
valgrind --leak-check=full --show-leak-kinds=all ./bin/contrast data/image0.ppm data/output/image0_valgrind_par.pgm 2

echo ""
echo "Vérification de l'exactitude des résultats..."
diff -q data/output/image0_valgrind_seq.pgm data/output/image0_valgrind_par.pgm
if [ $? -eq 0 ]; then
    echo "--> [SUCCÈS] Les sorties séquentielle et parallèle sont identiques."
else
    echo "--> [ÉCHEC] Les sorties séquentielle et parallèle diffèrent !"
    exit 1
fi

# Nettoyage des fichiers temporaires de valgrind
rm -f data/output/image0_valgrind_seq.pgm data/output/image0_valgrind_par.pgm

echo ""
echo "============================================="
echo "2. Lancement des mesures de performances"
echo "============================================="

IMAGES=("data/image0.ppm" "data/image1.ppm" "data/image2.ppm")
THREADS=(0 1 2 4 8 16)
RUNS=5

CSV_FILE="data/output/results.csv"
echo "Image,Threads,Run,ToGreyTime,ContrastTime,TotalTime" > "$CSV_FILE"

for img in "${IMAGES[@]}"; do
    img_name=$(basename "$img" .ppm)
    echo "Traitement de $img_name..."
    for t in "${THREADS[@]}"; do
        if [ "$t" -eq 0 ]; then
            echo -n "  Séquentiel : "
        else
            echo -n "  Threads $t : "
        fi
        
        for ((r=1; r<=RUNS; r++)); do
            # Lancement et récupération des temps
            res=$(./bin/contrast "$img" "data/output/${img_name}_t${t}_r${r}.pgm" "$t")
            to_grey=$(echo "$res" | cut -d',' -f1)
            contrast=$(echo "$res" | cut -d',' -f2)
            total=$(echo "$res" | cut -d',' -f3)
            echo -n "Run $r ($total s)  "
            echo "$img_name,$t,$r,$to_grey,$contrast,$total" >> "$CSV_FILE"
        done
        echo ""
    done
done

# Nettoyer les grandes images générées pour ne pas encombrer le disque
# mais garder au moins une de chaque pour vérification
for img in "${IMAGES[@]}"; do
    img_name=$(basename "$img" .ppm)
    mv "data/output/${img_name}_t0_r1.pgm" "data/output/${img_name}_seq.pgm"
    mv "data/output/${img_name}_t4_r1.pgm" "data/output/${img_name}_par.pgm"
    rm -f data/output/${img_name}_t*.pgm
done

echo ""
echo "Mesures terminées. Traitement des données..."

# Calcul des moyennes avec Python (bibliothèque standard uniquement)
python3 - <<EOF
import csv
from collections import defaultdict

data = []
with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data.append({
            'Image': row['Image'],
            'Threads': int(row['Threads']),
            'Run': int(row['Run']),
            'ToGreyTime': float(row['ToGreyTime']),
            'ContrastTime': float(row['ContrastTime']),
            'TotalTime': float(row['TotalTime'])
        })

grouped = defaultdict(list)
for r in data:
    grouped[(r['Image'], r['Threads'])].append(r)

summary = {}
for (img, threads), runs in grouped.items():
    avg_to_grey = sum(r['ToGreyTime'] for r in runs) / len(runs)
    avg_contrast = sum(r['ContrastTime'] for r in runs) / len(runs)
    avg_total = sum(r['TotalTime'] for r in runs) / len(runs)
    summary[(img, threads)] = {
        'AvgToGrey': avg_to_grey,
        'AvgContrast': avg_contrast,
        'AvgTotal': avg_total
    }

images = sorted(list(set(img for (img, threads) in summary.keys())))
threads_list = sorted(list(set(threads for (img, threads) in summary.keys())))

with open('data/output/summary.csv', 'w') as f:
    writer = csv.writer(f)
    writer.writerow(['Image', 'Threads', 'ToGreyTime', 'ContrastTime', 'TotalTime', 'Speedup', 'SpeedupToGrey', 'SpeedupContrast'])
    
    for img in images:
        seq_total = summary[(img, 0)]['AvgTotal']
        seq_to_grey = summary[(img, 0)]['AvgToGrey']
        seq_contrast = summary[(img, 0)]['AvgContrast']
        
        for t in threads_list:
            vals = summary[(img, t)]
            speedup = seq_total / vals['AvgTotal'] if vals['AvgTotal'] > 0 else 0
            speedup_to_grey = seq_to_grey / vals['AvgToGrey'] if vals['AvgToGrey'] > 0 else 0
            speedup_contrast = seq_contrast / vals['AvgContrast'] if vals['AvgContrast'] > 0 else 0
            
            writer.writerow([
                img, t, vals['AvgToGrey'], vals['AvgContrast'], vals['AvgTotal'],
                speedup, speedup_to_grey, speedup_contrast
            ])

print("\n=== RÉSULTATS SYNTHÉTIQUES (MOYENNE SUR 5 RUNS) ===")
for img in images:
    print(f"\nImage: {img}")
    print(f"{'Threads':<12}{'ToGreyTime':<15}{'ContrastTime':<15}{'TotalTime':<15}{'Speedup':<10}{'SpdGrey':<10}{'SpdContr':<10}")
    print("-" * 87)
    
    seq_total = summary[(img, 0)]['AvgTotal']
    seq_to_grey = summary[(img, 0)]['AvgToGrey']
    seq_contrast = summary[(img, 0)]['AvgContrast']
    
    for t in threads_list:
        vals = summary[(img, t)]
        speedup = seq_total / vals['AvgTotal'] if vals['AvgTotal'] > 0 else 0
        speedup_to_grey = seq_to_grey / vals['AvgToGrey'] if vals['AvgToGrey'] > 0 else 0
        speedup_contrast = seq_contrast / vals['AvgContrast'] if vals['AvgContrast'] > 0 else 0
        
        lbl = "Séquentiel" if t == 0 else str(t)
        print(f"{lbl:<12}{vals['AvgToGrey']:<15.6f}{vals['AvgContrast']:<15.6f}{vals['AvgTotal']:<15.6f}{speedup:<10.2f}{speedup_to_grey:<10.2f}{speedup_contrast:<10.2f}")

print("\nSynthèse enregistrée dans data/output/summary.csv")
EOF
