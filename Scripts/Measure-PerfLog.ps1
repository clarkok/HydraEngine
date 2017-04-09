param (
    [string] $logFile,
    [string] $perfSession = $null
)

function ParseTimestamp([string]$literal)
{
    $splited = $literal -split '[:.]'
    return ((([int64]$splited[0] * 60 + $splited[1]) * 60 + $splited[2]) * 1000 + $splited[3]) * 1000 + $splited[4]
}

function ParseLogLine([string]$line)
{
    $PATTERN = [regex]'^(?<timestamp>\d+:\d\d:\d\d\.\d\d\d\.\d\d\d)\t(?<threadId>\d+)\tPerf \d\.\d\d\d\(\+\d\.\d\d\d\) (?<session>[^:]+)::(?<phase>.+)$'

    $match = $PATTERN.Match($line)
    if (-not $match.Success)
    {
        return $null
    }

    return New-Object -Type psobject -Property @{
        Timestamp = $match.Groups["timestamp"].value
        Microsecond = ParseTimestamp $match.Groups["timestamp"].value
        ThreadId = [int]$match.Groups["threadId"].value
        Session = $match.Groups["session"].value
        Phase = $match.Groups["phase"].value
    }
}

function Get-Logs([string]$logFile)
{
    Get-Content $logFile | ForEach-Object {
        ParseLogLine $_
    }
}