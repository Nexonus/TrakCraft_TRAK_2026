# Voxel Landscape — C++ / OpenGL

Projekt renderera świata voxelowego zgodny z tematem „Renderer świata voxelowego (Minecraft)”. Implementacja w C++17, OpenGL 3.3 Core, GLSL 330. Świat, meshing, wybieranie bloków i frustum culling są zaimplementowane w projekcie. GLFW obsługuje okno i wejście, a Dear ImGui panel użytkownika.

## Uruchomienie na Windows

Jeżeli nie masz kompilatora C++ ani CMake, uruchom PowerShell w folderze projektu:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup-windows.ps1
powershell -ExecutionPolicy Bypass -File build.ps1 -Run
```

Pierwszy skrypt pobiera **przenośne** CMake 3.31.8, GCC 15.2.0 (w64devkit 2.4.0) i 7-Zip do lokalnego `.tools/`. Nie zmienia systemowego PATH ani rejestru. Drugi buduje program w konfiguracji Release, uruchamia testy i otwiera aplikację. Pierwsza konfiguracja CMake pobiera źródła GLFW 3.4 i Dear ImGui 1.91.9b; kolejne kompilacje używają lokalnej kopii. Wymagane są internet podczas przygotowania oraz sterownik obsługujący OpenGL 3.3.

Gotowy program zbudowany przez MinGW uruchomisz również bezpośrednio:

```powershell
.\build\voxel.exe
```

Możesz też dwukrotnie kliknąć `run.bat`. Zapisane wyniki przykładowego pomiaru i opis ich interpretacji znajdują się w [docs/POMIARY.md](docs/POMIARY.md).

Z zainstalowanym Visual Studio 2022 (komponent **Desktop development with C++**) i CMake:

```powershell
cmake -S . -B build-vs
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
.\build-vs\Release\voxel.exe
```

Linux/macOS: standardowa kompilacja CMake, z zainstalowanym kompilatorem C++ i bibliotekami deweloperskimi platformy dla GLFW/OpenGL. Przykład dla Ubuntu/Debian:

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev wayland-protocols
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/voxel
```

Można wyłączyć backend Wayland przez `-DGLFW_BUILD_WAYLAND=OFF` i używać X11. Na macOS wymagane są narzędzia deweloperskie Xcode. Te platformy nie są automatycznie zweryfikowane przez sam fakt poprawnej kompilacji na Windows.

## Sterowanie

| Klawisz | Działanie |
| --- | --- |
| Tab / przycisk „Wejdź do świata” | Przechwycenie lub uwolnienie myszy |
| Mysz | Obrót kamery |
| W, A, S, D | Swobodny lot |
| Spacja / lewy Ctrl lub C | W górę / w dół |
| Lewy Shift | Szybszy ruch |
| LPM / PPM | Usunięcie / dodanie wskazanego bloku, zasięg 9 jednostek |
| ŚPM | Pobranie materiału wskazanego bloku |
| 1–7 | Trawa, ziemia, kamień, piasek, drewno, liście, śnieg |
| Home | Powrót kamery do widoku początkowego |
| Esc | Uwolnienie kursora |
| F1 | Ukrycie / pokazanie interfejsu |
| F2 | Wszystkie ściany / tylko ściany odkryte |
| F3 | Włączenie / wyłączenie frustum cullingu |
| F4 | Siatka trójkątów |
| F7 | Porównanie A/B i zapis CSV |
| F12 | Zapis zrzutu do `results/preview.tga` |

Kamera lata swobodnie, bez kolizji i grawitacji. Edycja działa przy przechwyconej myszy, aby kliknięcia interfejsu nie zmieniały terenu. Nie można dodać bloku w komórce kamery, poza wysokością świata ani w niezaładowanym chunku.

## Generowanie terenu podczas ruchu

**Domyślnie włączony jest streaming chunków**: poruszanie kamerą tworzy dalszy teren w dowolnym kierunku, również przy ujemnych X/Z. Wysokość świata wynosi 48 bloków; pozioma mapa nie ma dawnej granicy 96 × 96. Współrzędne i kamera mają ograniczenia precyzji liczb komputerowych, więc nie jest to matematycznie nieskończony świat.

