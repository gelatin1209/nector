param(
    [string]$CompilerRoot = (Join-Path $PSScriptRoot "..\TdmGcc"),
    [switch]$SkipTests,
    [switch]$Release
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$cxx = Join-Path $CompilerRoot "bin\g++.exe"
$cc = Join-Path $CompilerRoot "bin\gcc.exe"
if (-not (Test-Path $cxx) -or -not (Test-Path $cc)) {
    throw "TDM-GCC was not found under $CompilerRoot"
}

$build = Join-Path $root "build"
$objects = Join-Path $build "objects"
$monoObjects = Join-Path $build "mono_objects"
$testBin = Join-Path $build "tests\bin"
$dist = Join-Path $root "dist"
$library = Join-Path $dist "library"
$static = Join-Path $library "static"
$tools = $library
$assets = Join-Path $library "assets"
$distInclude = Join-Path $library "include"
$document = Join-Path $dist "document"
$licenses = Join-Path $dist "license"
$metadata = Join-Path $dist "metadata"
$locale = Join-Path $library "locale"
$cmakePackage = Join-Path $library "cmake"
$pkgConfig = Join-Path $library "pkgconfig"
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
foreach ($directory in @($build, $objects, $monoObjects, $testBin, $library, $static, $assets, $distInclude, $licenses, $metadata, $locale, $cmakePackage, $pkgConfig)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}
Remove-Item (Join-Path $library "tools") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $document "license") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $library "hello.exe") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $library "*.dll-nector") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $build "hello.dll-nector") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $distInclude "nector") -Recurse -Force -ErrorAction SilentlyContinue
$legacyLibraries = @(
    "nector.anim.dll", "nector.audio.dll", "nector.core.dll", "nector.font.dll",
    "nector.image.dll", "nector.input.dll", "nector.platform.dll", "nector.raster.dll",
    "nector.scene.dll", "nector_cdsl.dll"
)
foreach ($name in $legacyLibraries) {
    Remove-Item (Join-Path $library $name) -Force -ErrorAction SilentlyContinue
}
Remove-Item (Join-Path $static "Nector.Raster.dll.a") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $static "lib*.dll.a") -Force -ErrorAction SilentlyContinue

$cxxFlags = @("-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-static-libgcc", "-static-libstdc++", "-mthreads")
$includes = @("-I$root\include", "-I$root\third_party\zlib", "-I$root\third_party\libpng", "-I$root\third_party\libjpeg", "-I$root\third_party\brotli\c\include", "-I$root\third_party\miniaudio")

function Invoke-Compile {
    param([string]$Compiler, [string[]]$Arguments)
    & $Compiler @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "compiler failed with exit code $LASTEXITCODE"
    }
}

function Cpp-Object {
    param([string]$Source, [string]$Output, [string[]]$Definitions, [string[]]$ExtraFlags = @())
    $parent = Split-Path -Parent $Output
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $arguments = $cxxFlags + $includes + $Definitions + $ExtraFlags + @("-c", $Source, "-o", $Output)
    Invoke-Compile $cxx $arguments
}

function C-Object {
    param([string]$Source, [string]$Output)
    $parent = Split-Path -Parent $Output
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $arguments = @("-std=c99", "-O2", "-w") + $includes + @("-c", $Source, "-o", $Output)
    Invoke-Compile $cc $arguments
}

function Link-Dll {
    param([string]$Output, [string[]]$InputObjects, [string[]]$Libraries, [string]$ImportLibrary)
    $arguments = @("-shared", "-static-libgcc", "-static-libstdc++", "-mthreads") + $InputObjects + @("-o", $Output, "-Wl,--out-implib,$ImportLibrary") + $Libraries
    Invoke-Compile $cxx $arguments
}

function Link-Exe {
    param([string]$Output, [string[]]$InputObjects, [string[]]$Libraries)
    $arguments = @("-static-libgcc", "-static-libstdc++", "-mthreads") + $InputObjects + @("-o", $Output) + $Libraries
    Invoke-Compile $cxx $arguments
}

