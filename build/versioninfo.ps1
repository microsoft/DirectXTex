param(
[string]$version
)
$versionComma = $version.Replace(".", ",")
$files = 'Texassemble\texassemble.rc', 'Texconv\Texconv.rc', 'Texdiag\texdiag.rc'
foreach ($file in $files) { (Get-Content $file).replace('1,0,0,0', $versionComma).replace('1.0.0.0', $version) | Set-Content $file }
