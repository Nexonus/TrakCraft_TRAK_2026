# Porównanie wariantów siatki

Pomiar wykonano 3 listopada 2026 r. na NVIDIA GeForce RTX 5080 Laptop GPU, sterownik 610.78, kontekst OpenGL 3.3.0. Jest to wynik konkretnego uruchomienia, a nie gwarantowana wydajność na innym komputerze.

Źródłem liczb jest `results/streaming_benchmark.csv`; poszczególne próbki znajdują się w `results/streaming_benchmark_samples.csv`. Ponowne uruchomienie benchmarku z tą samą ścieżką nadpisuje te pliki.

## Warunki

- Świat strumieniowany: promień 4, 81 załadowanych chunków, wysokość 48, seed 1337.
- Kamera: pozycja (8; 43; 8), yaw −115°, pitch −22°, pionowy FOV 68°.
- Obraz: 1440 × 900 pikseli, okno ukryte, 4 próbki MSAA.
- Frustum culling i backface culling włączone w obu wariantach; wireframe i VSync wyłączone.
- Kamera i świat stałe, oba zestawy siatek przygotowane przed pomiarem. Kolejka generatora opróżniona, streaming zatrzymany na czas porównania.
- Każdy wariant: 20 klatek rozgrzewki, następnie 120 próbek.
- GPU: `GL_TIME_ELAPSED`; CPU: fragment renderujący scenę, z oczekiwaniem na zakończenie GPU. GUI, zamiana buforów i budowa siatek poza pomiarem.

## Wyniki

| Wielkość | Tylko odkryte ściany | Wszystkie ściany |
| --- | ---: | ---: |
| Widoczne chunki | 34 | 34 |
| Wysłane trójkąty | 50 210 | 2 091 588 |
| Bufory geometrii wszystkich załadowanych chunków | 8,97 MiB | 357,81 MiB |
| Średni czas GPU | około 0,026 ms | około 0,399 ms |
| Średni czas CPU z oczekiwaniem | około 0,118 ms | około 0,496 ms |

Usuwanie zasłoniętych ścian zmniejszyło liczbę trójkątów o **97,60%**. W tym pomiarze czas samego renderowania GPU był około 15 razy krótszy. Nie oznacza to 15-krotnego wzrostu FPS całego programu: interfejs, wejście, synchronizacja, sterownik i zamiana buforów również mają koszt. Czasy poniżej jednej milisekundy są wrażliwe na taktowanie karty i obciążenie systemu; porównanie liczby trójkątów i bajtów jest niezależne od tych wahań.

Oba warianty korzystają z tych samych voxelów, tekstur, oświetlenia, kamery i testu frustum. Wariant wszystkich ścian wysyła sześć ścian każdego pełnego bloku. Wariant zoptymalizowany wysyła ścianę tylko wtedy, gdy sąsiad w danym kierunku jest powietrzem, znajduje się poza wysokością świata lub w niezaładowanym chunku. Zapytanie o sąsiada przechodzi przez granice chunków. Wczytanie albo usunięcie sąsiada powoduje przebudowę zależnych siatek.

Test porównawczy obrazów wykazał zerową średnią różnicę bufora głębokości oraz średni błąd koloru około 0,0023 na skali 0–255. Test GPU potwierdził również przebudowę trzech chunków po dodaniu i po usunięciu bloku w (15, 46, 15): chunka zawierającego blok, sąsiada +X i sąsiada +Z. Chunk po przekątnej pozostał niezmieniony. Kamera skierowana od świata w odległym miejscu spowodowała odrzucenie wszystkich chunków; wyłączenie cullingu ponownie wysłało wszystkie niepuste chunki.

W testach CPU przeszło **1389 sprawdzeń**, obejmujących także generowanie w tle, ujemne współrzędne, zachowanie edycji po powrocie, limit liczby chunków i poprawność granic przy ładowaniu i usuwaniu sąsiadów. Test GPU wykonał również 48 klatek lotu z aktywnym streamingiem, sprawdził zwalnianie buforów obu wariantów po oddaleniu się i przywrócenie edytowanego voxela po powrocie. Zakończył się bez błędów OpenGL.

## Powtórzenie

```powershell
.\build\voxel.exe --benchmark --headless --seed 1337 --radius 4 --samples 120 --output results/streaming_benchmark.csv --screenshot results/streaming-preview.tga
```

Przy ocenie frustum cullingu warto obrócić kamerę i przełączać F3. Widok początkowy obejmuje 34 spośród 81 załadowanych chunków. Podczas zwykłego lotu okno załadowanych chunków przesuwa się razem z kamerą; limit liczby rezydentnych chunków wynosi (2r + 3)², czyli 121 dla promienia 4. Liczba zapisanych edycji może rosnąć z liczbą zmienianych komórek, lecz odwiedzony, nieedytowany teren nie jest przechowywany po usunięciu z okna ładowania.
