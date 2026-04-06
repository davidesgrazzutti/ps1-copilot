param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$target = "snake_ps1"
$src = "src/main.c"
$obj = "src/main.o"

$sdkRoot = $env:PSN00BSDK_ROOT
if ([string]::IsNullOrWhiteSpace($sdkRoot)) {
    $sdkRoot = "C:/psn00bsdk"
}

$includePath = $env:PSN00BSDK_INCLUDE
if ([string]::IsNullOrWhiteSpace($includePath)) {
    $includePath = Join-Path $sdkRoot "include/libpsn00b"
}
if (-not (Test-Path (Join-Path $includePath "psxgpu.h"))) {
    $altInclude = Join-Path $sdkRoot "include"
    if (Test-Path (Join-Path $altInclude "libpsn00b/psxgpu.h")) {
        $includePath = Join-Path $altInclude "libpsn00b"
    }
}

$libPath = $env:PSN00BSDK_LIBS
if ([string]::IsNullOrWhiteSpace($libPath)) {
    $libPath = Join-Path $sdkRoot "lib/libpsn00b/release"
}

if (-not (Test-Path (Join-Path $libPath "libpsxgpu_exe_nogprel.a"))) {
    $altLibPath = Join-Path $libPath "libpsn00b/release"
    if (Test-Path (Join-Path $altLibPath "libpsxgpu_exe_nogprel.a")) {
        $libPath = $altLibPath
    }
}

$ldscript = Join-Path $sdkRoot "lib/libpsn00b/ldscripts/exe.ld"

$toolchainBin = $env:PSN00BSDK_TOOLCHAIN_BIN
if (-not [string]::IsNullOrWhiteSpace($toolchainBin)) {
    if (-not (Test-Path $toolchainBin)) {
        Write-Error "PSN00BSDK_TOOLCHAIN_BIN non valido: $toolchainBin"
    }
    $env:Path = "$toolchainBin;$env:Path"
}

$prefix = $env:PSN00BSDK_CC_PREFIX
if ([string]::IsNullOrWhiteSpace($prefix)) {
    $prefix = "mipsel-none-elf-"
}

$cc = "$($prefix)gcc"
$objcopy = "$($prefix)objcopy"
$elf2x = "elf2x"

if ($Clean) {
    Remove-Item -ErrorAction SilentlyContinue "$obj", "$target.elf", "$target.exe", "$target.bin", "$target.map"
    Write-Host "Pulizia completata."
    exit 0
}

if (-not (Get-Command $cc -ErrorAction SilentlyContinue)) {
    Write-Error "Compilatore non trovato: $cc. Aggiungi la cartella bin della toolchain al PATH o imposta PSN00BSDK_TOOLCHAIN_BIN."
}

if (-not (Get-Command $objcopy -ErrorAction SilentlyContinue)) {
    Write-Error "Objcopy non trovato: $objcopy. Aggiungi la cartella bin della toolchain al PATH o imposta PSN00BSDK_TOOLCHAIN_BIN."
}

if (-not (Get-Command $elf2x -ErrorAction SilentlyContinue)) {
    Write-Error "Strumento non trovato: elf2x. Aggiungi C:/psn00bsdk/bin al PATH o imposta PSN00BSDK_TOOLCHAIN_BIN."
}

if (-not (Test-Path $includePath)) {
    Write-Error "Include path non valido: $includePath"
}

if (-not (Test-Path $libPath)) {
    Write-Error "Library path non valido: $libPath"
}

if (-not (Test-Path $ldscript)) {
    Write-Error "Linker script non trovato: $ldscript"
}

$compileArgs = @(
    "-O2", "-G0", "-Wall", "-Wextra", "-Wa,--strip-local-absolute",
    "-ffreestanding", "-fno-builtin", "-nostdlib", "-fdata-sections", "-ffunction-sections",
    "-fsigned-char", "-fno-strict-overflow", "-msoft-float", "-march=r3000", "-mtune=r3000",
    "-mabi=32", "-mno-mt", "-mno-llsc", "-fno-pic", "-mno-abicalls", "-mno-gpopt",
    "-Iinclude", "-I$includePath", "-c", $src, "-o", $obj
)

& $cc @compileArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$linkArgs = @(
    $obj,
    "-nostdlib", "-Wl,-gc-sections", "-G0", "-static",
    "-T$ldscript", "-Wl,-Map=$target.map",
    (Join-Path $libPath "libpsxgpu_exe_nogprel.a"),
    (Join-Path $libPath "libpsxgte_exe_nogprel.a"),
    (Join-Path $libPath "libpsxspu_exe_nogprel.a"),
    (Join-Path $libPath "libpsxcd_exe_nogprel.a"),
    (Join-Path $libPath "libpsxpress_exe_nogprel.a"),
    (Join-Path $libPath "libpsxsio_exe_nogprel.a"),
    (Join-Path $libPath "libpsxetc_exe_nogprel.a"),
    (Join-Path $libPath "libpsxapi_exe_nogprel.a"),
    (Join-Path $libPath "libsmd_exe_nogprel.a"),
    (Join-Path $libPath "liblzp_exe_nogprel.a"),
    (Join-Path $libPath "libc_exe_nogprel.a"),
    "-lgcc",
    "-o", "$target.elf"
)

& $cc @linkArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $elf2x -q "$target.elf" "$target.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $objcopy -O binary "$target.elf" "$target.bin"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build completata: $target.elf, $target.exe e $target.bin"
