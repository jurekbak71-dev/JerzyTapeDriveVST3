# Jerzy Tape Drive VST3

**Jerzy Tape Drive 1.2** to studyjny efekt taśmowy VST3 dla Windows/macOS z automatyzowalnymi parametrami, skalowalnym interfejsem wektorowym i rozbudowanym modelem zużytego transportu.

## Tor audio

Wejście → **Optical Input Compressor** → **Preamp** → **Tape Drive** → **Wow / Flutter / Tape Age** → **Output** → HPF / LPF.

- **DRIVE** — nasycenie i kompresja charakterystyczna dla toru taśmowego.
- **WOW** — wolne odchylenia transportu: składowe mechaniczne + skorelowana losowa zmiana prędkości.
- **FLUTTER** — szybsze drżenie transportu i jitter.
- **TAPE AGE** — zużycie taśmy: dodatkowy scrape flutter, niestabilność prędkości, losowe poślizgi transportu, dynamiczna utrata góry, head bump, długie i mikro-ubytki sygnału, szum, trzaski i niskoczęstotliwościowe uderzenia.
- **OPTICAL INPUT COMPRESSOR** — miękkie kolano, połączona detekcja stereo, szybka i wolna pamięć redukcji oraz programozależny powrót. Aktywna sekcja ma własne lampowo-transformatorowe zabarwienie, więc wpływa na harmoniczne, dół i górę również przy małej redukcji.
- **PREAMP** — OFF / TUBE / TRANSISTOR.
- **HPF / LPF** — filtry wyjściowe z regulacją częstotliwości oraz Q.
- **DRY** — równoległe dodanie sygnału nieprzetworzonego.

Parametry i identyfikatory TapeDrive pozostają zgodne z wcześniejszymi sesjami. Stare stany projektu są migrowane bez automatycznego włączania nowych artefaktów zużycia.

## GUI 1.2

Interfejs został zbudowany ponownie bez bitmapowego tła. Panel, oksydowana stal, ramki, gałki, przełączniki, LED-y i mierniki są rysowane wektorowo przez VSTGUI.

Bazowy układ ma **1040 × 640 px**. Dostępne skale panelu: **70%, 85%, 100%, 125%**, a host może również zmieniać rozmiar okna. Jeden transform obejmuje jednocześnie rysowanie i obszary kliknięć, dlatego DPI monitora nie jest już nakładane drugi raz przez osobny zoom VSTGUI.

Prawy przycisk myszy na gałkach i przełącznikach przywraca wartość domyślną. Wszystkie edytowalne parametry VST3 są dostępne dla automatyzacji hosta, w tym FL Studio.

## Kompilacja

Wymagane: CMake 3.25+, C++17 i oficjalny Steinberg VST3 SDK z submodułami.

```sh
cmake -S . -B build -DVST3_SDK_ROOT=/path/to/vst3sdk -DSMTG_CREATE_PLUGIN_LINK=0
cmake --build build --config Release --target JerzyTapeDrive TapeDriveDSPTests
ctest --test-dir build -C Release --output-on-failure
```

Workflow **Build TapeDrive Windows VST3** buduje Windows x64, uruchamia test DSP oraz natywny test GUI i publikuje artefakt **JerzyTapeDrive-Windows-x64**.

Repozytorium zawiera wyłącznie Tape Drive.

## Automatyzacja VST3

Procesor odczytuje wszystkie punkty automatyzacji i respektuje ich pozycje w próbkach. Parametry ciągłe są interpolowane liniowo zgodnie z VST3 (z poprzednią wartością przy pozycji -1), a przełączniki zmieniają stan przy wskazanej próbce. Bloki bez zmian są przetwarzane w całości; obsługa kolejek nie alokuje pamięci w wątku audio. Test `TapeDriveAutomation` sprawdza przebiegi i skoki, granice bloków, bypass, mono/stereo, float/double, bufor in-place oraz aktualizacje bez audio.


<!-- Jerzy VST GUI System CI validation -->

<!-- Jerzy GUI validation pass 2 -->
