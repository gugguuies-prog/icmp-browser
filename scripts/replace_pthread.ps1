# Script to replace pthread calls with thread_compat API
# Run from repository root

$files = Get-ChildItem -Path "src" -Include "*.c","*.cpp" -Recurse

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    $modified = $false
    
    # Replace pthread_mutex_init
    if ($content -match 'pthread_mutex_init\(&([^,]+),\s*NULL\)') {
        $content = $content -replace 'pthread_mutex_init\(&([^,]+),\s*NULL\)', 'mutex_init(&$1)'
        $modified = $true
    }
    
    # Replace pthread_mutex_destroy
    if ($content -match 'pthread_mutex_destroy') {
        $content = $content -replace 'pthread_mutex_destroy\(&([^\)]+)\)', 'mutex_destroy(&$1)'
        $modified = $true
    }
    
    # Replace pthread_mutex_lock
    if ($content -match 'pthread_mutex_lock') {
        $content = $content -replace 'pthread_mutex_lock\(&([^\)]+)\)', 'mutex_lock(&$1)'
        $modified = $true
    }
    
    # Replace pthread_mutex_unlock
    if ($content -match 'pthread_mutex_unlock') {
        $content = $content -replace 'pthread_mutex_unlock\(&([^\)]+)\)', 'mutex_unlock(&$1)'
        $modified = $true
    }
    
    # Replace pthread_mutex_trylock
    if ($content -match 'pthread_mutex_trylock') {
        $content = $content -replace 'pthread_mutex_trylock\(&([^\)]+)\)', 'mutex_trylock(&$1)'
        $modified = $true
    }
    
    if ($modified) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
        Write-Host "Updated: $($file.FullName)" -ForegroundColor Green
    }
}

Write-Host "`nDone! All pthread_mutex calls replaced with thread_compat API." -ForegroundColor Cyan
