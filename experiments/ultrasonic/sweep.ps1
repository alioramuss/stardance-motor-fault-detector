# ===========================================================================
#  Angle-sweep collector for the non-contact sensing experiment.
#
#  Sends 'U' to the rig, then splits the stream by angle into two files each:
#     <condition>.a<angle>.acc.csv    ax,ay,az at 200 Hz
#     <condition>.a<angle>.dist.csv   t_ms,mm  at ~25 Hz  (-1 = no echo)
#
#  Run it twice — once with the fan clean, once with the weight on:
#     .\sweep.ps1 -Condition healthy
#     .\sweep.ps1 -Condition faulty
#
#  Close the Arduino Serial Monitor and the live dashboard first.
# ===========================================================================

param(
  [Parameter(Mandatory=$true)]
  [ValidateSet("healthy","faulty")]
  [string]$Condition,

  [string]$Port   = "COM3",
  [int]   $Baud   = 115200,
  [string]$OutDir = "$env:USERPROFILE\stardance\sweep"
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, one
$sp.ReadTimeout = 20000
$sp.DtrEnable   = $true
$sp.NewLine     = "`n"

try { $sp.Open() }
catch { Write-Host "Could not open $Port - Serial Monitor or dashboard still holding it?" -ForegroundColor Red; exit 1 }

Write-Host "Opened $Port. Waiting for the rig to boot..." -ForegroundColor Cyan
Start-Sleep -Seconds 3
$sp.DiscardInBuffer()
$sp.Write("U")

$acc = $null; $dist = $null
$angle = ""; $nAcc = 0; $nDist = 0; $nMiss = 0
$report = @()

while ($true) {
  try { $line = $sp.ReadLine().Trim() } catch { Write-Host "Serial timed out." -ForegroundColor Yellow; break }
  if (-not $line) { continue }

  if ($line.StartsWith("#")) {
    $p = $line.Split(",")
    switch ($p[0]) {

      "#ANGLE" {
        $angle = $p[2]
        if ($acc)  { $acc.Close() }
        if ($dist) { $dist.Close() }
        $acc  = [System.IO.StreamWriter]::new("$OutDir\$Condition.a$angle.acc.csv")
        $dist = [System.IO.StreamWriter]::new("$OutDir\$Condition.a$angle.dist.csv")
        $acc.WriteLine("ax,ay,az")
        $dist.WriteLine("t_ms,mm")
        $nAcc = 0; $nDist = 0; $nMiss = 0
        Write-Host ("angle {0,3} deg  recording..." -f $angle) -ForegroundColor Cyan
      }

      "#D" {
        if ($dist) {
          $dist.WriteLine("$($p[1]),$($p[2])")
          $nDist++
          if ([int]$p[2] -lt 0) { $nMiss++ }
        }
      }

      "#ENDANGLE" {
        if ($acc)  { $acc.Close();  $acc  = $null }
        if ($dist) { $dist.Close(); $dist = $null }
        $missPct = if ($nDist) { [math]::Round(100 * $nMiss / $nDist, 1) } else { 0 }
        Write-Host ("               {0} accel rows, {1} pings, {2}% missed echoes" -f $nAcc, $nDist, $missPct) `
          -ForegroundColor $(if ($missPct -gt 20) { "Yellow" } else { "DarkGray" })
        $report += [pscustomobject]@{ angle=$angle; accel_rows=$nAcc; pings=$nDist; missed_pct=$missPct }
      }

      "#SWEEP" { Write-Host $line -ForegroundColor Cyan
                 if ($p[1] -eq "done") { break } }
      default  { }
    }
    continue
  }

  if ($acc) {
    $f = $line.Split(",")
    if ($f.Count -eq 4) { $acc.WriteLine("$($f[1]),$($f[2]),$($f[3])"); $nAcc++ }
  }
}

if ($acc)  { $acc.Close() }
if ($dist) { $dist.Close() }
$sp.Close()

Write-Host ""
Write-Host "Done. $Condition sweep written to $OutDir" -ForegroundColor Green
$report | Format-Table -AutoSize
Write-Host "A high missed-echo percentage means the sensor is aimed badly or is too close/far." -ForegroundColor DarkGray
