This release contains prebuilt macOS and Windows packages for `The Jury`.

Minimal install steps:

1. Download the archive for your platform and unzip it.
2. Keep `ppa_service` and `ppa_service.toml` in the same folder.
3. Start the service:
   - macOS: run `./ppa_service`
   - Windows: run `.\ppa_service.exe`
4. In Lightroom Classic, open `File > Plug-in Manager...`, click `Add`, and select the unzipped `PpaCritique.lrplugin` folder.
5. If needed, edit `ppa_service.toml` to point to your local Ollama runtime or preferred model.

The package also includes `INSTALL.md` with fuller setup details.
