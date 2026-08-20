# ===========================================================================
#  Automated collection driver for the motor fault detector rig.
#
#  Sends 'G' to the Arduino, then listens. The rig announces each trial with
#  a "#TRIAL,<n>,<label>,..." marker; everything until "#END" is written to
#  <label>.<n>.csv, which is exactly the filename format Edge Impulse uses to
#  infer labels on upload.
#
#  Usage:   .\collect.ps1
#           .\collect.ps1 -Port COM5 -OutDir C:\Users\alita\stardance\rig
#
#  Close the Arduino Serial Monitor and the live dashboard first.
# ===========================================================================

param(
  [string]$Port   = "COM3",
  [int]   $Baud   = 115200,
  [string]$OutDir = "$env:USERPROFILE\stardance\rig"
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, one
$sp.ReadTimeout  = 8000
$sp.DtrEnable    = $true
$sp.NewLine      = "`n"

try { $sp.Open() }
catch { Write-Host "Could not open $Port - is the Serial Monitor or dashboard still holding it?" -ForegroundColor Red; exit 1 }

Write-Host "Opened $Port. Waiting for the rig to boot and calibrate..." -ForegroundColor Cyan
Start-Sleep -Seconds 3
$sp.DiscardInBuffer()

# ask for a fresh calibration, then start the run
$sp.Write("K"); Start-Sleep -Seconds 6
$sp.Write("G")

$writer   = $null
$current  = ""
$rows     = 0
$summary  = @()

while ($true) {
  try { $line = $sp.ReadLine().Trim() } catch { Write-Host "Serial timed out." -ForegroundColor Yellow; break }
  if (-not $line) { continue }

  if ($line.StartsWith("#")) {
    $parts = $line.Split(",")

    switch ($parts[0]) {

      "#CAL"   { Write-Host $line -ForegroundColor DarkCyan }

      "#TRIAL" {
        $n     = $parts[1]
        $label = $parts[2]
        $asked = ($parts[3] -replace "asked=", "")
        $dist  = ($parts[4] -replace "dist=",  "")

        $current = "$OutDir\$label.$n.csv"
        if ($writer) { $writer.Close() }
        $writer = [System.IO.StreamWriter]::new($current)
        $writer.WriteLine("ax,ay,az")
        $rows = 0

        $flag = if ($label -ne $asked) { "  <-- MISMATCH, flap may have jammed" } else { "" }
        Write-Host ("trial {0,2}  {1,-8} (asked {2,-8}) {3,4} mm{4}" -f $n, $label, $asked, $dist, $flag) `
          -ForegroundColor $(if ($label -ne $asked) { "Red" } else { "Green" })

        $summary += [pscustomobject]@{ trial = $n; label = $label; asked = $asked; dist_mm = $dist }
      }

      "#END"   { if ($writer) { $writer.Close(); $writer = $null }
                 Write-Host ("            {0} rows" -f $rows) -ForegroundColor DarkGray }

      "#RUN"   { Write-Host $line -ForegroundColor Cyan
                 if ($parts[1] -eq "done") { break } }

      default  { }   # #DIST and anything else: ignore
    }
    continue
  }

  # data line: timestamp,ax,ay,az  ->  keep only the three axes
  if ($writer) {
    $f = $line.Split(",")
    if ($f.Count -eq 4) { $writer.WriteLine("$($f[1]),$($f[2]),$($f[3])"); $rows++ }
  }
}

if ($writer) { $writer.Close() }
$sp.Close()

Write-Host ""
Write-Host "Done. Files written to $OutDir" -ForegroundColor Green
$summary | Format-Table -AutoSize
$summary | Export-Csv -NoTypeInformation -Path "$OutDir\trial_log.csv"
Write-Host "Trial log: $OutDir\trial_log.csv"
Write-Host ""
Write-Host "Next: upload the CSVs in $OutDir to Edge Impulse with 'infer from filename'." -ForegroundColor Cyan
