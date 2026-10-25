$ErrorActionPreference='Stop'
if (!$env:MHGU_GAME -or !$env:MHGU_NDK_BIN) { throw 'Configure MHGU_GAME e MHGU_NDK_BIN; veja BUILDING.md.' }
python (Join-Path $PSScriptRoot 'tools/build.py')
if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação' }
python (Join-Path $PSScriptRoot 'tools/package.py')
if ($LASTEXITCODE -ne 0) { throw 'Falha na verificação do pacote' }
