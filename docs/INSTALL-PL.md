# Instrukcja instalacji i uruchomienia MacMateCaja (macOS)

MacMateCaja to natywny port menedżera plików MATE Caja dla systemu macOS, wykorzystujący silnik GTK 3 Quartz (bez X11/XQuartz) oraz wbudowane podsystemy GVfs i D-Bus.

---

## 1. Wymagania systemowe

Wersja portu macOS: **1.28.1**, oparta na silniku MATE Caja 1.28.0.

- **Architektura**: Apple Silicon (arm64: M1, M2, M3, M4 lub nowszy).
- **Minimalna wersja systemu**: macOS 26.0 lub nowszy (wynika z platformowych bibliotek Mach-O).
- **Uprawnienia**: Do uruchomienia wystarcza zwykłe konto użytkownika. Instalator `.pkg` może poprosić o zatwierdzenie administratora przy zapisie do `/Applications`. ZIP można rozpakować do `~/Applications`. Nie trzeba wyłączać SIP.

---

## 2. Instalacja

Dostępne są dwie metody instalacji:

### Metoda A: Instalator pakietu `.pkg` (Zalecana)
1. Pobierz plik `MacMateCaja-1.28.1-arm64.pkg`.
2. Uruchom instalator podwójnym kliknięciem.
3. Pakiet jest standardowym instalatorem komponentowym, który automatycznie umieści aplikację w katalogu `/Applications/MacMateCaja.app`.
4. Instalator nie wykonuje żadnych ukrytych skryptów `preinstall`/`postinstall`.

### Metoda B: Archiwum ZIP
1. Pobierz plik `MacMateCaja-1.28.1-arm64.zip`.
2. Rozpakuj archiwum w Finderze.
3. Przeciągnij `MacMateCaja.app` do katalogu `/Applications`.

---

## 3. Pierwsze uruchomienie i zatwierdzenie Gatekeeper (Podpis ad-hoc)

Wydanie jest bezpłatnym projektem otwartoźródłowym i posiada lokalny podpis kryptograficzny ad-hoc (bez płatnego certyfikatu Apple Developer ID). Z tego powodu macOS wyświetli przy pierwszym uruchomieniu komunikat o nierozpoznanym deweloperze.

> [!IMPORTANT]
> **NIGDY nie wyłączaj zabezpieczeń Gatekeeper globalnie** (nie stosuj poleceń typu `sudo spctl --master-disable`).
> Nie należy również stosować masowego usuwania atrybutów kwarantanny dla całego dysku.

### Zatwierdzenie przez Ustawienia systemowe:
1. Przy próbie otwarcia aplikacja zostanie zablokowana.
2. Otwórz **Ustawienia systemowe** (**System Settings**) -> **Prywatność i ochrona** (**Privacy & Security**).
3. Przewiń w dół do sekcji *Ochrona* (*Security*).
4. Obok komunikatu o zablokowaniu MacMateCaja kliknij **Otwórz mimo to** (**Open Anyway**).

Zatwierdzaj tylko pakiet ze znanego źródła i zgodny z opublikowaną sumą SHA-256. Podpis ad-hoc nie potwierdza tożsamości autora ani kontroli aplikacji przez Apple. [Oficjalne instrukcje Apple](https://support.apple.com/102445).

---

## 4. Obsługa protokołów zdalnych (SFTP, SMB, NFS, WebDAV)

- **Przeglądanie i nawigacja**: Przeglądanie udziałów sieciowych (SFTP przez SSH, otoczenie sieciowe SMB/Windows, zasoby NFS i WebDAV) działa od razu po instalacji.
- Cały podsystem GVfs (`gvfsd`, `gvfsd-sftp`, `gvfsd-smb` itp.) oraz lokalna szyna D-Bus są wbudowane bezpośrednio w pakiet aplikacji i nie wymagają zewnętrznego oprogramowania.

---

## 5. Zewnętrzne wymagania opcjonalne

### A. Bezpośrednie otwieranie i edycja plików zdalnych w aplikacjach macOS (macFUSE + rclone)
- Przeglądanie katalogów zdalnych realizowane jest natywnie przez GVfs.
- Jeśli chcesz otwierać i modyfikować pliki zdalne bezpośrednio w programach macOS (np. TextEdit, VS Code) bez ich wcześniejszego ręcznego kopiowania na dysk, wymagany jest montaż systemu plików przez **macFUSE** oraz **rclone**.
- **Ważne**: Zgodnie z założeniami bezpieczeństwa MacMateCaja **nie pobiera po cichu plików zdalnych do tymczasowego folderu**, jeśli brak jest punktu montowania.
- **Zatwierdzenie macFUSE**:
  1. Pobierz i zainstaluj [macFUSE](https://osxfuse.github.io/).
  2. Podczas instalacji macOS wyświetli komunikat o zablokowaniu rozszerzenia systemowego dewelopera Benjamin Fleischer.
  3. Wejdź w **Ustawienia systemowe** -> **Prywatność i ochrona** i zezwól na rozszerzenie.
  4. Na komputerach Apple Silicon wymagane może być jednorazowe zezwolenie na rozszerzenia jądra w trybie odzyskiwania (Startup Security Utility -> Reduced Security -> Allow user management of kernel extensions).
  5. Ten pakiet zawiera `rclone`; dodatkowa instalacja nie jest potrzebna.

### B. Asystent węzłów NetBird
- Okno dialogowe NetBird w MacMateCaja pozwala na wygodne łączenie z hostami w sieci prywatnej.
- Wymaga działającego demona NetBird na maszynie oraz dostępnego interpretera `python3` w ścieżce systemowej.

### C. Generowanie miniatur PDF
- Miniatury dokumentów PDF są generowane lokalnie przez natywny mechanizm macOS Quick Look (`qlmanage`). Nie są wymagane żadne dodatkowe biblioteki czy pakiety.

---

## 6. Pliki konfiguracyjne i profile użytkownika

Aplikacja nie instaluje w systemie żadnych prywatnych konfiguracji dewelopera.
- Domyślna lokalizacja startowa to katalog domowy lub Biurko użytkownika.
- Użytkownik może zdefiniować własny plik konfiguracyjny INI (zgodnie ze specyfikacją w `CONFIGURATION.md`), wskazując własne zakładki, profile i preferencje.

## 7. Copy działa, ale Move/F6 jest wyłączone

Przenoszenie wymaga nie tylko odczytu pliku, lecz także prawa usunięcia go
z katalogu źródłowego. Własność samego pliku lub podkatalogu nie wystarcza,
jeżeli jego katalog nadrzędny należy do administratora i nie jest zapisywalny.
W takim przypadku „Move to (not permitted)” jest poprawną blokadą uprawnień,
nie oczekiwaniem na zakończenie obliczeń. Sprawdź uprawnienia katalogu nadrzędnego
w Finderze lub przez `gio info`; nie próbuj automatycznie zmieniać właściciela
całego dysku ani wykonywać masowego `chmod -R`.

Obliczanie rozmiarów może obciążać procesor w tle. Miniatury nie powinny
utrzymywać stanu ładowania całej listy — ich brak nie blokuje nawigacji.