$thirdParty = @()
$cSources = @()
$cSources += Get-ChildItem "$root\third_party\zlib\*.c"
$cSources += Get-ChildItem "$root\third_party\libpng\*.c" | Where-Object { $_.Name -ne "example.c" -and $_.Name -ne "pngtest.c" }
$cSources += Get-ChildItem "$root\third_party\brotli\c\common\*.c"
$cSources += Get-ChildItem "$root\third_party\brotli\c\dec\*.c"
$jpegNames = @(
    "jcapimin", "jcapistd", "jcarith", "jccoefct", "jccolor", "jcdctmgr", "jchuff", "jcinit", "jcmainct", "jcmarker",
    "jcmaster", "jcomapi", "jcparam", "jcprepct", "jcsample", "jctrans", "jdapimin", "jdapistd", "jdarith", "jdatadst",
    "jdatasrc", "jdcoefct", "jdcolor", "jddctmgr", "jdhuff", "jdinput", "jdmainct", "jdmarker", "jdmaster", "jdmerge",
    "jdpostct", "jdsample", "jdtrans", "jerror", "jfdctflt", "jfdctfst", "jfdctint", "jidctflt", "jidctfst", "jidctint",
    "jquant1", "jquant2", "jutils", "jmemmgr", "jmemnobs", "jaricom"
)
$cSources += $jpegNames | ForEach-Object { Get-Item "$root\third_party\libjpeg\$_.c" }
foreach ($source in $cSources) {
    $name = ($source.FullName.Substring($root.Length + 1) -replace '[\\/]', '_') + ".o"
    $output = Join-Path $objects $name
    C-Object $source.FullName $output
    $thirdParty += $output
}

$miniaudioObject = Join-Path $objects "miniaudio_backend.o"
Cpp-Object "$root\src\audio\miniaudio_backend.cpp" $miniaudioObject @() @("-w")
$thirdParty += $miniaudioObject

$modules = [ordered]@{
    "core" = "Nector.Core"
    "rasterization" = "Nector.Rasterization"
    "font" = "Nector.Font"
    "image" = "Nector.Image"
    "nimation" = "NImation"
    "input" = "Nector.Input"
    "scene" = "Nector.Scene"
    "audio" = "Nector.Audio"
    "platform" = "Nector.Platform"
    "interface" = "Nector.Interface"
}
$moduleObjects = @{}
$importLibraries = @{}
$allDefines = @("-Dnector_core_export", "-Dnector_rasterization_export", "-Dnector_font_export", "-Dnector_image_export", "-Dnector_anim_export", "-Dnector_input_export", "-Dnector_scene_export", "-Dnector_audio_export", "-Dnector_platform_export", "-Dnector_interface_export", "-Dnector_cdsl_export")

foreach ($module in $modules.Keys) {
    $compiled = @()
    $monoCompiled = @()
    $sources = Get-ChildItem "$root\src\$module\*.cpp"
    if ($module -eq "audio") {
        $sources = $sources | Where-Object { $_.Name -ne "miniaudio_backend.cpp" }
    }
    $exportName = $module
    if ($module -eq "nimation") { $exportName = "anim" }
    foreach ($source in $sources) {
        $object = Join-Path $objects "$module`_$($source.BaseName).o"
        Cpp-Object $source.FullName $object @("-Dnector_${exportName}_export")
        $compiled += $object
        $monoObject = Join-Path $monoObjects "$module`_$($source.BaseName).o"
        Cpp-Object $source.FullName $monoObject $allDefines
        $monoCompiled += $monoObject
    }
    $moduleObjects[$module] = $compiled
    $moduleObjects["mono_$module"] = $monoCompiled
}

$systemLibraries = @("-luser32", "-lgdi32", "-lole32", "-lwinmm")
$moduleDependencies = [ordered]@{
    "core" = @()
    "rasterization" = @("core")
    "font" = @("core", "rasterization")
    "image" = @("core", "rasterization")
    "nimation" = @("core", "image")
    "input" = @("core")
    "scene" = @("core", "rasterization", "font", "image", "nimation", "input")
    "audio" = @("core")
    "platform" = @("core", "rasterization", "input", "scene")
    "interface" = @("core", "scene")
}
$moduleThirdParty = @{
    "core" = @()
    "rasterization" = @()
    "font" = @($thirdParty | Where-Object { $_ -match 'zlib|brotli' })
    "image" = @($thirdParty | Where-Object { $_ -match 'zlib|libpng|libjpeg' })
    "nimation" = @()
    "input" = @()
    "scene" = @()
    "audio" = @($miniaudioObject)
    "platform" = @()
    "interface" = @()
}