- Domyślny promień ładowania to 4 chunki: kwadrat 9 × 9, czyli 81 chunków wokół kamery. Suwak **Zasięg** oraz parametr `--radius 2..6` zmieniają ten promień.
- Początkowe otoczenie jest przygotowywane przed pokazaniem sceny. Podczas ruchu pojedyncze zadanie generatora działa w tle, a gotowe chunki są przyjmowane na głównym wątku — najwyżej jeden nowy chunk na klatkę. Najbliższe brakujące chunki mają pierwszeństwo.
- Odległe chunki są zwalniane z RAM i obu zestawów buforów GPU. Jeden dodatkowy pas retencji zapobiega ciągłemu usuwaniu i odtwarzaniu chunków przy przekraczaniu granicy. Dla promienia 4 w pamięci znajduje się najwyżej 121 chunków, niezależnie od przebytej drogi.
- Po wczytaniu albo usunięciu chunka unieważniane są siatki jego załadowanych sąsiadów stykających się ścianą. Nie pozostają zasłonięte ściany pomiędzy nowymi fragmentami ani dziury po ich usunięciu.
- Szum jest liczony ze współrzędnych świata i seeda. Generator uwzględnia także drzewa z sąsiednich chunków, więc kształt terenu i koron nie zależy od kolejności ładowania.
- **Dodane i usunięte bloki pozostają zmienione po oddaleniu się i powrocie.** W pamięci przechowywana jest lista edycji, a nie kopia całego odwiedzonego terenu. Lista może rosnąć wraz z liczbą zmienianych komórek. Edycje obowiązują w bieżącej sesji; zamknięcie aplikacji albo przycisk **Nowy teren** je usuwa. Nie ma zapisu świata na dysk.
- Mgła jest dopasowana do zasięgu ładowania. Przy szybkim przemieszczaniu się lub dużym obciążeniu brakujący teren może pojawiać się stopniowo.

Przykłady:

```powershell
.\build\voxel.exe --radius 5
.\build\voxel.exe --finite --chunks 6
```

`--finite` zachowuje wariant ze skończonym światem do testów i porównań. W tym trybie `--chunks 6` oznacza 36 chunków i obszar 96 × 48 × 96 bloków. W trybie streamingu `--chunks` nie określa wielkości świata.

## Pokrycie wymagań

| Wymaganie | Realizacja |
| --- | --- |
| 1. Mapa wysokości i ≥3 rodzaje bloków | Pięć oktaw deterministycznego szumu wartościowego; siedem materiałów i proceduralne drzewa. Dziewięć tekstur 16 × 16 generowanych przy uruchomieniu. |
| 2. Podział na chunki | Osobne VAO, VBO i EBO dla każdego chunka oraz wariantu siatki. |
| 3. Pomijanie zasłoniętych ścian | Zapytanie o sześciu sąsiadów we współrzędnych świata, również przez granice chunków. |
| 4. Tekstury i światło | `sampler2DArray`, filtrowanie nearest; odmienne tekstury góry/boku trawy i drewna; sześć poziomów jasności zależnych od kierunku ściany. Dodatkowo mgła odległościowa. |
| 5. Kamera i edycja | Kamera FPS w trybie lotu; promień DDA do pierwszego pełnego voxela i wskazanie sąsiedniej komórki do dodania bloku. |
| 6. Lokalne przebudowy | Numer rewizji chunka. Edycja wnętrza zmienia jedną rewizję; przy krawędzi także rewizje sąsiadów stykających się ścianą, bez chunków diagonalnych. |
| 7. Frustum culling | Sześć płaszczyzn wyznaczanych z macierzy projekcja × widok; test dodatniego wierzchołka AABB każdego chunka. |
| 8. Statystyki i porównanie | Czas klatki, CPU render, GPU render, liczba widocznych i odrzuconych chunków, trójkątów oraz pamięć siatek. Benchmark A/B tej samej sceny i kamery. |

Siatki obu wariantów są buforowane niezależnie. Przy pierwszym przełączeniu na wariant „wszystkie ściany” tworzone są jego siatki. Po edycji aktualizowany jest aktywny wariant, a nieaktywny uzgadnia rewizje przy następnym użyciu. Nie następuje ponowne generowanie geometrii całego świata po pojedynczej edycji.

## Porównanie wydajności

Ustaw kamerę, a następnie naciśnij **F7**. Program:

1. Dokańcza ładowanie otoczenia kamery i przygotowuje oba zestawy siatek przed pomiarem.
2. Zatrzymuje streaming i zamraża kamerę, scenę, rozdzielczość i ustawienie frustum cullingu, wyłącza VSync i wireframe.
3. Renderuje wariant odkrytych ścian i wariant wszystkich ścian. Każdy ma 20 klatek rozgrzewki i domyślnie 120 próbek.
4. Mierzy GPU przez `GL_TIME_ELAPSED`. Mierzy też czas renderowania z perspektywy CPU z oczekiwaniem na GPU. Pomiar nie obejmuje generowania siatek, rysowania GUI i zamiany buforów.
5. Zapisuje średnią, percentyl 95, liczbę trójkątów, widocznych chunków i bajtów siatki do `results/benchmark.csv`, a wszystkie próbki do `results/benchmark_samples.csv`.

Zmiana rozmiaru okna przerywa pomiar, aby oba warianty miały tę samą rozdzielczość. Standardowy backface culling jest włączony w obu wariantach. Wariant bazowy rzeczywiście przechowuje i wysyła wszystkie sześć ścian każdego pełnego bloku; ukryte powierzchnie nadal obciążają przetwarzanie geometrii mimo odrzucania przez test głębokości. „Widoczne chunki” oznaczają niepuste chunki zaakceptowane przez test frustum, a „trójkąty” liczbę wysłaną do GPU, przed odrzucaniem tylnych ścian i testem głębokości.

