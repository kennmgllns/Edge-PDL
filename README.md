## phase-data-logger

Phase Data Logger firmware on ESP32 module

---

#### Requirements
1. [Visual Studio Code](https://code.visualstudio.com)
    - install vscode extensions (recommended)
        -  C/C++, ms-vscode.cpptools
        -  C/C++ Extension Pack, ms-vscode.cpptools-extension-pack

2. [Espressif IoT Development Framework](https://github.com/espressif/esp-idf)
    - install [ESP-IDF Extension](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md)
    - select **v5.2** as ESP-IDF version (required)
    - *recommended [coding style](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/contribute/style-guide.html)*


#### Dependencies
```bash
git submodule add http://192.168.0.50:3000/Edge_Electrons/esp32-general.git src/general
```
  \- or simply clone the repo with all the submodules
```bash
git clone --recurse-submodules http://192.168.0.50:3000/Edge_Electrons/energy-monitor-v3.git
```


#### Build, Flash and Monitor
* Use VS Code to [build & flash](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/basic_use.md) the binary files. The IDE can also be used to monitor the log prints.
* Edit the contents of `.vscode/settings.json` to specify the flash process & serial port, *e.g.*:

```javascript
{
    "idf.flashType": "UART",
    "idf.portWin": "COM16"
}
```