foreach ($module in $moduleDependencies.Keys) {
    $libraries = @()
    foreach ($dependency in $moduleDependencies[$module]) {
        $libraries += Join-Path $static "$($modules[$dependency]).dll.a"
    }
    $libraries += $moduleThirdParty[$module]
    if ($module -eq "platform" -or $module -eq "audio") { $libraries += $systemLibraries }
    $import = Join-Path $static "$($modules[$module]).dll.a"
    Link-Dll (Join-Path $library "$($modules[$module]).dll") $moduleObjects[$module] $libraries $import
    $importLibraries[$module] = $import
}

$monoInput = @()
foreach ($module in $modules.Keys) {
    $monoInput += $moduleObjects["mono_$module"]
}
$monoInput += $thirdParty
Link-Dll (Join-Path $library "Nector.dll") $monoInput $systemLibraries (Join-Path $static "Nector.dll.a")

$cdslObjects = @()
foreach ($source in Get-ChildItem "$root\src\dsl_compile\*.cpp") {
    $object = Join-Path $objects "dsl_compile_$($source.BaseName).o"
    Cpp-Object $source.FullName $object @("-Dnector_cdsl_export")
    $cdslObjects += $object
}
Link-Dll (Join-Path $library "Nector.DslCompile.dll") $cdslObjects @((Join-Path $static "Nector.dll.a")) (Join-Path $static "Nector.DslCompile.dll.a")

Copy-Item "$root\include\*" $distInclude -Force
Copy-Item "$root\packaging\document\*" $document -Recurse -Force
Copy-Item "$root\packaging\license\*" $licenses -Recurse -Force
Copy-Item "$root\packaging\metadata\*" $metadata -Recurse -Force
Copy-Item "$root\packaging\locale\*" $locale -Recurse -Force
Copy-Item "$root\packaging\cmake\*" $cmakePackage -Recurse -Force
Copy-Item "$root\packaging\pkgconfig\*" $pkgConfig -Recurse -Force
if ($Release) {
    $buildTarget = (& $cxx -dumpmachine).Trim()
    $compilerVersion = (& $cxx --version | Select-Object -First 1).Trim()
    $buildInfo = @(
        "Nector 0.1.0"
        "ABI version: 0"
        "Package layout: 1"
        "Platform: Windows"
        "Target: $buildTarget"
        "Compiler: $compilerVersion"
        "C++ standard: C++17"
    )
    [System.IO.File]::WriteAllLines((Join-Path $library "build-info.txt"), $buildInfo, $utf8NoBom)
}
$licenseFiles = [ordered]@{
    "third_party\zlib\LICENSE" = "zlib.LICENSE.txt"
    "third_party\libpng\LICENSE" = "libpng.LICENSE.txt"
    "third_party\libjpeg\LICENSE" = "libjpeg.LICENSE.txt"
    "third_party\brotli\LICENSE" = "brotli.LICENSE.txt"
    "third_party\miniaudio\LICENSE" = "miniaudio.LICENSE.txt"
    "third_party\fonts\LICENSE.dejavu.txt" = "dejavu.LICENSE.txt"
    "third_party\fonts\LICENSE.noto_sans.txt" = "noto_sans.LICENSE.txt"
    "third_party\fonts\LICENSE.source_han_serif.txt" = "source_han_serif.LICENSE.txt"
    "third_party\fonts\LICENSE.source_han_sans.txt" = "source_han_sans.LICENSE.txt"
    "third_party\fonts\LICENSE.noto_sans_cjk.txt" = "noto_sans_cjk.LICENSE.txt"
    "third_party\fonts\LICENSE.comic_pro_atlas.txt" = "comic_pro_atlas.LICENSE.txt"
    "tests\LICENSE.roboto.txt" = "roboto.LICENSE.txt"
    "tests\LICENSE.source_code_pro.txt" = "source_code_pro.LICENSE.txt"
}
foreach ($license in $licenseFiles.Keys) {
    Copy-Item (Join-Path $root $license) (Join-Path $licenses $licenseFiles[$license]) -Force
}
$fontFiles = @(
    "third_party\fonts\deja_vu_sans.ttf",
    "third_party\fonts\noto_sans.ttf",
    "third_party\fonts\source_han_serif_sc.otf",
    "third_party\fonts\source_han_sans_sc.otf",
    "third_party\fonts\noto_sans_cjk_sc.otf",
    "third_party\fonts\comic_pro_atlas.ttf",
    "tests\source_code_pro.otf",
    "tests\source_code_pro.otc",
    "tests\roboto.woff",
    "tests\roboto.woff2"
)
foreach ($font in $fontFiles) {
    Copy-Item (Join-Path $root $font) $assets -Force
}

