# Client PC Qt — compiler sur un nouveau PC

Le dossier `build/` n'est **pas** sur GitHub, et c'est voulu : il contient des
chemins absolus propres à chaque PC (ex. `C:/Users/<nom>/...`, version de Qt).
Qt Creator le **recrée automatiquement** à la première configuration.

## Prérequis
- Qt 6 (testé avec 6.12.0) avec le kit **MinGW 64-bit**, installé via le Qt Maintenance Tool.
- Qt Creator (CMake et MinGW sont fournis avec Qt).

## Ouvrir et lancer le projet (Qt Creator)
1. `git pull`
2. Qt Creator → *Fichier → Ouvrir un fichier ou un projet* →
   choisir **`pc-Qt/client_test/CMakeLists.txt`** (pas un dossier `build`).
3. Page *Configure Project* : cocher un kit **Desktop Qt 6.x MinGW 64-bit**,
   puis cliquer sur **Configure Project**. Le dossier `build/` est créé.
4. Ctrl+R (Run). Choisir la cible **client_test** (pas `udp_cli`) en bas à gauche.

Après un pull qui ajoute/renomme des fichiers : *Build → Run CMake*.

## En cas d'erreur
- *« The current CMakeCache.txt directory ... is different »* : le dossier
  `build/` vient d'un autre PC (copie par clé USB, zip...). Le supprimer et
  reconfigurer (étape 3).
- *« No valid kits found »* : installer un kit Qt MinGW 64-bit avec le
  Maintenance Tool, puis *Edit → Preferences → Kits*.
- L'exe lancé hors de Qt Creator ne démarre pas (DLL Qt manquantes) :
  `windeployqt client_test.exe` dans le dossier de l'exe.

## Ligne de commande (sans Qt Creator)
```
cmake -S pc-Qt/client_test -B pc-Qt/client_test/build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
cmake --build pc-Qt/client_test/build
```
(`C:\Qt\Tools\mingw1310_64\bin` et `C:\Qt\Tools\CMake_64\bin` doivent être dans le PATH.)

## IHM3
`IHM3/` est un projet **qmake** : ouvrir `IHM3/IHM3.pro`, choisir le même kit, Run.
