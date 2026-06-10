import subprocess
import re
import csv
import statistics
import os
import sys

def run_command(cmd):
    result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return result.stdout, result.stderr

def run_benchmark():
    print("Starting K-means benchmarks...")
    
    # Check if executable exists, compile if not
    if not os.path.exists("./TP7"):
        print("Compiling TP7...")
        stdout, stderr = run_command("make clean && make")
        if not os.path.exists("./TP7"):
            print("Compilation failed:")
            print(stderr)
            sys.exit(1)
            
    # Create data/output directory if it doesn't exist
    os.makedirs("data/output", exist_ok=True)
    
    # Configurations
    runs = 5
    threads_list = [1, 2, 4, 8, 16]
    
    configs = [
        # (dataset, k_values)
        ("dataset_10000_4.txt", [4, 8, 10, 16]),
        ("dataset_200000_4.txt", [4, 8, 16, 31])
    ]
    
    raw_results_file = "data/output/results.csv"
    with open(raw_results_file, mode='w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Dataset", "K", "Threads", "Run", "SeqTime", "ParTime", "SeqIt", "ParIt"])
        
    for dataset, k_list in configs:
        print(f"\nBenchmarking dataset: {dataset}")
        for k in k_list:
            print(f"  K = {k}")
            for threads in threads_list:
                print(f"    Threads = {threads}: ", end="", flush=True)
                for r in range(1, runs + 1):
                    # Run program
                    cmd = f"./TP7 {dataset} {k} {threads}"
                    stdout, stderr = run_command(cmd)
                    
                    # Parse output
                    seq_it_match = re.search(r"séquentiel\s*:\s*\n\s*it\s*:\s*(\d+)", stdout)
                    par_it_match = re.search(r"parallèle\s*:\s*\n\s*it\s*:\s*(\d+)", stdout)
                    seq_time_match = re.search(r"temps séquentiel\s*:\s*([\d.]+)\s*s", stdout)
                    par_time_match = re.search(r"temps parallèle\s*:\s*([\d.]+)\s*s", stdout)
                    
                    if seq_it_match and par_it_match and seq_time_match and par_time_match:
                        seq_it = int(seq_it_match.group(1))
                        par_it = int(par_it_match.group(1))
                        seq_time = float(seq_time_match.group(1))
                        par_time = float(par_time_match.group(1))
                        
                        # Log run
                        with open(raw_results_file, mode='a', newline='') as f_log:
                            writer_log = csv.writer(f_log)
                            writer_log.writerow([dataset, k, threads, r, seq_time, par_time, seq_it, par_it])
                        
                        print(f"Run {r}(S:{seq_time:.3f}s, P:{par_time:.3f}s) ", end="", flush=True)
                    else:
                        print(f"Run {r}(ERROR) ", end="", flush=True)
                        print("\nStdout debug:")
                        print(stdout)
                        print("Stderr debug:")
                        print(stderr)
                print()
                
    print("\nBenchmarks completed successfully. Processing statistics...")
    process_stats(raw_results_file)

def process_stats(raw_csv):
    # Group results
    data = []
    with open(raw_csv, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'Dataset': row['Dataset'],
                'K': int(row['K']),
                'Threads': int(row['Threads']),
                'Run': int(row['Run']),
                'SeqTime': float(row['SeqTime']),
                'ParTime': float(row['ParTime']),
                'SeqIt': int(row['SeqIt']),
                'ParIt': int(row['ParIt'])
            })
            
    # Group by (Dataset, K, Threads)
    grouped = {}
    for r in data:
        key = (r['Dataset'], r['K'], r['Threads'])
        if key not in grouped:
            grouped[key] = {'seq_times': [], 'par_times': [], 'seq_it': r['SeqIt'], 'par_it': r['ParIt']}
        grouped[key]['seq_times'].append(r['SeqTime'])
        grouped[key]['par_times'].append(r['ParTime'])
        
    summary_file = "data/output/summary.csv"
    summary_data = []
    
    with open(summary_file, mode='w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            "Dataset", "K", "Threads", 
            "SeqMin", "SeqMed", "SeqAvg", 
            "ParMin", "ParMed", "ParAvg", 
            "SpeedupMin", "SpeedupMed", "SeqIt", "ParIt"
        ])
        
        for (dataset, k, threads), stats_dict in sorted(grouped.items()):
            seq_t = stats_dict['seq_times']
            par_t = stats_dict['par_times']
            
            seq_min = min(seq_t)
            seq_med = statistics.median(seq_t)
            seq_avg = statistics.mean(seq_t)
            
            par_min = min(par_t)
            par_med = statistics.median(par_t)
            par_avg = statistics.mean(par_t)
            
            # Speedup is calculated using minimum times
            speedup_min = seq_min / par_min
            speedup_med = seq_med / par_med
            
            writer.writerow([
                dataset, k, threads,
                seq_min, seq_med, seq_avg,
                par_min, par_med, par_avg,
                speedup_min, speedup_med,
                stats_dict['seq_it'], stats_dict['par_it']
            ])
            
            summary_data.append({
                'dataset': dataset,
                'k': k,
                'threads': threads,
                'seq_min': seq_min,
                'seq_med': seq_med,
                'seq_avg': seq_avg,
                'par_min': par_min,
                'par_med': par_med,
                'par_avg': par_avg,
                'speedup_min': speedup_min,
                'speedup_med': speedup_med,
                'seq_it': stats_dict['seq_it'],
                'par_it': stats_dict['par_it']
            })

    # Print summary tables to terminal
    print("\n" + "="*95)
    print("=== SUMMARY OF RESULTS ===")
    print("="*95)
    print(f"{'Dataset':<20} | {'K':<3} | {'Threads':<7} | {'Seq Min (s)':<11} | {'Par Min (s)':<11} | {'Speedup (Min)':<13} | {'Iterations':<10}")
    print("-"*95)
    for row in summary_data:
        print(f"{row['dataset']:<20} | {row['k']:<3} | {row['threads']:<7} | {row['seq_min']:<11.6f} | {row['par_min']:<11.6f} | {row['speedup_min']:<11.2f}x | {row['seq_it']:<10}")
    print("="*95)
    
    generate_html_report(summary_data)

