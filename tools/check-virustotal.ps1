<#
Checks one answer from VirusTotal, or the release's zip before it's sent there. Written 2026-11-08 by Melanikus for the AvP port.

release.yml's scan job asks VirusTotal (www.virustotal.com, which runs about 70 virus scanners on a file) about the zip a release is
about to put out. It saves each answer to a file exactly as it came, and runs this on that file before it believes anything in it. This
script never goes on the network and never sees the key: the scan job keeps both.

Why: an answer comes from outside our code, so it's treated as hostile (AvP Tao #3). It's read as strict JSON, every part this uses is
checked for its type and its range, VirusTotal's hash of the file has to be ours, and nothing VirusTotal wrote is ever printed: GitHub
reads a line that starts with :: as an order, so only numbers and words this script has checked go into the log.

What it checks, by what it's given:
- -Package <zip> -Sha256 <hash>: the zip is the one the build made (that's its SHA-256), and small enough for VirusTotal's upload.
- -Answer <file> -Kind lookup -Sha256 <hash>: VirusTotal's answer to "what do you know about the file with this hash?"
  (GET /api/v3/files/<hash>, when it answered 200). Either it has a finished analysis of enough scanners, which is counted, or it hasn't,
  and the file is sent again.
- -Answer <file> -Kind upload -Sha256 <hash>: its answer to the upload (POST /api/v3/files): the number of the analysis it started.
- -Answer <file> -Kind analysis -Sha256 <hash> -AnalysisId <id>: its answer to "how is that analysis going?"
  (GET /api/v3/analyses/<id>): still going, or finished, and then counted.

What it prints: lines for a person, and, when the answer is in order, one line starting ANSWER: for the scan job, made only of what has
been checked:
  ANSWER: ready                       the zip may be sent
  ANSWER: not-analysed                VirusTotal has no finished analysis of enough scanners, so the zip is sent again
  ANSWER: analysis <id>               the upload started this analysis
  ANSWER: waiting                     the analysis hasn't finished
  ANSWER: counted <malicious> <suspicious> <answered>
                                      finished: how many scanners called it malicious, how many suspicious, and how many gave an answer
Anything else is REFUSED: and the reason, with exit 1. The scan job stops on a refusal, and no draft is made: an answer that can't be
trusted can't be shown to anyone as a result.

A file a scanner flags isn't a refusal here. The user's ruling, 2026-11-08: the draft is still made, its text starts by saying what was
flagged, and the run ends red, so nobody publishes it before reading VirusTotal's report. release.yml's last job does both.

It needs PowerShell 7, which has the strict JSON reader. tests/release-test.ps1 tries it on made-up answers, with no network.
#>
param(
    [string]$Package,
    [string]$Answer,
    [string]$Kind,
    [string]$Sha256,
    [string]$AnalysisId
)

$ErrorActionPreference = 'Stop'

# The fewest scanners that must give an answer before a result counts: "0 of 3 flagged it" says almost nothing. A placeholder number,
# agreed with PO on 2026-11-08; a finished analysis of a zip usually has about 60.
$fewestScanners = 20

# VirusTotal takes a file of up to 32 MB in one upload. Bigger needs another way of sending it, which this doesn't do yet.
$largestUpload = 32000000

# The largest answer read. An answer about one file is far smaller; this is only so a runaway answer isn't read at all.
$largestAnswer = 5000000

# The kinds of count VirusTotal gives for an analysis. Each is a number of scanners. Only the first four are answers about the file;
# the others say a scanner couldn't look. A kind not on this list is refused, because it could be a verdict this would miss.
$countsThatAreAnswers = 'malicious', 'suspicious', 'undetected', 'harmless'
$countsKnown = $countsThatAreAnswers + @('timeout', 'confirmed-timeout', 'failure', 'type-unsupported')

function Stop-Answer([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Stop-Answer 'this needs PowerShell 7 or later, for its strict JSON reader'
}

# Our own hash: 64 of the small letters and digits a SHA-256 is written in. Everything VirusTotal says about a file is compared with it.
if ($Sha256 -cnotmatch '^[0-9a-f]{64}\z')
{
    Stop-Answer 'the hash to check against has to be a SHA-256, 64 small letters and digits'
}

$report = "https://www.virustotal.com/gui/file/$Sha256"

# --- The zip, before it's sent
if ($Package)
{
    if ($Answer -or $Kind -or $AnalysisId)
    {
        Stop-Answer '-Package is checked on its own, with only -Sha256'
    }

    if (-not (Test-Path -LiteralPath $Package -PathType Leaf))
    {
        Stop-Answer 'the zip to send is not there'
    }

    $size = (Get-Item -LiteralPath $Package).Length

    if ($size -gt $largestUpload)
    {
        Stop-Answer "the zip is $size bytes, and VirusTotal takes at most $largestUpload in one upload"
    }

    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Package).Hash.ToLowerInvariant() -cne $Sha256)
    {
        Stop-Answer 'the zip is not the one the build made: its hash is different'
    }

    Write-Host "The zip is $size bytes and is the one the build made."
    Write-Host 'ANSWER: ready'
    exit 0
}

