# sferaBloch (`sbl`)

Visualizzatore interattivo della **sfera di Bloch** scritto in C puro con SDL2.

Il programma disegna un wireframe di sfera in proiezione prospettica, ruotabile con il mouse, e un vettore di stato che rappresenta lo stato di un qubit singolo. Da tastiera si applicano gate quantistici (Pauli X/Y/Z e rotazioni di 45°) e si osserva il movimento del vettore sulla sfera.

Repo: <https://github.com/teocnt/CBlochSphere>

---

## Indice

- [Comandi](#comandi)
- [Logiche matematiche](#logiche-matematiche)
  - [Convenzione degli assi](#convenzione-degli-assi)
  - [Coordinate polari ↔ cartesiane](#coordinate-polari--cartesiane)
  - [Generazione del modello della sfera](#generazione-del-modello-della-sfera)
  - [Rotazioni 3D](#rotazioni-3d)
  - [Proiezione prospettica](#proiezione-prospettica)
  - [Ordinamento in profondità (back/front)](#ordinamento-in-profondità-backfront)
  - [Linee spesse](#linee-spesse)
  - [Gate quantistici](#gate-quantistici)
- [Installazione e compilazione](#installazione-e-compilazione)
- [Compatibilità tra sistemi operativi](#compatibilità-tra-sistemi-operativi)
- [Struttura del progetto](#struttura-del-progetto)
- [Note e limiti noti](#note-e-limiti-noti)

---

## Comandi

| Input | Azione |
|---|---|
| Trascinamento con tasto sinistro del mouse | Ruota la sfera (yaw/pitch) |
| `X` | Applica il gate di Pauli **X** |
| `Y` | Applica il gate di Pauli **Y** |
| `Z` | Applica il gate di Pauli **Z** |
| `Shift` + `X` / `Y` / `Z` | Rotazione di **45°** attorno all'asse corrispondente |
| `R` | Reset dello stato a \|0⟩ |
| `F` | Attiva/disattiva fullscreen (ricalcola gli offset di centratura) |
| `Esc` o chiusura finestra | Uscita |

Ad ogni gate applicato il nuovo stato `(theta, phi)` viene stampato su stdout.

---

## Logiche matematiche

### Convenzione degli assi

Il codice usa una convenzione "grafica" in cui **l'asse polare è `y`** (verticale sullo schermo) e `z` è l'asse di profondità, uscente verso l'osservatore. La corrispondenza con gli assi della sfera di Bloch è quindi:

| Sfera di Bloch | Asse nel codice | Direzione a schermo |
|---|---|---|
| Z (asse \|0⟩–\|1⟩) | `y` | verticale |
| X | `x` | orizzontale |
| Y | `z` | profondità |

Lo stato del qubit è memorizzato direttamente come coppia di angoli in `Polar2 { theta, phi }`, dove `theta ∈ [0, π]` è l'angolo polare misurato dal polo nord (`|0⟩`, cioè `+y`) e `phi ∈ [0, 2π)` è l'azimut nel piano `x–z`. Lo stato iniziale è `(0, 0)` = `|0⟩`.

Questa parametrizzazione corrisponde alla forma standard dello stato puro di un qubit:

```
|ψ⟩ = cos(θ/2) |0⟩ + e^(iφ) sin(θ/2) |1⟩
```

dove `θ` e `φ` sono esattamente gli angoli sferici del vettore di Bloch.

### Coordinate polari ↔ cartesiane

`fromPolarToCartesian` (raggio unitario, asse polare `y`):

```
y = cos θ
x = sin θ · cos φ
z = sin θ · sin φ
```

`fromCartesianToPolar` è l'inversa:

```
θ = acos(clamp(y, -1, 1))
φ = atan2(z, x)          → riportato in [0, 2π) sommando 2π se negativo
```

Il **clamp** di `y` in `[-1, 1]` prima di `acos` è necessario: dopo una catena di rotazioni in virgola mobile il valore può uscire di pochissimo dal dominio, producendo `NaN`.

### Generazione del modello della sfera

La sfera è un reticolo di paralleli e meridiani costruito campionando uniformemente lo spazio degli angoli:

- `NPAR = 12` paralleli, passo `Δθ = π / (NPAR − 1)` → include entrambi i poli;
- `NMER = 24` meridiani, passo `Δφ = 2π / NMER` → chiusura ciclica.

Numero di vertici:

```
nPoints = NPAR · NMER = 288
```

I punti sono memorizzati in ordine *row-major*: l'indice del vertice `(i, j)` è `i·NMER + j`.

Le connessioni (spigoli del wireframe) sono generate in `calculateSphereConnections`:

- un arco **lungo il parallelo**, per ogni punto: `(i, j) → (i, (j+1) mod NMER)` — il modulo chiude l'anello;
- un arco **lungo il meridiano**, solo se `i < NPAR − 1`: `(i, j) → (i+1, j)`.

Da cui:

```
nConnections = NPAR·NMER + (NPAR−1)·NMER = 288 + 264 = 552
```

### Rotazioni 3D

Le rotazioni sono implementate come matrici di rotazione elementari applicate componente per componente (nessuna libreria di algebra lineare).

**Attorno a Y** (`rotateYVec3`):

```
x' =  x·cos α + z·sin α
y' =  y
z' = −x·sin α + z·cos α
```

**Attorno a X** (`rotateXVec3`):

```
x' =  x
y' =  y·cos α − z·sin α
z' =  y·sin α + z·cos α
```

**Attorno a Z** (`rotateZVec3`):

```
x' =  x·cos α − y·sin α
y' =  x·sin α + y·cos α
z' =  z
```

Nel rendering l'ordine di composizione è **fisso**: prima yaw attorno a `y`, poi pitch attorno a `x`.

```
p_rot = Rx(xRotationAngle) · Ry(yRotationAngle) · p
```

Gli angoli di camera sono accumulati dal delta del mouse (`xrel`, `yrel`) moltiplicato per la sensibilità `rotationSense = 0.01` rad/pixel, e riportati in `[0, 2π)` con `fmod` + correzione del segno, per evitare che crescano indefinitamente e perdano precisione.

### Proiezione prospettica

Proiezione prospettica classica con centro di proiezione sull'asse `z` a distanza `PRESPECTIVE = 3.0`:

```
x_2D = x / (z + d)
y_2D = y / (z + d)          con d = 3.0
```

Poiché la sfera ha raggio 1, il denominatore resta in `[2, 4]`: mai zero, nessuna divisione singolare e nessun punto dietro il piano di proiezione. Segue la mappatura in coordinate schermo, con inversione dell'asse verticale (in SDL `y` cresce verso il basso):

```
X = round(x_2D · SCALE + xOffset)
Y = round(yOffset − y_2D · SCALE)
```

con `SCALE = 600` e offset inizialmente al centro finestra (`1000×900`), ricalcolati quando si passa a fullscreen.

### Ordinamento in profondità (back/front)

Non esistendo z-buffer, la profondità è gestita con un **painter's algorithm** semplificato in due passate:

1. `drawSphere(&sphere, false)` disegna gli spigoli con `zMid ≤ 0` (emisfero posteriore) in grigio scuro;
2. si disegna il vettore di stato;
3. `drawSphere(&sphere, true)` disegna gli spigoli con `zMid > 0` (emisfero anteriore) in bianco semi-trasparente.

Il criterio di appartenenza è il punto medio in `z` dello spigolo dopo la rotazione. Il risultato è che il vettore di stato appare correttamente davanti alla faccia posteriore e dietro a quella anteriore.

### Linee spesse

SDL2 disegna solo linee di 1 pixel. `drawThickLine` costruisce una linea spessa **ricorsivamente**: ad ogni livello disegna due linee parallele offsettate di `±(t−1)` pixel e richiama sé stessa con `t−1`, fino al caso base `t = 0`. L'offset è applicato sull'asse *ortogonale* a quello dominante (`|dx| > |dy|` → offset verticale, altrimenti orizzontale), così che lo spessore risulti perpendicolare alla direzione della linea.

### Gate quantistici

I gate di Pauli sono rotazioni di **π** attorno agli assi della sfera di Bloch, e sono implementati come trasformazioni in forma chiusa direttamente sugli angoli `(θ, φ)` — senza passare per le cartesiane:

| Gate | Trasformazione sugli angoli | Effetto sul vettore di Bloch |
|---|---|---|
| **Z** | `φ → (φ + π) mod 2π` | `(x, y, z) → (−x, y, −z)` — rotazione di π attorno all'asse polare |
| **X** | `θ → π − θ`, `φ → −φ` (riportato in `[0, 2π)`) | `(x, y, z) → (x, −y, −z)` — rotazione di π attorno all'asse `x` |
| **Y** | `θ → π − θ`, `φ → π − φ` | `(x, y, z) → (−x, −y, z)` — rotazione di π attorno all'asse `z` del codice (= asse Y di Bloch) |
| **Reset** (`R`) | `θ = 0`, `φ = 0` | riporta lo stato a \|0⟩ |

Verifica per il gate X: con `θ → π − θ` si ha `cos θ → −cos θ` (quindi `y → −y`) mentre `sin θ` è invariato; con `φ → −φ` si ha `cos φ` invariato (`x → x`) e `sin φ → −sin φ` (`z → −z`). Le tre componenti confermano la rotazione di π attorno a `x`.

Le **rotazioni di 45°** (`apply45RotationGate`, attivate con `Shift`) seguono invece il percorso generale, perché non hanno una forma chiusa altrettanto compatta sugli angoli:

```
polare → cartesiano → matrice di rotazione di π/4 → cartesiano → polare
```

La mappatura asse-logico → asse-codice rispetta la convenzione della tabella sopra: `'X'` usa `rotateXVec3`, `'Y'` usa `rotateZVec3`, `'Z'` usa `rotateYVec3`.

> Nota sulla convenzione degli angoli: una rotazione di `α` del **vettore di Bloch** corrisponde a un gate `R_n(α) = exp(−i·α·σ_n/2)`, cioè a un angolo `α/2` nello spazio di Hilbert. Le rotazioni di 45° qui implementate corrispondono quindi ai gate `R(π/4)`, ovvero — per l'asse Z — al gate **T**.

---

## Installazione e compilazione

### Dipendenze

- un compilatore C (C99 o superiore): **GCC**, **Clang** o **MinGW-w64**
- **SDL2** (libreria di sviluppo, non solo il runtime)
- **libm** (libreria matematica standard, generalmente già presente)
- `make` (opzionale: il comando di compilazione è una riga sola)

### Clonare il repository

```bash
git clone https://github.com/teocnt/CBlochSphere.git
cd CBlochSphere
```

### Compilazione con il Makefile

Il `Makefile` incluso è configurato per **macOS su Apple Silicon** (percorsi Homebrew `/opt/homebrew`):

```makefile
CC      = gcc
CFLAGS  = -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lSDL2
TARGET  = sbl
```

```bash
make          # produce l'eseguibile ./sbl
make clean    # rimuove l'eseguibile
```

Su altre piattaforme è sufficiente sovrascrivere le variabili da riga di comando, senza modificare il file:

```bash
make CFLAGS="$(sdl2-config --cflags)" LDFLAGS="$(sdl2-config --libs) -lm"
```

### Compilazione manuale (portabile)

Il modo più robusto su qualunque sistema è lasciare che sia `sdl2-config` (o `pkg-config`) a risolvere i percorsi:

```bash
gcc sferaBL.c -o sbl $(sdl2-config --cflags) $(sdl2-config --libs) -lm
```

oppure:

```bash
gcc sferaBL.c -o sbl $(pkg-config --cflags --libs sdl2) -lm
```

Flag consigliati in sviluppo:

```bash
gcc -std=c99 -Wall -Wextra -O2 sferaBL.c -o sbl $(sdl2-config --cflags --libs) -lm
```

- `-lm` è **obbligatorio** su Linux con glibc meno recenti (`sin`, `cos`, `acos`, `atan2`, `fmod`, `round`); su macOS è innocuo perché libm fa parte di libSystem.
- `-O2` è consigliato: il loop di rendering ricalcola 288 punti e 552 spigoli due volte per frame.

### Esecuzione

```bash
./sbl
```

Il programma non accetta argomenti da riga di comando e apre direttamente una finestra `1000×900`.

---

## Compatibilità tra sistemi operativi

Il codice è **portabile a livello sorgente**: usa solo C standard, `math.h` e SDL2, senza chiamate di sistema specifiche. L'unico punto non portabile del repository è il `Makefile`, tarato su Homebrew ARM.

### macOS

**Apple Silicon (M1/M2/M3/M4)** — configurazione predefinita del Makefile.

```bash
brew install sdl2
make
```

Homebrew installa in `/opt/homebrew`, esattamente i percorsi già presenti nel Makefile.

**Intel (x86_64)** — Homebrew installa in `/usr/local`, quindi i percorsi vanno sovrascritti:

```bash
brew install sdl2
make CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib -lSDL2"
```

Note:

- su macOS `gcc` è tipicamente un alias di **Clang**; il codice compila correttamente con entrambi;
- `M_PI` è disponibile di default in `math.h` su macOS;
- se si vuole distribuire un `.app`, SDL2 richiede che il main sia gestito da `SDLmain`; per l'uso da terminale come qui non serve nulla di particolare;
- al primo avvio macOS può richiedere permessi di accesso allo schermo o mostrare un avviso Gatekeeper per un binario non firmato.

### Linux

Piattaforma di riferimento più semplice. Installazione dei pacchetti di sviluppo:

```bash
# Debian / Ubuntu / Linux Mint
sudo apt install build-essential libsdl2-dev

# Fedora / RHEL
sudo dnf install gcc make SDL2-devel

# Arch / Manjaro
sudo pacman -S base-devel sdl2

# openSUSE
sudo zypper install gcc make libSDL2-devel
```

Compilazione:

```bash
gcc sferaBL.c -o sbl $(sdl2-config --cflags --libs) -lm
```

Note:

- `-lm` è necessario su distribuzioni con glibc < 2.35 (con glibc ≥ 2.35 libm è fusa in libc, ma il flag resta innocuo);
- `M_PI` è esposto da `math.h` con le estensioni POSIX/GNU attive per default; compilando con `-std=c99` **stretto** potrebbe non esserlo — usare `-std=gnu99` oppure `-D_GNU_SOURCE`;
- su **Wayland** SDL2 funziona sia nativamente sia via XWayland; in caso di problemi di rendering forzare il backend con `SDL_VIDEODRIVER=x11 ./sbl`;
- il renderer è creato con `SDL_RENDERER_ACCELERATED`: su macchine senza driver GPU adeguati (VM, server headless) la creazione può fallire e il programma esce con messaggio d'errore. In quel caso si può forzare il fallback software con `SDL_RENDER_DRIVER=software ./sbl`;
- il fullscreen usa `SDL_WINDOW_FULLSCREEN_DESKTOP`, che su Linux dipende dal window manager: alcuni WM tiling potrebbero ignorarlo.

### Windows

Nessun `Makefile` pronto: il percorso più semplice è **MSYS2 + MinGW-w64**.

```bash
# dalla shell "MSYS2 MINGW64"
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkg-config make
gcc sferaBL.c -o sbl.exe $(pkg-config --cflags --libs sdl2) -lm
```

Per un eseguibile che apra **anche** una console (utile, dato che il programma stampa lo stato su stdout) non aggiungere `-mwindows`. Con MinGW la funzione `main` viene rimappata da SDL su `SDL_main`: se si ottengono errori di link relativi a `WinMain`, aggiungere `-lmingw32 -lSDL2main -lSDL2` **in quest'ordine**.

Alternativa con **MinGW-w64 standalone** e SDL2 scaricata manualmente da <https://libsdl.org>:

```bash
gcc sferaBL.c -o sbl.exe -IC:\SDL2\include -LC:\SDL2\lib -lmingw32 -lSDL2main -lSDL2 -lm
```

`SDL2.dll` (dalla cartella `lib\x64`) va copiata accanto a `sbl.exe`, altrimenti il programma non parte.

**MSVC (Visual Studio) — richiede modifiche al sorgente**, per due incompatibilità note:

1. `M_PI` non è definita in `math.h` di MSVC senza `#define _USE_MATH_DEFINES` prima dell'`#include <math.h>` (o `/D_USE_MATH_DEFINES` da riga di comando);
2. `drawSphere` usa **VLA** (array a lunghezza variabile: `Vec2 projectedPoints[sphere->model.nPoints]`), feature C99 che MSVC non supporta. Servirebbe un array a dimensione fissa o `malloc`.

Per queste ragioni **MinGW/MSYS2 è la toolchain consigliata su Windows**. Funziona anche la compilazione via WSL2 seguendo le istruzioni Linux (con WSLg per la finestra grafica).

### Riepilogo compatibilità

| Sistema | Toolchain | Stato | Note |
|---|---|---|---|
| macOS Apple Silicon | Clang/GCC + Homebrew | ✅ configurazione predefinita | Makefile pronto |
| macOS Intel | Clang/GCC + Homebrew | ✅ | sovrascrivere i percorsi con `/usr/local` |
| Linux (X11) | GCC/Clang | ✅ | usare `sdl2-config`, aggiungere `-lm` |
| Linux (Wayland) | GCC/Clang | ✅ | eventuale fallback `SDL_VIDEODRIVER=x11` |
| Windows MSYS2/MinGW | GCC | ✅ | ordine dei flag di link, `SDL2.dll` a fianco |
| Windows MSVC | cl.exe | ⚠️ | serve `_USE_MATH_DEFINES` e rimozione dei VLA |
| WSL2 | GCC | ✅ | richiede WSLg o un X server |
| BSD | Clang | ✅ (non testato) | `pkg-config sdl2` |

---

## Struttura del progetto

```
.
├── Makefile      # build a singola regola, percorsi Homebrew ARM
├── sferaBL.c     # sorgente unico: modello, matematica, rendering, input
├── .gitignore    # ignora l'eseguibile "sbl"
└── README.md
```

Il sorgente è organizzato in blocchi funzionali:

- **tipi** — `Polar2`, `Vec2`, `Vec3`, `Connection`, `Model`, `Sphere`;
- **conversioni** — `fromPolarToCartesian`, `fromCartesianToPolar`;
- **costruzione del modello** — `calculateSphereModel`, `calculateSpherePoints`, `calculateSphereConnections`;
- **algebra 3D** — `rotateXVec3`, `rotateYVec3`, `rotateZVec3`;
- **rendering** — `projectsP3`, `drawSphere`, `drawSystemVector`, `drawThickLine`;
- **gate quantistici** — `applyXGate`, `applyYGate`, `applyZGate`, `apply45RotationGate`, `resetState`;
- **main** — inizializzazione SDL, event loop, ciclo di disegno.

Parametri modificabili tramite `#define` in testa al file:

| Macro | Valore | Significato |
|---|---|---|
| `WINDOW_WIDTH` / `WINDOW_HEIGHT` | 1000 / 900 | dimensioni finestra |
| `SCALE` | 600 | fattore di scala della proiezione |
| `PRESPECTIVE` | 3.0 | distanza del centro di proiezione |
| `NPAR` | 12 | numero di paralleli |
| `NMER` | 24 | numero di meridiani |
| `DELAY` | 20 | ritardo di frame (dichiarato, attualmente non usato) |
| `RADIUS` | 1 | raggio della sfera |

---

## Note e limiti noti

- **Poli degeneri**: con `θ = 0` e `θ = π` i `NMER` punti di un parallelo coincidono nello stesso vertice, quindi ai poli vengono disegnati archi di lunghezza nulla. È un costo trascurabile ma spiega perché il numero di vertici effettivamente distinti è `(NPAR−2)·NMER + 2 = 242` anziché 288.
- **Loop non limitato in frequenza**: la macro `DELAY` è definita ma non viene mai usata (nessuna `SDL_Delay`) e il renderer non usa VSync, quindi il ciclo gira alla massima velocità possibile con conseguente uso intensivo di CPU/GPU. Aggiungere `SDL_RENDERER_PRESENTVSYNC` o una `SDL_Delay(DELAY)` risolverebbe.
- **Calcolo di `zMid` in `drawSphere`**: `z1` e `z2` leggono entrambi `rotatedPoints[tmpC.start].z`, quindi il test di profondità di fatto usa solo il punto iniziale dello spigolo invece del punto medio. In pratica l'effetto visivo è quasi identico (gli spigoli sono corti), ma sugli archi a cavallo dell'orizzonte la classificazione front/back può risultare leggermente imprecisa.
- **Titolo della finestra**: è ancora `"rotating cube"`, residuo del progetto da cui deriva il motore di rendering.
- **Memoria**: `sphereData.model.points` e `.connections` sono allocati con `malloc` e non liberati prima dell'uscita; il campo `rotatedPoints` della struct `Model` non viene mai usato (il rendering usa un VLA locale). Nessun impatto pratico, il sistema operativo recupera tutto alla terminazione.
- **Modello fisico**: viene rappresentato solo lo **stato puro** di un qubit singolo (vettore sulla superficie della sfera). Stati misti, che vivono all'interno della sfera, e sistemi multi-qubit non sono modellati.
