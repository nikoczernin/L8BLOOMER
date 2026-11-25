# Installing L8BLOOMER

Download the macOS universal ZIP from [L8BLOOMER releases](https://github.com/nikoczernin/L8BLOOMER/releases/latest), unzip it, and quit Logic.

## Automatic installation

In Terminal, type `bash ` (with a trailing space), drag the extracted `Install.command` into the Terminal window, then press Return. This installs only for your user account and does not require an administrator password. Existing versions are backed up under `~/Library/Audio/Plug-Ins/L8BLOOMER backups/`.

From a source checkout after building, use `bash scripts/install.sh` instead.

## Manual installation

In Finder, choose **Go → Go to Folder…** and paste `~/Library/Audio/Plug-Ins/Components`. Create the folder if needed. Copy `L8BLOOMER.component` there. Move any previous copy to a backup outside `Components` before installing the new one.

If you have the older **Slow Bloom.component**, move it out of `Components` as well. It shares L8BLOOMER’s Audio Unit identity. The installer handles this automatically and keeps a backup; existing Logic sessions retain their parameter identifiers.

Restart Logic, then choose **Audio FX → Audio Units → Niko Audio → L8BLOOMER** on an audio channel.

## If it does not appear

1. Confirm the bundle is directly in `Components`, not inside another extracted folder.
2. In Logic, open **Settings → Plug-in Manager**, find `L8BLOOMER` under **Niko Audio**, select it, and choose **Reset & Rescan Selection**. Older Logic versions call Settings “Preferences”.
3. Restart your Mac if the newly installed Audio Unit has not been registered yet.
4. For a diagnostic check, run:
   ```sh
   auval -v aufx SlBl Niko -strict
   ```

## If macOS blocks the download

The binary has a local ad-hoc signature. It is not signed with an Apple Developer ID or notarized. If macOS reports that it cannot verify the developer or blocks the downloaded component, first confirm it came from this repository’s release page.

For a download you trust, quit Logic and remove the quarantine attribute **only from this installed plugin**:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/L8BLOOMER.component"
```

Then reopen Logic and rescan that plugin in Plug-in Manager. This does not disable Gatekeeper globally. If you prefer, build from the published source instead.

The release’s `SHA256SUMS.txt` lets you check downloads in Terminal using `shasum -a 256 <downloaded-file>` and comparing the result to the matching line. To verify every listed archive after downloading them to one folder, run `shasum -a 256 -c SHA256SUMS.txt` there.

## Update or uninstall

Quit Logic before either operation. The installer preserves previous copies outside the active `Components` folder. To roll back, replace the active component with the backup. To uninstall, move `L8BLOOMER.component` from `Components` to the Trash, then restart Logic. Existing sessions will report a missing plugin until it is reinstalled.
