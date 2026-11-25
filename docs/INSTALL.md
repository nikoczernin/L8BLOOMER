# Getting L8BLOOMER into Logic

Grab the macOS ZIP from the [L8BLOOMER release page](https://github.com/nikoczernin/L8BLOOMER/releases/latest), unzip it, and quit Logic.

## Let the helper do it

In Terminal, type `bash ` (with a space after it), drag `Install.command` from the download into the window, and press Return. It copies the plugin into your user folder and backs up any older version. No admin password needed.

Backups live in `~/Library/Audio/Plug-Ins/L8BLOOMER backups/`.

If you built the plugin yourself, run `bash scripts/install.sh` from the project folder.

## Or copy it yourself

In Finder, choose **Go → Go to Folder…** and paste:

```text
~/Library/Audio/Plug-Ins/Components
```

Create the folder if it’s missing, then copy the whole `L8BLOOMER.component` into it. If you’re replacing an older copy, put that one somewhere outside `Components` first so you can go back to it.

Still have **Slow Bloom.component** from an older version? Move that out of `Components` too. It shares L8BLOOMER’s plugin identity, so only one should be installed. The helper takes care of this and saves a backup. Your existing Logic settings carry over.

Open Logic again and choose **Audio FX → Audio Units → Niko Audio → L8BLOOMER** on an audio channel.

## If Logic can’t find it

1. Check that `L8BLOOMER.component` is directly inside `Components`, rather than tucked inside the unzipped download folder.
2. Open Logic’s **Settings → Plug-in Manager**, find `L8BLOOMER` under **Niko Audio**, and click **Reset & Rescan Selection**. On older versions of Logic, Settings is called “Preferences”.
3. If it still hasn’t shown up, restart your Mac and try again.

For a more detailed check, Apple’s Audio Unit validator can tell you what’s going on. Run this in Terminal:

```sh
auval -v aufx SlBl Niko -strict
```

## If macOS blocks it

The download is locally signed, but it hasn’t been signed with an Apple Developer ID or notarized by Apple. That means macOS may warn you or stop Logic from opening it.

If you downloaded it from this repo’s release page and trust it, quit Logic and run this in Terminal:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/L8BLOOMER.component"
```

This clears the download quarantine flag for this plugin only. Open Logic again and rescan it in Plug-in Manager. Building from the source is another option.

## Checking your download

Each release has a `SHA256SUMS.txt` file. Run `shasum -a 256 <downloaded-file>` and compare the result with the matching line in that file. If you’ve downloaded both archives and the checksum file into one folder, you can check them together with `shasum -a 256 -c SHA256SUMS.txt`.

## Updating, going back, or removing it

Quit Logic first. The helper keeps a backup when you update, so you can put the old `.component` back if you need to. To remove the plugin, move `L8BLOOMER.component` from `Components` to the Trash and reopen Logic. Sessions that used it will show it as missing until you reinstall it.
