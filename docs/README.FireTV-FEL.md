# Kodi FEL for Fire TV Cube 3

**Experimental software. Use at your own risk.**

Experimental Kodi **22.0 RC2 (Piers)** fork for **Amazon Fire TV Cube 3 (Gazelle)**. Installs
separately from official Kodi as **Kodi FEL** (`org.xbmc.kodi.fel`), with its own
settings and app storage. Other devices show an unsupported-device warning and exit.

Built on [Kodi](https://github.com/xbmc/xbmc) and
[djnice’s Android FEL work](https://github.com/djnice/vlc-fel-android), adapted
for Gazelle in [our VLC FEL fork](https://github.com/signde/vlc-fel-android).
Kodi decodes the base layer; `libdvfelplay` decodes the enhancement layer and
feeds the hardware Dolby Vision composer.

## Requirements

- Rooted Cube 3, Fire OS **7.7.0.2 (PS7702.4965N)** or **7.7.1.7 (PS7717.5741N)**.
- The combined [Fire TV AV v0.2.0 experimental](https://github.com/signde/firetv-passthrough/releases/tag/v0.2.0) Magisk module, with FEL enabled.
- Dolby Vision display and unencrypted Profile 7 FEL media.

## Installation

Install the Kodi FEL APK directly; official Kodi can remain installed. To reuse
your existing setup, back it up from official Kodi and restore it in Kodi FEL.

Install Kodi FEL and the module in either order. Reboot after installing the
Magisk module; installing Kodi afterward needs no additional reboot.
Keep MediaCodec Surface hardware acceleration enabled. For lossless
audio, select **AudioTrack (IEC), Kodi IEC packer**, enable passthrough and only
the formats supported by your receiver. The fork does not change Kodi’s audio
implementation. The player itself does not install kernel modules.

If you experience audio-sync issues with Kodi's packer, particularly with DD+,
try **AudioTrack (RAW), Android IEC packer** (the system packer) under
**Settings → System → Audio → Passthrough output device**. Keep passthrough
and your receiver's supported formats enabled. This resolved the observed DD+
sync issue in our Cube 3 comparison with the Fire TV AV module active.
`superviseaudiodelay` does not apply to system-packer passthrough; it can remain
enabled for PCM output or when switching back to Kodi's packer.


For Cube 3 audio, create or update
`/sdcard/Android/data/org.xbmc.kodi.fel/files/.kodi/userdata/advancedsettings.xml`:

```xml
<advancedsettings>
  <audio>
    <superviseaudiodelay>true</superviseaudiodelay>
    <allowmultichannelfloat>true</allowmultichannelfloat>
  </audio>
</advancedsettings>
```

If the file exists, merge these entries into its existing `<audio>` section;
keep your other settings. Fully exit and restart Kodi FEL afterward.
Delay supervision helps audio resynchronize after pauses and seeks; brief audio
dropouts can occur while it recovers. Multichannel float enables higher-precision
PCM output. After restoring a backup, recheck that passthrough and the supported
formats are still enabled.

## Playback checks

| Path | Result |
| --- | --- |
| P7 FEL, 24 fps | ✅ Picture, audio, pause/resume and seeking on both tested builds |
| P7 FEL, 25 fps over SMB | ✅ TrueHD, pause/resume and seeking on PS7717 |
| P7 MEL | ✅ Normal DV fallback, TrueHD and seeking on PS7717 |
| P5 / P8 | ✅ Normal DV path on PS7717; use the audio settings above |
| 4K60 FEL | ⚠️ Plays, but presentation drops frames |
| Unsupported Android device | ✅ Warning on launcher and external-player entry |

FEL uses the module’s shared, explicitly activated player interface. MEL,
other DV profiles and ordinary video use Kodi’s normal path. Dolby Vision signal
reporting alone does not prove FEL composition; the FEL diagnostic was also
visually checked.

## Known issues

- **Unsupported test clip:** `DV FEL All Layers Test (Woman at 80s).mkv`
  (the rainbow pinwheel) has mismatched base/enhancement-layer GOPs and picture
  ordering, contrary to [Profile 7's layer-alignment requirement](https://ott.dolby.com/OnDelKits/Dolby_Vision_Online_Delivery_Kit/v1/Documentation/Specs/Visio_Profiles/help_files/topics/c_notes_profile_public.html).
  It is not supported and should not be used to judge FEL playback of normal movies.
- **4K60 FEL:** frame pacing remains experimental. Deferred.
- **Seeking:** recoverable MediaCodec buffer-release warnings remain.
- **PCM to compressed audio:** a transition stall also reproduced without our
  modules. Deferred.
- **Alexa during TrueHD:** not qualified; the module fixes the ordinary TrueHD
  microphone error storm, but Alexa interaction remains deferred.

## Player Process Info

Includes **signde PPI (Kodi FEL)** in its classic layout. Enable it under
**Add-ons → My add-ons** if prompted. In **Keymap Editor**, choose
**Edit → Global → Add-ons → Launch signde PPI (Kodi FEL)**, assign a remote
button and save. Alternatively, bind a button to
`RunScript(script.signde.tinyppi)` to toggle it during playback, or launch it from
Programs. It shows source HDR/DV details, FEL pipeline status, audio output,
bitrates, player queues and Kodi CPU usage (percentage of total CPU capacity).
HDR10 and Dolby Vision metadata is read from the source stream; sampled RPU values
can lead the displayed frame. Speaker graphics and CoreELEC VS10 controls are disabled.
Fields not exposed by Fire OS or Kodi show N/A. HDMI driver telemetry may be blocked
by Android permissions; the overlay does not request root or change output modes.