Zwykły tryb pokazuje osobno czas całej klatki, czas wysłania poleceń CPU oraz asynchroniczny czas GPU (z opóźnieniem kilku klatek). W benchmarku pomiar CPU jest synchronizowany. Czas GPU nie obejmuje interfejsu. Wartości różnych liczników nie muszą się sumować.

Powtarzalny benchmark z linii poleceń, z ukrytym oknem:

```powershell
.\build\voxel.exe --benchmark --headless --seed 1337 --samples 120 --output results/benchmark.csv --screenshot results/preview.tga
```

`--headless` ukrywa okno, ale nadal wymaga działającego sterownika i kontekstu OpenGL. Nie jest to renderer programowy ani test bez GPU. Pomiary zależą od sprzętu, sterownika i obciążenia systemu. Liczba bajtów dotyczy buforów geometrii aktywnego wariantu, bez narzutu sterownika, tekstur i drugiego zestawu siatek.

## Testy

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
.\build\voxel.exe --smoke-test --screenshot results/smoke.tga
```

Testy CPU sprawdzają: pustą siatkę i pojedynczy blok, winding wszystkich ścian, ukrywanie wspólnej ściany między chunkami i jej przywrócenie, zewnętrzną powłokę pełnego świata, bazowe sześć ścian na blok, lokalne unieważnianie rewizji, DDA w obu kierunkach i w pionie, granicę zasięgu oraz przypadki frustum przed/za kamerą, na granicy i poza płaszczyznami. Sprawdzają również powtarzalność seeda i materiały terenu.

Testy streamingu obejmują ujemne współrzędne, niezależność generowania od okna ładowania, pracę generatora w tle, zachowanie dodanych i usuniętych bloków po ponownym wczytaniu, limit liczby chunków po długiej podróży, zmianę promienia i seeda przy aktywnym zadaniu oraz unieważnianie i odtwarzanie ścian przy wczytywaniu i usuwaniu sąsiadów.

Test GPU wysyła oba warianty siatek, dodaje i usuwa blok na styku chunków, sprawdza liczbę przebudów, porównuje obrazy i bufory głębokości A/B oraz sprawdza frustum culling. W trybie streamingu wykonuje dodatkowo skok do odległego miejsca, 48 klatek lotu z działającą kolejką generatora i powrót do edytowanego bloku; kontroluje zwalnianie nieaktywnych buforów i limit pamięci. Następnie renderuje 12 klatek z GUI. Porównanie kolorów uwzględnia tolerancję na różnice próbkowania tekstur na granicach pikseli. Tryb skończony można sprawdzić osobno przez `--smoke-test --finite`.

Testy samej logiki można zbudować bez sieci i bibliotek graficznych:

```powershell
cmake -S . -B build-core -DVOXEL_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## Organizacja kodu

```text
src/math.hpp        wektory, macierze, kamera, frustum
src/world.*         bloki, chunki, generator, rewizje, DDA
src/mesh.*          budowa geometrii i przypisanie tekstur
src/gl_api.hpp      minimalny loader funkcji OpenGL
src/renderer.*      shadery, tekstury, bufory GPU, pomiar i zrzuty
src/main.cpp        wejście, GUI, benchmark, parametry CLI
tests/core_tests.cpp
```

Z rozszerzeń zaimplementowano streaming chunków. Brak greedy meshingu, ambient occlusion i przezroczystej wody. Wszystkie materiały, również liście, są nieprzezroczyste.

## Źródła i zależności

Inspiracja organizacją renderera: [James Randall — Simple C++ Voxel Engine](https://www.jamesdrandall.com/projects/simple_cpp_voxel_engine/). Artykuł opisuje m.in. chunki, generowanie wysokości i podstawowe oświetlenie. Kod tego projektu jest osobną implementacją, bez kopiowania silnika autora.

- [GLFW 3.4](https://github.com/glfw/glfw/tree/3.4), licencja zlib; [dokumentacja kontekstu](https://www.glfw.org/docs/3.4/context_guide.html).
- [Dear ImGui 1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b), licencja MIT. Obsługuje wyłącznie GUI.
- [CMake](https://github.com/Kitware/CMake), BSD 3-Clause; [w64devkit](https://github.com/skeeto/w64devkit) i [7-Zip](https://www.7-zip.org/) są opcjonalnymi narzędziami przygotowania środowiska. Licencje składników znajdują się w ich pobranych katalogach.

Tekstury są generowane przez projekt, bez zewnętrznych plików graficznych. Shadery są wbudowane, więc uruchomienie programu nie zależy od bieżącego katalogu z zasobami.
