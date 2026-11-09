# The words that point a Hudson run at a relay on another machine, checked the same way by run-net-pair.ps1 and run-hudson.ps1, which
# both read this file. Written 2026-11-09 by Melanikus (Third) for the AvP port.
#
#   -RemoteRelay <name or address>  the one relay beyond this machine: a server's name, looked up once, or one plain IPv4 address, which
#                                   needs no name looked up, so nothing on this machine (its hosts file) has to change
#   -RelayListPin <64 hex digits>   the SHA-256 of the relay's list's certificate, the whole certificate as it's stored (DER): what
#                                   openssl's "x509 -fingerprint -sha256" prints, without the colons. The game checks the list's
#                                   certificate against it before it sends a byte. An address can't be checked any other way, so an
#                                   address needs a pin; a name may have one too
#   -RelayServerName <text>         the server's name as people see it, such as "New York (Mel's server)": 1 to 32 letters, digits,
#                                   spaces and ' ( ) - . , and nothing else
#
# Every word is input from outside (AVP Tao 3), so each is checked to its shape here, before it goes anywhere: into a settings file, onto
# a command line, or to the name service.

# One plain IPv4 address: four numbers 0-255 with dots between, no leading zeros, and nothing before or after (\A and \z: a "$" would
# let a final newline through)
function Test-HudsonRelayAddress([string]$text)
{
    if ($text -cnotmatch '\A(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\z') { return $false }
    foreach ($i in 1..4) { if ([int]$Matches[$i] -gt 255) { return $false } }
    return $true
}

# Why these three words can't be used together as given, or '' if they can. Each reason is its own, so a test can tell them apart.
function Get-HudsonRelayWordsRefusal([string]$remoteRelay, [string]$pin, [string]$serverName)
{
    if (-not $remoteRelay)
    {
        if ($pin) { return "-RelayListPin says how to check -RemoteRelay's list, so it needs -RemoteRelay" }
        if ($serverName) { return "-RelayServerName names -RemoteRelay's server, so it needs -RemoteRelay" }
        return ''
    }

    if ($pin -and $pin -cnotmatch '\A[0-9a-fA-F]{64}\z') { return "-RelayListPin is the SHA-256 of the list's certificate: 64 hex digits, and nothing else" }
    if ($serverName -and $serverName -cnotmatch "\A[A-Za-z0-9 '()\-.,]{1,32}\z") { return "-RelayServerName is 1 to 32 letters, digits, spaces and ' ( ) - . , only" }

    if (Test-HudsonRelayAddress $remoteRelay)
    {
        if (-not $pin) { return "-RemoteRelay is an address, so its list can only be trusted by its certificate: it needs -RelayListPin" }

        # Not this machine (that's -Relay alone), nor "any address", nor one that's everyone's at once
        if ($remoteRelay -match '\A(0|127|22[4-9]|2[3-5]\d)\.' -or $remoteRelay -match '\A169\.254\.') { return "-RemoteRelay's address isn't one a relay on another machine can have" }
        return ''
    }

    # Digits and dots that aren't one plain address are refused, never looked up as a name
    if ($remoteRelay -cmatch '\A[0-9.]+\z') { return "-RemoteRelay looks like an address, but isn't one plain IPv4 address" }

    if ($remoteRelay.Length -gt 253 -or $remoteRelay -cnotmatch '\A[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?(\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)+\z') { return "-RemoteRelay is a server's name, in small letters, or one IPv4 address" }
    return ''
}