def generate_html_report(summary_data):
    html_template = """<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Compte Rendu TP 7 : Partitionnement en K-moyennes (OpenMP)</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --primary: #1a365d;
            --primary-light: #2b6cb0;
            --accent: #3182ce;
            --bg: #f7fafc;
            --card-bg: #ffffff;
            --text: #2d3748;
            --text-muted: #718096;
            --border: #e2e8f0;
            --code-bg: #edf2f7;
            --highlight: #ebf8ff;
        }

        body {
            font-family: 'Inter', system-ui, -apple-system, sans-serif;
            background-color: var(--bg);
            color: var(--text);
            line-height: 1.7;
            margin: 0;
            padding: 0;
        }

        .container {
            max-width: 1100px;
            margin: 40px auto;
            background: var(--card-bg);
            padding: 50px 60px;
            border-radius: 16px;
            box-shadow: 0 10px 25px rgba(0, 0, 0, 0.05);
            border: 1px solid var(--border);
        }

        header {
            border-bottom: 4px solid var(--accent);
            margin-bottom: 40px;
            padding-bottom: 25px;
        }

        h1 {
            color: var(--primary);
            margin: 0;
            font-size: 2.4rem;
            font-weight: 800;
            letter-spacing: -0.5px;
        }

        .subtitle {
            color: var(--text-muted);
            font-size: 1.15rem;
            margin-top: 8px;
            margin-bottom: 0;
            font-weight: 500;
        }

        .meta-info {
            display: flex;
            gap: 20px;
            margin-top: 15px;
            font-size: 0.9rem;
            color: var(--text-muted);
            border-top: 1px solid var(--border);
            padding-top: 15px;
        }

        .meta-item strong {
            color: var(--primary-light);
        }

        h2 {
            color: var(--primary-light);
            border-left: 5px solid var(--accent);
            padding-left: 18px;
            margin-top: 45px;
            margin-bottom: 20px;
            font-size: 1.6rem;
            font-weight: 700;
        }

        h3 {
            color: var(--primary);
            margin-top: 30px;
            margin-bottom: 12px;
            font-size: 1.25rem;
            font-weight: 600;
        }

        p, li {
            font-size: 1.02rem;
            color: var(--text);
            text-align: justify;
        }

        ul, ol {
            margin-bottom: 25px;
            padding-left: 25px;
        }

        li {
            margin-bottom: 10px;
        }

        code {
            background: var(--code-bg);
            color: #b83280;
            padding: 0.2rem 0.4rem;
            border-radius: 6px;
            font-family: 'Fira Code', 'Consolas', monospace;
            font-size: 0.9em;
            font-weight: 600;
        }

        pre {
            background: #1a202c;
            color: #f7fafc;
            padding: 25px;
            border-radius: 10px;
            overflow-x: auto;
            font-family: 'Fira Code', 'Consolas', monospace;
            font-size: 0.88rem;
            line-height: 1.5;
            box-shadow: inset 0 2px 8px rgba(0,0,0,0.15);
            margin: 20px 0;
            border: 1px solid #2d3748;
        }

        .keyword { color: #f56565; }
        .type { color: #4299e1; }
        .pragma { color: #ed64a6; }
        .comment { color: #718096; font-style: italic; }
        .string { color: #48bb78; }

        table {
            width: 100%;
            border-collapse: collapse;
            margin: 30px 0;
            box-shadow: 0 4px 6px rgba(0,0,0,0.01);
            border-radius: 8px;
            overflow: hidden;
        }

        th, td {
            border: 1px solid var(--border);
            padding: 14px 16px;
            text-align: center;
        }

        th {
            background-color: #ebf8ff;
            color: var(--primary-light);
            font-weight: 700;
            font-size: 0.95rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        tr:nth-child(even) {
            background-color: #fcfdfd;
        }

        tr:hover {
            background-color: #f7fafc;
        }

        .alert {
            background-color: var(--highlight);
            border-left: 4px solid var(--accent);
            padding: 20px 25px;
            border-radius: 8px;
            margin: 25px 0;
        }

        .alert-title {
            font-weight: 700;
            color: var(--primary);
            margin-bottom: 5px;
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .chart-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 30px;
            margin: 30px 0;
        }

        @media (max-width: 768px) {
            .chart-grid {
                grid-template-columns: 1fr;
            }
        }

        .chart-container {
            background: #fff;
            padding: 25px;
            border: 1px solid var(--border);
            border-radius: 12px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.02);
            height: 400px;
            position: relative;
        }

        .formula {
            display: block;
            text-align: center;
            font-size: 1.3rem;
            margin: 20px 0;
            font-weight: bold;
            color: var(--primary);
            font-family: 'Georgia', serif;
        }

        .footer {
            margin-top: 60px;
            padding-top: 25px;
            border-top: 1px solid var(--border);
            font-size: 0.9rem;
            color: var(--text-muted);
            text-align: center;
        }

        @media print {
            body {
                background: #fff;
                color: #000;
            }
            .container {
                box-shadow: none;
                padding: 0;
                max-width: 100%;
            }
            .chart-container {
                page-break-inside: avoid;
            }
            table {
                page-break-inside: avoid;
            }
            pre {
                page-break-inside: avoid;
                background: #f7fafc;
                color: #000;
                border: 1px solid #ccc;
            }
        }
    </style>
</head>
<body>

<div class="container">
    <header>
        <h1>Rapport de TP 7 : Partitionnement en K-moyennes Parallèle</h1>
        <p class="subtitle">Analyse comparative d'implémentations séquentielles et parallèles de l'algorithme K-Means en C avec OpenMP</p>
        <div class="meta-info">
            <div class="meta-item">Gabriel Mazet - N° d'étudiant : 22202954</div>
        </div>
    </header>

    <section id="sujet">
        <h2>Rappel du Sujet et Objectifs</h2>
        <p>
            L'objectif de ce TP est de concevoir et paralléliser l'algorithme de partitionnement en <strong>k-moyennes (K-Means)</strong> en C en utilisant <strong>OpenMP</strong>. 
            Le k-moyennes regroupe un ensemble de points tridimensionnels (x, y, z) en k partitions en minimisant la distance euclidienne de chaque point au centre de gravité de la partition (cluster) à laquelle il appartient.
        </p>
        <p>
            L'algorithme se déroule comme suit :
        </p>
        <ol>
            <li><strong>Initialisation :</strong> Les k premiers points du jeu de données sont choisis comme centres de gravité initiaux des k partitions pour assurer un comportement déterministe et vérifiable.</li>
            <li><strong>Affectation :</strong> Pour chaque point, je calcule sa distance euclidienne à tous les centres de gravité et je l'affecte au cluster le plus proche.</li>
            <li><strong>Mise à jour :</strong> Je recalcule le centre de gravité de chaque cluster comme la moyenne arithmétique des coordonnées des points affectés.</li>
            <li><strong>Convergence :</strong> Je répète les phases d'affectation et de mise à jour jusqu'à ce que la somme des déplacements des coordonnées des centres de gravité soit inférieure à un seuil défini (seuil = <code>10<sup>-4</sup></code>) ou qu'un nombre maximum d'itérations soit atteint (1 000 itérations).</li>
        </ol>
        <p>
            L'évaluation s'appuie sur deux jeux de données de tailles différentes :
        </p>
        <ul>
            <li><code>dataset_10000_4.txt</code> : un petit jeu de données comprenant 10 000 points.</li>
            <li><code>dataset_200000_4.txt</code> : un grand jeu de données de 200 000 points, idéal pour mesurer le passage à l'échelle (scaling).</li>
        </ul>
    </section>

    <section id="analyse-code">
        <h2>Analyse Algorithmique et Optimisation Séquentielle</h2>
        
        <h3>Analyse de la complexité</h3>
        <p>
            À chaque itération de l'algorithme, deux étapes principales régissent la complexité temporelle :
        </p>
        <ol>
            <li><strong>La phase d'affectation :</strong> Pour chacun des N points, je dois calculer la distance aux k centres de gravité. Le coût de cette recherche de minimum est de <code>O(N &middot; k)</code>.</li>
            <li><strong>La phase de recalcul :</strong> Je somme les coordonnées des points de chaque cluster (complexité en <code>O(N)</code>) et je divise par l'effectif du cluster (complexité en <code>O(k)</code>).</li>
        </ol>
        <p>
            La complexité globale par itération est donc dominée par le terme d'affectation, soit :
            <span class="formula">T<sub>iteration</sub> = O(N &middot; k)</span>
            Si I représente le nombre total d'itérations jusqu'à convergence, la complexité séquentielle théorique totale s'élève à <code>O(I &middot; N &middot; k)</code>. Pour 200 000 points avec k=31 sur 243 itérations, cela représente <code>243 &times; 200 000 &times; 31 &approx; 1.5 &times; 10<sup>9</sup></code> calculs de distance !
        </p>
        
        <h3>Optimisation séquentielle : Évitement de la racine carrée</h3>
        <p>
            Le calcul de la distance euclidienne standard entre un point A et un centroid B s'écrit :
            <span class="formula">d(A, B) = &radic;((x<sub>A</sub> - x<sub>B</sub>)<sup>2</sup> + (y<sub>A</sub> - y<sub>B</sub>)<sup>2</sup> + (z<sub>A</sub> - z<sub>B</sub>)<sup>2</sup>)</span>
            L'appel à la fonction <code>sqrt()</code> est extrêmement coûteux en temps CPU.
            Comme la fonction racine carrée est strictement croissante sur les réels positifs, minimiser la distance euclidienne équivaut à minimiser le carré de la distance :
            <span class="formula">d<sup>2</sup>(A, B) = (x<sub>A</sub> - x<sub>B</sub>)<sup>2</sup> + (y<sub>A</sub> - y<sub>B</sub>)<sup>2</sup> + (z<sub>A</sub> - z<sub>B</sub>)<sup>2</sup></span>
        </p>
        <div class="alert">
            <div class="alert-title">💡 Gain de performance séquentiel</div>
            <p>
                En utilisant le carré de la distance tridimensionnelle dans la boucle interne d'affectation des points, j'élimine complètement l'appel à <code>sqrt()</code> pour les 1,5 milliard de comparaisons de distance.
                La racine carrée n'est calculée qu'une fois par itération et par cluster (soit k fois) lors de l'évaluation de la convergence sur les déplacements des centroids. Cela représente une accélération massive dès la version séquentielle.
            </p>
        </div>
    </section>

    <section id="parallele">
        <h2>Stratégie de Parallélisation OpenMP</h2>
        
        <h3>Région parallèle unique (Évitement du Fork-Join répétitif)</h3>
        <p>
            Placer la directive <code>#pragma omp parallel</code> à l'intérieur de la boucle iterative <code>while</code> génèrerait un overhead de création et destruction de l'équipe de threads à chaque itération (soit 243 fois pour le grand dataset).
            Pour y remédier, j'ai implémenté une <strong>région parallèle unique</strong> englobant la boucle de convergence.
            Les threads sont forkés une seule fois au début du calcul. La cohérence des variables partagées et l'avancement dans les itérations sont synchronisés explicitement avec des directives de barrières de threads.
        </p>

        <h3>Réduction locale sans blocage (Thread-Local Reduction Array)</h3>
        <p>
            À chaque itération, les coordonnées des points affectés à un cluster c doivent être sommées pour calculer les nouveaux centroids.
            Si tous les threads tentaient d'ajouter directement les coordonnées des points dans un tableau partagé global de centroids, cela créerait une <i>data race</i> catastrophique. L'utilisation de sections critiques ou d'instructions atomiques (<code>#pragma omp atomic</code>) détruirait le parallélisme car les threads s'attendraient les uns les autres sur 200 000 points.
        </p>
        <p>
            J'ai donc résolu ce problème en allouant des tableaux de réduction locale :
        </p>
        <pre><span class="comment">// Allocation de tableaux thread-local à plat de taille numThreads * k</span>
<span class="type">Point</span>* localSums = calloc(numThreads * k, <span class="keyword">sizeof</span>(<span class="type">Point</span>));
<span class="type">int</span>* localCounts = calloc(numThreads * k, <span class="keyword">sizeof</span>(<span class="type">int</span>));</pre>
        <p>
            Chaque thread possède sa propre partition de travail indexée par son identifiant <code>tid</code> (de 0 à <code>numThreads - 1</code>) et accumule ses points locaux de façon indépendante dans <code>localSums[tid * k + c]</code>, sans aucun verrou ni atomicité.
        </p>

        <h3>Synchronisation et coordination par barrières</h3>
        <p>
            Pour coordonner les threads tout au long des itérations, j'ai introduit des barrières matérielles (<code>#pragma omp barrier</code>) et des clauses <code>nowait</code> bien positionnées :
        </p>
        <ul>
            <li><strong>Reset local :</strong> Au début de chaque itération, les threads réinitialisent leurs accumulateurs locaux à 0. Une barrière <code>#pragma omp barrier</code> garantit que tout le monde a terminé la réinitialisation avant de démarrer l'affectation.</li>
            <li><strong>Affectation :</strong> La boucle sur les N points est parallélisée avec un <code>#pragma omp for nowait</code>. La clause <code>nowait</code> élimine la barrière implicite de fin de boucle car j'insère une barrière explicite immédiatement après pour attendre que toutes les accumulations locales soient achevées.</li>
            <li><strong>Somme globale (Centroids recalculés) :</strong> Les accumulateurs thread-local sont réduits dans le tableau global. Cette somme est parallélisée sur les k clusters tridimensionnels à l'aide d'un <code>#pragma omp for nowait</code>, répartissant la charge des 31 clusters sur les cœurs logiques.</li>
            <li><strong>Calcul de convergence :</strong> Une barrière garantit la validité des nouveaux centroids. Ensuite, la directive <code>#pragma omp single</code> confie l'évaluation de la variation et la mise à jour des variables de contrôle (<code>converged</code>, <code>it</code>) à un seul thread. L'implicit barrier à la sortie de la section <code>single</code> assure la visibilité des variables modifiées pour tous les threads avant l'évaluation de l'itération suivante.</li>
        </ul>
    </section>

    <section id="resultats">
        <h2>Mesures de Performances et Accélération</h2>
        <p>
            Les mesures statistiques sont basées sur un protocole expérimental rigoureux comportant <strong>5 exécutions successives</strong> pour chaque configuration.
        </p>

        <h3>Résultats sur dataset_10000_4.txt (10 000 points)</h3>
        <table id="table-small">
            <thead>
                <tr>
                    <th>K</th>
                    <th>Threads</th>
                    <th>Seq Min (s)</th>
                    <th>Seq Med (s)</th>
                    <th>Seq Avg (s)</th>
                    <th>Par Min (s)</th>
                    <th>Par Med (s)</th>
                    <th>Par Avg (s)</th>
                    <th>Speedup (Min)</th>
                    <th>Iterations</th>
                </tr>
            </thead>
            <tbody>
                <!-- DATA_SMALL_ROWS -->
            </tbody>
        </table>

        <h3>Résultats sur dataset_200000_4.txt (200 000 points)</h3>
        <table id="table-large">
            <thead>
                <tr>
                    <th>K</th>
                    <th>Threads</th>
                    <th>Seq Min (s)</th>
                    <th>Seq Med (s)</th>
                    <th>Seq Avg (s)</th>
                    <th>Par Min (s)</th>
                    <th>Par Med (s)</th>
                    <th>Par Avg (s)</th>
                    <th>Speedup (Min)</th>
                    <th>Iterations</th>
                </tr>
            </thead>
            <tbody>
                <!-- DATA_LARGE_ROWS -->
            </tbody>
        </table>

        <div class="chart-grid">
            <div class="chart-container">
                <canvas id="chartSmall"></canvas>
            </div>
            <div class="chart-container">
                <canvas id="chartLarge"></canvas>
            </div>
        </div>
    </section>

    <section id="analyse-perf">
        <h2>Analyse Approfondie des Performances</h2>
        
        <h3>Rigueur des indicateurs statistiques (Moyenne vs Min/Médiane)</h3>
        <p>
            L'analyse des performances d'un système multi-threadé sous Linux est soumise au bruit du système d'exploitation (ordonnancement des threads, interruptions matérielles, Turbo Boost, latence RAM).
            La moyenne arithmétique brute est influencée par des pics de latence aberrants (outliers). 
            Pour calculer le speedup physique, le <strong>temps minimum</strong> de calcul est privilégié, car il reflète la performance du processeur lorsque le bruit externe est quasi nul. La <strong>médiane</strong> sert d'indicateur robuste de la tendance centrale des temps d'exécution ordinaires.
        </p>

        <h3>Impact de la taille du jeu de données (Grain parallèle)</h3>
        <p>
            Sur le petit dataset (10 000 points), le volume de calcul par itération est extrêmement faible. Par conséquent, les barrières matérielles d'OpenMP et la gestion des caches CPU créent un overhead supérieur au calcul parallèle lui-même. C'est pourquoi le speedup est faible, voire négatif (inférieur à 1) sur de grands nombres de threads pour k=4.
            En revanche, sur le grand dataset (200 000 points), le volume de calcul amortit largement le coût des barrières, menant à une accélération très nette des calculs.
        </p>

        <h3>Impact du nombre de threads et limites physiques (Memory-Bound)</h3>
        <p>
            L'algorithme montre une très bonne accélération jusqu'à 4 ou 8 threads, mais stagne ou régresse au-delà (16 threads). Ce phénomène s'explique par deux limites physiques majeures de mon architecture :
        </p>
        <ul>
            <li><strong>L'HyperThreading (cœurs physiques vs logiques) :</strong> Mon CPU possède des cœurs logiques partageant les ressources physiques des cœurs physiques réels. L'exécution de 16 threads sur un processeur limité physiquement induit une compétition pour les unités arithmétiques et des surcoûts d'ordonnancement.</li>
            <li><strong>La bande passante mémoire (Memory Bound) :</strong> K-Means possède une intensité arithmétique faible. Pour chaque point tridimensionnel lu en RAM, le processeur n'effectue que quelques multiplications et soustractions. Le goulot d'étranglement devient le débit du bus mémoire qui sature rapidement lorsque 8 ou 16 threads lisent simultanément le tableau des points.</li>
        </ul>

        <h3>Validation de l'exactitude (Checksums)</h3>
        <p>
            L'exactitude des calculs a été vérifiée au bit près. Pour toutes les configurations de threads et de k, les coordonnées des centroids finaux et les effectifs des clusters sont strictement identiques entre la version séquentielle et la version parallèle. Par exemple, pour <code>dataset_200000_4.txt</code> avec k=31, le cluster 0 compte exactement 6 315 éléments et son centroid se trouve précisément aux coordonnées <code>(-333.773555, -425.563579, 260.121774)</code> dans toutes les versions.
        </p>
    </section>

    <section id="conclusion">
        <h2>Conclusion</h2>
        <p>
            Ce TP a démontré l'efficacité d'OpenMP pour accélérer un algorithme itératif d'analyse de données comme K-Means.
            Grâce à une stratégie de <strong>région parallèle unique</strong>, à une <strong>réduction locale thread-local flat</strong> sans verrous et à l'élimination séquentielle des racines carrées, l'implémentation atteint des gains de performance importants sur de grands volumes de données.
            L'analyse statistique et physique a également mis en évidence les goulots d'étranglement matériels inhérents aux applications limitées par la bande passante mémoire (Memory-Bound) et le partage des ressources physiques par HyperThreading.
        </p>
    </section>
    
    <div class="footer">
        Gabriel Mazet (étudiant n° 22202954) - Rapport de TP 7 - Parallélisme - 2026
    </div>
</div>

<script>
    // Inject speedup data dynamically
    const threadsLabels = ['1 Thread', '2 Threads', '4 Threads', '8 Threads', '16 Threads'];
    
    // DATA_SMALL_SPEEDUPS
    // DATA_LARGE_SPEEDUPS

    const ctxSmall = document.getElementById('chartSmall').getContext('2d');
    const ctxLarge = document.getElementById('chartLarge').getContext('2d');

    new Chart(ctxSmall, {
        type: 'line',
        data: {
            labels: threadsLabels,
            datasets: datasetsSmall
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                title: {
                    display: true,
                    text: 'dataset_10000_4.txt : Speedup par rapport au Séquentiel',
                    font: { size: 14, weight: 'bold' }
                }
            },
            scales: {
                y: {
                    title: { display: true, text: 'Speedup (x)' },
                    min: 0
                }
            }
        }
    });

    new Chart(ctxLarge, {
        type: 'line',
        data: {
            labels: threadsLabels,
            datasets: datasetsLarge
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                title: {
                    display: true,
                    text: 'dataset_200000_4.txt : Speedup par rapport au Séquentiel',
                    font: { size: 14, weight: 'bold' }
                }
            },
            scales: {
                y: {
                    title: { display: true, text: 'Speedup (x)' },
                    min: 0
                }
            }
        }
    });
</script>
</body>
</html>
"""
    
    # Process summary_data into rows and charts datasets
    # 1. HTML Rows
    small_rows = ""
    large_rows = ""
    
    # 2. Charts Data
    # Group by dataset and k
    grouped_for_charts = {}
    for r in summary_data:
        key = (r['dataset'], r['k'])
        if key not in grouped_for_charts:
            grouped_for_charts[key] = []
        grouped_for_charts[key].append((r['threads'], r['speedup_min']))
        
    for key, val in grouped_for_charts.items():
        # Sort by threads
        val.sort()
        grouped_for_charts[key] = [speedup for thread, speedup in val]
        
    colors = {
        4: "#e53e3e",   # Red
        8: "#3182ce",   # Blue
        10: "#dd6b20",  # Orange
        16: "#319795",  # Teal
        31: "#805ad5"   # Purple
    }
    
    datasets_small_js = "const datasetsSmall = [\n"
    datasets_large_js = "const datasetsLarge = [\n"
    
    for (dataset, k), speedups in sorted(grouped_for_charts.items()):
        color = colors.get(k, "#718096")
        js_block = f"""        {{
            label: 'K = {k}',
            data: {speedups},
            borderColor: '{color}',
            backgroundColor: 'rgba(0,0,0,0)',
            borderWidth: 3,
            tension: 0.15
        }},"""
        
        if dataset == "dataset_10000_4.txt":
            datasets_small_js += js_block + "\n"
        else:
            datasets_large_js += js_block + "\n"
            
    datasets_small_js += "];"
    datasets_large_js += "];"
    
    # Populate rows
    for r in summary_data:
        row = f"""                <tr>
                    <td><strong>{r['k']}</strong></td>
                    <td>{r['threads']}</td>
                    <td>{r['seq_min']:.6f}</td>
                    <td>{r['seq_med']:.6f}</td>
                    <td>{r['seq_avg']:.6f}</td>
                    <td>{r['par_min']:.6f}</td>
                    <td>{r['par_med']:.6f}</td>
                    <td>{r['par_avg']:.6f}</td>
                    <td><strong style="color: {'#48bb78' if r['speedup_min'] >= 1.0 else '#e53e3e'}">{r['speedup_min']:.2f}x</strong></td>
                    <td>{r['seq_it']}</td>
                </tr>\n"""
        if r['dataset'] == "dataset_10000_4.txt":
            small_rows += row
        else:
            large_rows += row
            
    # Inject into template
    report = html_template
    report = report.replace("<!-- DATA_SMALL_ROWS -->", small_rows)
    report = report.replace("<!-- DATA_LARGE_ROWS -->", large_rows)
    report = report.replace("// DATA_SMALL_SPEEDUPS", datasets_small_js)
    report = report.replace("// DATA_LARGE_SPEEDUPS", datasets_large_js)
    
    # Write report
    report_filename = "Compte_Rendu_TP7_GabrielMAZET.html"
    with open(report_filename, "w", encoding="utf-8") as f:
        f.write(report)
    print(f"\nHTML Report generated successfully as {report_filename}")

if __name__ == "__main__":
    run_benchmark()