# --- An answer
if ($Kind -cnotin 'lookup', 'upload', 'analysis')
{
    Stop-Answer '-Kind has to be lookup, upload or analysis'
}

if ($Kind -ceq 'analysis' -and $AnalysisId -cnotmatch '^[A-Za-z0-9+/=]{1,200}\z')
{
    Stop-Answer "an analysis is checked against the number the upload gave, and that's missing or isn't one"
}

if (-not $Answer -or -not (Test-Path -LiteralPath $Answer -PathType Leaf))
{
    Stop-Answer "there is no saved answer to read"
}

$length = (Get-Item -LiteralPath $Answer).Length

if ($length -eq 0)
{
    Stop-Answer "VirusTotal's answer is empty"
}

if ($length -gt $largestAnswer)
{
    Stop-Answer "VirusTotal's answer is $length bytes, far more than an answer about one file, so it isn't read"
}

# Strict JSON: no comments, no commas left at the end, nothing after the end, and no deeper than 64 levels. A duplicated name is caught
# below, when a part is looked for.
Add-Type -AssemblyName System.Text.Json

try
{
    $document = [System.Text.Json.JsonDocument]::Parse([System.IO.File]::ReadAllText($Answer, [System.Text.Encoding]::UTF8), [System.Text.Json.JsonDocumentOptions]::new())
}
catch
{
    Stop-Answer "VirusTotal's answer isn't JSON"
}

$objectKind = [System.Text.Json.JsonValueKind]::Object
$stringKind = [System.Text.Json.JsonValueKind]::String
$numberKind = [System.Text.Json.JsonValueKind]::Number

# One part of an object, by its name, written exactly so. $where is our own name for the object, such as data.attributes, for the reason,
# and empty for the whole answer. Missing is refused unless -Optional, and then it gives back $null. A name that's there twice is always
# refused: the two could say different things, and there'd be no knowing which one VirusTotal meant.
function Get-Part($object, [string]$name, [string]$where, [switch]$Optional)
{
    $place = if ($where) { "$where.$name" } else { $name }

    if ($object.ValueKind -ne $objectKind)
    {
        Stop-Answer "in VirusTotal's answer, $(if ($where) { $where } else { 'the whole answer' }) isn't an object"
    }

    $found = @($object.EnumerateObject() | Where-Object { $_.Name -ceq $name })

    if ($found.Count -gt 1)
    {
        Stop-Answer "VirusTotal's answer has $place more than once"
    }

    if ($found.Count -eq 0)
    {
        if ($Optional)
        {
            return $null
        }

        Stop-Answer "VirusTotal's answer has no $place"
    }

    return $found[0].Value
}

# A part that has to be a string, given back as one
function Get-Text($object, [string]$name, [string]$where)
{
    $part = Get-Part $object $name $where

    if ($part.ValueKind -ne $stringKind)
    {
        Stop-Answer "in VirusTotal's answer, $where.$name isn't text"
    }

    return $part.GetString()
}

# The counts of one analysis, by kind, as whole numbers from 0 to 1000: there are about 70 scanners, so anything more is wrong.
# Each known kind may be there; the four that are answers have to be.
function Get-Counts($stats, [string]$where)
{
    if ($stats.ValueKind -ne $objectKind)
    {
        Stop-Answer "in VirusTotal's answer, $where isn't a list of counts"
    }

    $counts = @{}

    foreach ($count in $stats.EnumerateObject())
    {
        if ($countsKnown -cnotcontains $count.Name)
        {
            Stop-Answer "VirusTotal's counts in $where have a kind this doesn't know, which could be a verdict it would miss. A person has to look at the answer, and add the kind here if it's safe"
        }

        if ($counts.ContainsKey($count.Name))
        {
            Stop-Answer "VirusTotal's counts in $where have $($count.Name) more than once"
        }

        $number = 0

        if ($count.Value.ValueKind -ne $numberKind -or -not $count.Value.TryGetInt32([ref]$number) -or $number -lt 0 -or $number -gt 1000)
        {
            Stop-Answer "VirusTotal's count of $($count.Name) in $where isn't a whole number from 0 to 1000"
        }

        $counts[$count.Name] = $number
    }

    foreach ($needed in $countsThatAreAnswers)
    {
        if (-not $counts.ContainsKey($needed))
        {
            Stop-Answer "VirusTotal's counts in $where have no count of $needed"
        }
    }

    return $counts
}

# How many scanners gave an answer about the file
function Get-Answered($counts)
{
    return $counts['malicious'] + $counts['suspicious'] + $counts['undetected'] + $counts['harmless']
}

