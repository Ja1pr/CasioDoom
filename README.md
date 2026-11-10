# CasioDoom

Port původního **DOOMu** (zdrojáky [id-software/doom](https://github.com/id-software/doom),
`linuxdoom-1.10`) jako doplněk (add‑in) pro kalkulačku **CASIO fx‑9860GIII**:
displej 128×64, 1 bit na pixel, bez zvuku.

![Snímky z testovací PC verze](docs/screenshots.png)

> **Stav: neodzkoušeno na skutečné kalkulačce.**
> Engine je kompletní a běží v testovací PC verzi, která používá přesně stejný
> kód jako kalkulačka (včetně přehrání originálních dem `DEMO1–3` ve shodě
> s původní hrou). Kód pro kalkulačku se překládá kompilátorem `sh-elf-gcc`
> proti hlavičkám gint, ale hotový `.g1a` jsem neměl jak slinkovat ani pustit
> na reálném hardwaru. Nejrizikovější částí je hledání WADu ve flash paměti
> (viz [Jak to funguje](#jak-to-funguje)) a množství volné RAM. Počítejte
> s tím, že první spuštění může vyžadovat doladění.

## Co je zachováno

- celá herní logika originálu (příšery, zbraně, dveře, výtahy, přepínače,
  teleporty, tajné místnosti, všech 5 obtížností včetně Nightmare),
- menu: nová hra / epizoda / obtížnost, nastavení, ukládání a načítání (6 pozic),
  nápověda, ukončení,
- stavový řádek s obličejem, automapa (zoom, sledování, mřížka, značky),
- mezihra se statistikami a mapou epizody, závěrečné texty, „cast“ z Doomu II,
- přechodový efekt „melt“, titulní smyčka s demy,
- cheaty – protože se na kalkulačce nedají psát, jsou v menu
  *Options → Cheats* (IDDQD, IDFA, IDKFA, IDCLIP, IDDT).

Co chybí: zvuk a hudba, barvy, multiplayer, nahrávání dem, nízké detaily a
plynulá změna velikosti okna (jsou jen dva režimy: se stavovým řádkem a celá
obrazovka).

## Postup

### 1. Převod WADu

Kalkulačka má na doplněk jen asi 100 kB RAM a 3 MB úložiště, takže původní WAD
(4–12 MB) nejde použít přímo. Skript `tools/wad2calc.py` z něj udělá
„kalkulačkový WAD“ `DOOM.CWD`. Stačí Python 3, nic dalšího.

```sh
python3 tools/wad2calc.py doom1.wad                      # shareware -> DOOM.CWD (1,9 MB)
python3 tools/wad2calc.py doom.wad  --maps E1M1-E1M9     # jen 1. epizoda
python3 tools/wad2calc.py doom2.wad --maps MAP01-MAP06 --shift 2
python3 tools/wad2calc.py doom.wad mujlevel.wad --maps E1M1   # IWAD + PWAD
```

| volba | význam |
|---|---|
| `-o SOUBOR` | výstup (výchozí `DOOM.CWD`) |
| `--maps` | které mapy zahrnout (`E1M1-E1M9`, `MAP01,MAP02`…); výchozí: všechny |
| `--shift 0/1/2` | zmenšení textur a spritů: plné / poloviční (výchozí) / čtvrtinové |
| `--gamma` | zesvětlení šedé škály (výchozí 1.3) |
| `--all-textures` | ponechat i textury, které vybrané mapy nepoužívají |
| `--endian little` | jen pro testovací PC verzi |

Převodník odstraní zvuky a hudbu, převede paletu na 64 odstínů šedi, složí a
zmenší textury, sprity a podlahy, nahradí písmo, stavový řádek a menu verzemi
pro 128×64 a mapy přestaví do struktur, které kalkulačka čte přímo z flash.
Soubor se musí vejít do úložiště kalkulačky (cca 3 MB spolu s doplňkem) –
u registrované verze nebo Doomu II proto vyberte jen část map.

### 2. Sestavení doplňku

Je potřeba [fxSDK](https://git.planet-casio.com/Lephenixnoir/fxsdk) s jádrem
[gint](https://git.planet-casio.com/Lephenixnoir/gint) (≥ 2.9):

```sh
fxsdk build-fx          # vytvoří CasioDoom.g1a
```

V repozitáři je i workflow pro GitHub Actions (`.github/workflows/build.yml`),
které fxSDK nainstaluje a `CasioDoom.g1a` nabídne ke stažení jako artefakt.
Tento workflow jsem neměl možnost spustit, berte ho jako výchozí bod.

### 3. Instalace

1. Na kalkulačce v menu *MEMORY* spusťte **optimalizaci úložiště** (Storage
   Memory → Optimization). Je to důležité: `DOOM.CWD` musí ležet ve flash
   pokud možno souvisle.
2. Přes USB zkopírujte do kořene úložiště `CasioDoom.g1a` a `DOOM.CWD`
   (nejdřív `DOOM.CWD`).
3. Spusťte doplněk **DOOM**. Při startu se chvíli zobrazuje „Locating
   DOOM.CWD…“.

Uložené hry (`DOOMSAV0.DSG` až `DOOMSAV5.DSG`) a nastavení (`DOOM.CFG`) se
zapisují do úložiště.

## Ovládání

| klávesa | akce |
|---|---|
| šipky | pohyb vpřed/vzad, otáčení |
| `SHIFT` | střelba |
| `ALPHA` | použít / otevřít |
| `OPTN` (držet) | úkroky místo otáčení |
| `VARS` (držet) | běh |
| `F5` / `F6` | úkrok vlevo / vpravo |
| `1`–`7` | zbraně |
| `F1` | automapa (`+`/`−` zoom, `0` celá mapa, `X,θ,T` sledování, `log` mřížka, `ln` značka, `sin` smazat značky) |
| `F2` / `F3` | uložit / načíst hru |
| `F4` | jas (5 úrovní) |
| `+` / `−` | celá obrazovka ↔ stavový řádek |
| `EXIT` | menu / zpět |
| `EXE` | potvrzení (v menu a u dotazů = „ano“), zopakování poslední zprávy |
| `DEL` | o úroveň menu zpět |
| `AC/ON` | pauza |
| `MENU` | hlavní menu kalkulačky |

## Jak to funguje

**Paměť.** fx‑9860GIII dává doplňku řádově 100 kB RAM. Do té by se nevešla ani
jediná mapa, natož textury. Proto se `DOOM.CWD` vůbec nenačítá: při startu
doplněk projde soubor po sektorech, najde každý z nich ve flash paměti (ta je
vidět v adresním prostoru procesoru) a hra pak čte geometrii map, textury a
sprity **přímo z flash**. V RAM je jen to, co se za hry mění:

| | velikost |
|---|---|
| obrazový buffer 128×64 (64 odstínů šedi) + 1bit kopie | 9 kB |
| pracovní tabulky rendereru | ≈ 24 kB |
| ostatní (HUD, tabulky, klávesy…) | ≈ 4 kB |
| statické proměnné | 4 kB |
| stav mapy (sektory, čáry, předměty, příšery) | 21–72 kB podle mapy |

Pro mapy shareware epizody na Ultra‑Violence vychází stav mapy takto: E1M1
21 kB, E1M2 45, E1M3 56, E1M4 38, E1M5 44, E1M6 72, E1M7 53, E1M8 30, E1M9
37 kB. Když paměť dochází, hra při načítání mapy vynechá čistě dekorační
předměty (lampy, mrtvoly); kdyby nestačilo ani to, skončí hlášením
„Out of memory“. Kolik RAM kalkulačka skutečně poskytne, jsem nemohl změřit.

Aby to šlo, jsou oproti originálu data mapy rozdělena na neměnnou část ve
flash (bez ukazatelů, jen indexy) a malou proměnnou část v RAM; stisknutý
přepínač je 1 bit na stranu zdi, rolující zdi řeší renderer, tabulky stavů a
příšer jsou konstanty v ROM a předměty, které se nehýbou, zabírají jen část
struktury `mobj_t`.

**Fragmentace.** Pokud soubor ve flash neleží souvisle, musí se každý blok dat,
který přesahuje přes „šev“, zkopírovat do RAM – a té je málo. Proto ta
optimalizace úložiště před kopírováním. Po zápisu do úložiště (uložení hry) se
soubor teoreticky může přesunout; kdyby se hra po uložení začala chovat
divně, ukončete ji a spusťte znovu.

**Obraz.** Renderer kreslí do 8bitového bufferu v odstínech šedi (osvětlení
podle vzdálenosti funguje jako v originále) a teprve hotový snímek se
uspořádaným ditheringem 8×8 převede na 1 bit. Protože je Doom tmavá hra,
kontrast se při tom roztahuje; jas se mění klávesou `F4`. Zranění a sebrání
předmětu místo změny palety krátce ztmaví / zesvětlí obraz.

**Věrnost.** Herní kód je původní; upravený je jen přístup k datům. Původní
dema ze shareware WADu se přehrávají ve shodě s originálem (hráč v nich
dojde tam, kam má), což je dobrý test, že se logika nezměnila.

## Testovací PC verze

`src/host` obsahuje bezhlavou verzi pro PC: stejný engine, virtuální čas,
klávesy ze skriptu a ukládání snímků.

```sh
make -C src/host
python3 tools/wad2calc.py doom1.wad -o test.cwd --endian little
src/host/doomhost -wad test.cwd -warp 1 1 -frames 300 -dump snimky -every 20 \
    -keys "50:up 150:-up 160:fire 165:-fire"
src/host/doomhost -wad test.cwd -playdemo DEMO1 -frames 3000   # test synchronizace
```

Další volby: `-heap N` (velikost haldy; 64bitové PC potřebuje víc než
kalkulačka), `-frag N` (simulace fragmentovaného souboru), `-skill 1-5`,
`-exitlevel T`, `-finale T`. Na konci vypíše, kolik RAM by mapa zabrala na
kalkulačce.

## Struktura

| | |
|---|---|
| `tools/wad2calc.py` | převodník WAD → `DOOM.CWD` |
| `tools/gentables.c` | generátor `src/r_viewtab.c` (tabulky pro šířku 128) |
| `src/*.c` | engine (upravený `linuxdoom-1.10`) |
| `src/fx/i_fx.c` | vrstva pro kalkulačku (gint): displej, klávesy, časovač, soubory, WAD ve flash |
| `src/host/` | testovací PC verze |
| `CMakeLists.txt` | sestavení přes fxSDK |

## Licence

Zdrojový kód DOOMu vydala id Software pod GNU GPL; tento port je rovněž pod
GPL (viz `LICENSE`). Herní data (WAD) součástí repozitáře nejsou – použijte
shareware `doom1.wad` nebo vlastní zakoupenou kopii hry.