$programs = @{
    "nectorc" = "$root\examples\nectorc.cpp"
    "ndvwr" = "$root\src\ndvwr\ndvwr.cpp"
    "nddbg" = "$root\src\nddbg\nddbg.cpp"
    "test_graphics" = "$root\tests\test_graphics.cpp"
    "test_rasterization" = "$root\tests\test_rasterization.cpp"
    "test_font_layout" = "$root\tests\test_font_layout.cpp"
    "test_dsl" = "$root\tests\test_dsl.cpp"
    "test_nddbg" = "$root\tests\test_nddbg.cpp"
    "test_media" = "$root\tests\test_media.cpp"
    "test_font_faces" = "$root\tests\test_font_faces.cpp"
    "test_scene" = "$root\tests\test_scene.cpp"
    "test_raii" = "$root\tests\test_raii.cpp"
    "test_platform" = "$root\tests\test_platform.cpp"
    "test_invalid_inputs" = "$root\tests\test_invalid_inputs.cpp"
    "test_composite" = "$root\tests\test_composite.cpp"
}
foreach ($source in Get-ChildItem "$root\tests\test_*.cpp" | Where-Object { $_.BaseName -match '^test_(nimation|input|interface)' }) {
    $programs[$source.BaseName] = $source.FullName
    $programs["$($source.BaseName)_modular"] = $source.FullName
}
foreach ($program in $programs.Keys) {
    $object = Join-Path $objects "program_$program.o"
    Cpp-Object $programs[$program] $object @()
    $inputObjects = @($object)
    if ($program -eq "nddbg") {
        foreach ($name in @("package_inspector", "record_decoder", "decompiler", "report", "hex_dump")) {
            $helperObject = Join-Path $objects "program_nddbg_$name.o"
            Cpp-Object "$root\src\nddbg\$name.cpp" $helperObject @()
            $inputObjects += $helperObject
        }
    }
    $libraries = @((Join-Path $static "Nector.dll.a"))
    if ($program.EndsWith("_modular")) {
        $libraries = @()
        foreach ($module in $modules.Keys) {
            $libraries += $importLibraries[$module]
        }
    }
    if ($program -eq "test_platform") {
        $libraries = @((Join-Path $static "Nector.Platform.dll.a"), (Join-Path $static "Nector.Scene.dll.a"), (Join-Path $static "Nector.Core.dll.a"))
    }
    if ($program -eq "nddbg") {
        $libraries = @((Join-Path $static "Nector.Core.dll.a"))
    }
    if ($program -eq "nectorc" -or $program -eq "ndvwr" -or $program -eq "test_dsl" -or $program -eq "test_nddbg" -or $program -eq "test_raii") {
        $libraries += (Join-Path $static "Nector.DslCompile.dll.a")
    }
    if ($program -eq "ndvwr" -or $program -eq "test_platform") { $libraries += $systemLibraries }
    $outputDirectory = $tools
    if ($program.StartsWith("test_")) { $outputDirectory = $testBin }
    Link-Exe (Join-Path $outputDirectory "$program.exe") $inputObjects $libraries
}