# Says what a finished analysis found, and hands it on
function Write-Counted($counts)
{
    $answered = Get-Answered $counts
    $flagged = $counts['malicious'] + $counts['suspicious']

    Write-Host "VirusTotal: $flagged of $answered scanners flagged it ($($counts['malicious']) malicious, $($counts['suspicious']) suspicious, $($counts['undetected']) found nothing, $($counts['harmless']) called it harmless). Report: $report"
    Write-Host "ANSWER: counted $($counts['malicious']) $($counts['suspicious']) $answered"
}

$root = $document.RootElement
$data = Get-Part $root 'data' ''

# --- What VirusTotal knows about the file
if ($Kind -ceq 'lookup')
{
    if ((Get-Text $data 'type' 'data') -cne 'file')
    {
        Stop-Answer "VirusTotal's answer is about something that isn't a file"
    }

    if ((Get-Text $data 'id' 'data') -cne $Sha256)
    {
        Stop-Answer "VirusTotal's answer is about a different file: its hash isn't ours"
    }

    $attributes = Get-Part $data 'attributes' 'data'

    if ((Get-Text $attributes 'sha256' 'data.attributes') -cne $Sha256)
    {
        Stop-Answer "VirusTotal's answer is about a different file: its hash isn't ours"
    }

    # A finished analysis has a time. With none, VirusTotal hasn't finished looking at it, so it's sent again and waited for.
    $finished = Get-Part $attributes 'last_analysis_date' 'data.attributes' -Optional

    if ($null -eq $finished)
    {
        Write-Host 'VirusTotal knows this file but has no finished analysis of it, so it is sent again.'
        Write-Host 'ANSWER: not-analysed'
        exit 0
    }

    # Seconds since the start of 1970, as VirusTotal writes times. The last second of the year 9999 is the most a date can hold.
    $when = [long]0

    if ($finished.ValueKind -ne $numberKind -or -not $finished.TryGetInt64([ref]$when) -or $when -le 0 -or $when -gt 253402300799)
    {
        Stop-Answer "in VirusTotal's answer, data.attributes.last_analysis_date isn't a time"
    }

    $counts = Get-Counts (Get-Part $attributes 'last_analysis_stats' 'data.attributes') 'data.attributes.last_analysis_stats'
    $answered = Get-Answered $counts

    if ($answered -lt $fewestScanners)
    {
        Write-Host "VirusTotal's last analysis of this file had answers from only $answered scanners, fewer than $fewestScanners, so it is sent again."
        Write-Host 'ANSWER: not-analysed'
        exit 0
    }

    Write-Host "VirusTotal already knew this file. Its last analysis finished at $([DateTimeOffset]::FromUnixTimeSeconds($when).UtcDateTime.ToString('yyyy-MM-dd HH:mm')) UTC."
    Write-Counted $counts
    exit 0
}

# --- The upload: the analysis it started
if ($Kind -ceq 'upload')
{
    if ((Get-Text $data 'type' 'data') -cne 'analysis')
    {
        Stop-Answer "VirusTotal's answer to the upload isn't an analysis"
    }

    # The number goes into the address of the next question, so it may only be what VirusTotal's numbers are made of
    $id = Get-Text $data 'id' 'data'

    if ($id -cnotmatch '^[A-Za-z0-9+/=]{1,200}\z')
    {
        Stop-Answer "the analysis number in VirusTotal's answer isn't one"
    }

    Write-Host 'VirusTotal took the zip and started an analysis.'
    Write-Host "ANSWER: analysis $id"
    exit 0
}

# --- How the analysis is going
if ((Get-Text $data 'type' 'data') -cne 'analysis')
{
    Stop-Answer "VirusTotal's answer isn't about an analysis"
}

if ((Get-Text $data 'id' 'data') -cne $AnalysisId)
{
    Stop-Answer "VirusTotal's answer is about a different analysis from the one the upload started"
}

$attributes = Get-Part $data 'attributes' 'data'
$status = Get-Text $attributes 'status' 'data.attributes'

if ($status -cin 'queued', 'in-progress')
{
    Write-Host "VirusTotal's analysis is $status."
    Write-Host 'ANSWER: waiting'
    exit 0
}

if ($status -cne 'completed')
{
    Stop-Answer "VirusTotal's analysis is in a state this doesn't know"
}

# Only once it's finished: whose file it was, and what it found
$fileInfo = Get-Part (Get-Part $root 'meta' '') 'file_info' 'meta'

if ((Get-Text $fileInfo 'sha256' 'meta.file_info') -cne $Sha256)
{
    Stop-Answer "VirusTotal's analysis is of a different file: its hash isn't ours"
}

$counts = Get-Counts (Get-Part $attributes 'stats' 'data.attributes') 'data.attributes.stats'
$answered = Get-Answered $counts

if ($answered -lt $fewestScanners)
{
    Stop-Answer "only $answered scanners gave an answer about the zip, fewer than the $fewestScanners a result needs"
}

Write-Counted $counts

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
