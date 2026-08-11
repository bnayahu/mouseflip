
---

## Installation

Download `Primary.exe` below and run it. There is no installer; the icon
appears in the notification area.

**This binary is not code-signed.** Windows SmartScreen will warn on first run —
choose **More info → Run anyway**. To verify the download first:

```powershell
Get-FileHash Primary.exe -Algorithm SHA256
```

Compare the result against `SHA256SUMS.txt` below.

Built from source with MinGW-w64 on `ubuntu-latest` by
[the release workflow](../../.github/workflows/release.yml) — you can rebuild it
yourself with `./build.sh`.
