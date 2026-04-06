# Snake semplice per PS1 (homebrew)

Progetto di partenza per un gioco stile Snake su PlayStation 1, scritto in C.

## Requisiti

- `psn00bsdk` installato
- Toolchain MIPS (`mipsel-none-elf-gcc`, `objcopy`)
- Un emulatore PS1 (es. DuckStation)

## Struttura

- `src/main.c`: logica di gioco + rendering 2D a tile
- `Makefile`: build base in `.exe` PS1 + `.bin`
- `build.ps1`: build alternativa per Windows senza `make`

## Build (Windows / PowerShell)

1. Imposta variabili ambiente in base al tuo path:

```powershell
$env:PSN00BSDK_LIBS = "C:/psn00bsdk/lib"
$env:PSN00BSDK_INCLUDE = "C:/psn00bsdk/include"
```

2. Compila (metodo consigliato su Windows, senza `make`):

```powershell
.\build.ps1
```

3. Oppure compila con Makefile (se hai `make` installato):

```powershell
make
```

Genera:

- `snake_ps1.exe`
- `snake_ps1.bin`

## Errore: make non riconosciuto

Se vedi `make : Termine 'make' non riconosciuto`, su Windows hai due opzioni:

1. Usa direttamente lo script PowerShell:

```powershell
.\build.ps1
```

2. Installa `make` (ad esempio tramite MSYS2), poi riapri PowerShell.

Inoltre assicurati che questi eseguibili siano nel `PATH`:

- `mipsel-none-elf-gcc`
- `mipsel-none-elf-objcopy`

Verifica rapida:

```powershell
Get-Command mipsel-none-elf-gcc,mipsel-none-elf-objcopy
```

Se non vengono trovati, aggiungi al `PATH` la cartella `bin` della toolchain PS1.

## Controlli

- D-pad: muovi il serpente
- Start: riavvia dopo game over

## Note

- Questa e una base minima: niente audio, niente testo su schermo.
- Passo successivo consigliato: aggiungere punteggio e schermata title.