$manifestFiles = @()
foreach ($file in Get-ChildItem $dist -Recurse -File | Sort-Object FullName) {
    $relativePath = $file.FullName.Substring($dist.Length + 1).Replace('\', '/')
    if ($relativePath -eq "metadata/manifest.json" -or $relativePath -eq "metadata/checksums.sha256") { continue }
    $manifestFiles += [ordered]@{
        path = $relativePath
        size = $file.Length
        sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$manifest = [ordered]@{
    name = "Nector"
    version = "0.1.0"
    abi_version = 0
    package_layout = 1
    files = $manifestFiles
}
$manifestJson = ($manifest | ConvertTo-Json -Depth 5) + "`n"
[System.IO.File]::WriteAllText((Join-Path $metadata "manifest.json"), $manifestJson, $utf8NoBom)
$checksumLines = @()
foreach ($file in Get-ChildItem $dist -Recurse -File | Sort-Object FullName) {
    $relativePath = $file.FullName.Substring($dist.Length + 1).Replace('\', '/')
    if ($relativePath -eq "metadata/checksums.sha256") { continue }
    $hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $checksumLines += "$hash  $relativePath"
}
[System.IO.File]::WriteAllLines((Join-Path $metadata "checksums.sha256"), $checksumLines, $utf8NoBom)

if (-not $SkipTests) {
    $ttcFont = Join-Path $env:WINDIR "Fonts\simsun.ttc"
    $env:PATH = "$library;$env:PATH"
    Push-Location $root
    try {
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\deja_vu_sans.ttf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics TTF failed" }
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\noto_sans.ttf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics Noto Sans failed" }
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\source_han_serif_sc.otf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics Source Han Serif failed" }
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\source_han_sans_sc.otf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics Source Han Sans failed" }
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\noto_sans_cjk_sc.otf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics Noto Sans CJK failed" }
        & "$testBin\test_graphics.exe" "$root\third_party\fonts\comic_pro_atlas.ttf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics Comic Pro Atlas TTF failed" }
        & "$testBin\test_graphics.exe" "$root\tests\source_code_pro.otf" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics OTF failed" }
        & "$testBin\test_graphics.exe" "$root\tests\roboto.woff" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics WOFF failed" }
        & "$testBin\test_graphics.exe" "$root\tests\roboto.woff2" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics WOFF2 failed" }
        & "$testBin\test_graphics.exe" $ttcFont "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics TTC failed" }
        & "$testBin\test_graphics.exe" "$root\tests\source_code_pro.otc" "$root\tests\test.jpg"
        if ($LASTEXITCODE -ne 0) { throw "test_graphics OTC failed" }
        & "$testBin\test_font_faces.exe" "$root\tests\source_code_pro.otc"
        if ($LASTEXITCODE -ne 0) { throw "test_font_faces OTC failed" }
        & "$testBin\test_font_faces.exe" $ttcFont
        if ($LASTEXITCODE -ne 0) { throw "test_font_faces TTC failed" }
        & "$testBin\test_rasterization.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_rasterization failed" }
        & "$testBin\test_font_layout.exe" "$root\third_party\fonts\deja_vu_sans.ttf"
        if ($LASTEXITCODE -ne 0) { throw "test_font_layout failed" }
        & "$testBin\test_invalid_inputs.exe" "$root\third_party\fonts\deja_vu_sans.ttf"
        if ($LASTEXITCODE -ne 0) { throw "test_invalid_inputs failed" }
        & "$testBin\test_composite.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_composite failed" }
        & "$testBin\test_dsl.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_dsl failed" }
        & "$testBin\test_nddbg.exe" "$tools\nddbg.exe" "$tools\ndvwr.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_nddbg failed" }
        & "$testBin\test_scene.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_scene failed" }
        & "$testBin\test_raii.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_raii failed" }
        & "$testBin\test_platform.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_platform failed" }
        & "$testBin\test_media.exe"
        if ($LASTEXITCODE -ne 0) { throw "test_media failed" }
        foreach ($source in Get-ChildItem "$root\tests\test_*.cpp" | Where-Object { $_.BaseName -match '^test_(nimation|input|interface)' }) {
            & "$testBin\$($source.BaseName).exe"
            if ($LASTEXITCODE -ne 0) { throw "$($source.BaseName) failed" }
            & "$testBin\$($source.BaseName)_modular.exe"
            if ($LASTEXITCODE -ne 0) { throw "$($source.BaseName)_modular failed" }
        }
        & "$tools\nectorc.exe" "$root\examples\ui.nector" "$root\build\ui.dll-nector"
        if ($LASTEXITCODE -ne 0) { throw "nectorc failed" }
    } finally {
        Pop-Location
    }
}

Write-Host "Nector build completed: $library"
