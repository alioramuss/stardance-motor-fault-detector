# ===========================================================================
#  Serial to CSV logger for the motor fault detector.
#
#  Reads timestamped accelerometer data off the Arduino and writes it as CSV,
#  split into fixed-length chunks named so Edge Impulse can infer the label
#  from the filename.
#
#  Usage:
#     .\capture.ps1 -Label healthy
#     .\capture.ps1 -Label faulty -Seconds 60 -ChunkSeconds 5
#
#  Uses only System.IO.Ports, which ships with Windows — no extra software.
#  Close the Arduino IDE's Serial Monitor first; one program at a time can
#  hold the port.
# ===========================================================================

param(
  [Parameter(Mandatory=$true)]
  [string]$Label,                      # e.g. healthy, faulty, off

  [string]$Port         = "COM3",
  [int]   $Baud         = 115200,
  [int]   $Seconds      = 60,
  [int]   $ChunkSeconds = 5,
  [int]   $Rate         = 200,         # samples/sec the sketch is sending
  [string]$OutDir       = "data"
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$rowsPerChunk = $Rate * $ChunkSeconds
$chunks       = [math]::Floor($Seconds / $ChunkSeconds)

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, one
$sp.ReadTimeout = 5000
$sp.DtrEnable   = $true
$sp.NewLine     = "`n"

try { $sp.Open() }
catch {
  Write-Host "Could not open $Port." -ForegroundColor Red
  Write-Host "Close the Arduino Serial Monitor and the live dashboard, then try again."
  exit 1
}

Write-Host "Opened $Port. Letting the board settle..." -ForegroundColor Cyan
Start-Sleep -Seconds 2
$sp.DiscardInBuffer()

Write-Host "Recording $Seconds s as '$Label' -> $chunks files of $ChunkSeconds s" -ForegroundColor Cyan
Write-Host "Leave the machine running and don't touch the rig." -ForegroundColor DarkGray

for ($c = 1; $c -le $chunks; $c++) {
  $path = Join-Path $OutDir "$Label.$c.csv"
  $sw = [System.IO.StreamWriter]::new($path)
  $sw.WriteLine("ax,ay,az")

  $n = 0
  while ($n -lt $rowsPerChunk) {
    try { $line = $sp.ReadLine().Trim() } catch { break }
    if (-not $line -or $line.StartsWith("#") -or $line.StartsWith("timestamp")) { continue }

    $f = $line.Split(",")
    # the sketch prints timestamp,ax,ay,az — drop the timestamp
    if ($f.Count -eq 4) { $sw.WriteLine("$($f[1]),$($f[2]),$($f[3])"); $n++ }
    elseif ($f.Count -eq 3) { $sw.WriteLine($line); $n++ }
  }

  $sw.Close()
  Write-Host ("  {0,-24} {1} rows" -f "$Label.$c.csv", $n) -ForegroundColor Green
}

$sp.Close()

Write-Host ""
Write-Host "Done. $chunks files in $OutDir" -ForegroundColor Green
Write-Host "Upload them to Edge Impulse with 'infer label from filename'." -ForegroundColor DarkGray
